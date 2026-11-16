from pathlib import Path
import json, hashlib, difflib, re
r=Path.cwd(); w=r/'build/ladder-exit-facing'; old=r/'build/landing-native-animation'
repo=Path('C:/dev/TombRaider123VR'); stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v): (w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
checks={}
for id,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks'),('inspection','inspection.log','inspection/turn/walking checks')]:
    m=re.search(r'PASS: (\d+) '+label,read(w/file));assert m,(id,read(w/file)[-500:]);checks[id]=int(m[1])
assert checks['inspection']==11320
for id in ['456','123']:assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{id}.log')
assert 'FAIL: captured downward glance after ladder cannot authorize a body turn' in read(w/'baseline-inspection.log')
assert 'Replayed 41179 poses; 0 mismatches' in read(w/'replay.log')
previous=json.loads(read(old/'validation.json'))
changed=['src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h']
for n,h in previous['productionHashes'].items():
    if n in changed:assert sha(w/'before'/n)==h,n
    else:assert sha(r/n)==h,n
originals=json.loads(read(w/'port-original-hashes.json'));manifest=[];diff=[]
for n,h in originals.items():
    assert sha(repo/n)==h,n
    manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
    diff+=list(difflib.unified_diff(read(repo/n).splitlines(True),read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/FirstPerson.cpp','src/FirstPersonBodyIK.h','src/BodyFollower.h','src/GroundTorso.h','src/TorsoClearance.h']:
    assert sha(stage/n)==sha(repo/n),n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
diff=[]
for n in changed+['README.md']:
    diff+=list(difflib.unified_diff(read(w/'before'/n).splitlines(True),read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
testHashes={n:sha(r/n) for n in previous['testHashes']};testHashes['tools/inspection_heading_tests.cpp']=sha(r/'tools/inspection_heading_tests.cpp')
save('validation.json',dict(
    change='Hold physical body yaw during a downward head-led inspection corroborated by planted-torso arc geometry, preventing later hand movement from admitting a turn from the same glance.',
    scope='Shared TR1-5 controller body-facing inference; native ladder, landing animation and camera code unchanged',checks=checks,inHeadsetTested=False,
    regressionEvidence='The full session replays exactly under installed code. Its captured post-ladder planted glance admits a false body turn in the baseline and does not in the candidate. Existing user-labelled close-hand walking/turning regression also passes after rejecting a broader gaze-only candidate.',
    preserved=['Native landing animation/VRIK handoff','Standing height and body-size calibration','Camera clearance implementation','Existing translation pivot detector defaults','Roomscale walking in all four directions','Coordinated physical turns with level/downward gaze','Native simulation and ladder animation'],
    compromise='Once a downward head-led inspection also has planted-torso arc evidence, physical facing is held until gaze rises or returns near body facing, or the movement stick is used. A genuine turn started during that held inspection may require raising/centering gaze first. No foot tracking is available.',
    coverage='181 captured post-ladder samples, including the offending turn; existing labelled close-hand glances, forward/back and side steps, established physical turns; synthetic coordinated turns at 60/90/120 Hz, both directions, varying hand extension and downward gaze; four-direction physical travel; existing full mod and camera regressions.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in previous['productionHashes']},testHashes=testHashes))
s=read(old/'deploy.ps1').replace('landing-native-animation','ladder-exit-facing')
s=s.replace('10854778 straight-up',str(checks['456'])+' straight-up').replace('5416918 locomotion',str(checks['123'])+' locomotion')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded planted-inspection facing correction:',checks)
