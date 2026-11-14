from pathlib import Path
import json, hashlib, difflib, re

r=Path.cwd(); w=r/'build/landing-live-audit'; old=r/'build/landing-connected-eye'
repo=Path('C:/dev/TombRaider123VR'); stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v): (w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')

checks={}
for id,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks')]:
    match=re.search(r'PASS: (\d+) '+label,read(w/file)); assert match,(id,read(w/file)[-500:])
    checks[id]=int(match[1])
for id in ('456','123'): assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{id}.log')
assert 'PASS: 344448 landing/standing checks' in read(w/'units423.log')
assert 'PASS: 344448 landing/standing checks' in read(w/'focused.log')
assert 'FAIL: landing retains calibrated eye-to-neck clearance instead of dropping to the neck base' in read(w/'baseline-regression.log')
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    if n!='src/FirstPerson.cpp': assert sha(r/n)==h,n
assert sha(w/'before/src/FirstPerson.cpp')==previous['productionHashes']['src/FirstPerson.cpp']
originals=json.loads(read(w/'port-original-hashes.json'))
manifest=[]; diff=[]
for n,h in originals.items():
    assert sha(repo/n)==h,n
    manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
    diff+=list(difflib.unified_diff(read(repo/n).splitlines(True),read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/FirstPersonBodyIK.h','src/BodyFollower.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','src/GroundTorso.h']:
    assert sha(stage/n)==sha(repo/n),n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
save('validation.json',dict(
    change='Retain automatic torso calibration eye clearance in both the supported landing camera and body eye reference. Capture landing diagnostics once per tracked pose.',
    scope='TR1-5 first-person native fall landings',checks=checks,inHeadsetTested=False,
    regressionEvidence='The calibrated-clearance assertion fails against the exact installed production preimage and passes against this candidate. The final eye is compared to the torso-attached neck, not the independently camera-driven head joint.',
    observedSession='World scale 423 units/metre, auto torso scale 0.845, fixed eye reserve 20.9 cm. Prior landing measurements put the eye only approximately 6 cm above the connected neck base. This proves lost calibrated clearance, but is not visual proof that it explains the entire reported leg-level view.',
    preserved=['Supported leg reach, knee direction, foot contact and limb-length constraints (unchanged code)','Standing calibration','Native horizontal landing camera movement','Raw HMD pose and orientation','Tracked arms and native chest physics input','Roomscale/glance inference','Native collision, damage and animation timing'],
    compromise='Only automatic fitted body scale adds its existing eye reserve during impact. Share it between camera and body reference instead of applying extra torso compression. Manual body scale behavior is unchanged.',
    coverage='Extracted TR1-5 impact palettes; automatic/manual scale; 0/12/15/30 cm physical lowering; armed/unarmed; downward HMD gaze in TR4/5; calibrated eye clearance relative to torso-attached neck, fixed foot contacts, supported knees, rigid legs, live wrists, recovery to standing; independent camera interpolation/heading/tracking-mode tests. Additional 344448 integrated checks pass at the recorded 423 world units/metre.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in previous['productionHashes']},
    testHashes={n:sha(r/n) for n in previous['testHashes']}))
s=read(old/'deploy.ps1').replace('landing-connected-eye','landing-live-audit')
s=s.replace('6970330 straight-up',str(checks['456'])+' straight-up').replace('3893462 locomotion',str(checks['123'])+' locomotion')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded calibrated-clearance landing correction:',checks)
