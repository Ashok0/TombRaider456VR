from pathlib import Path
import json, hashlib, difflib, re

r=Path.cwd(); w=r/'build/landing-collar-eye'; old=r/'build/landing-live-audit'
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
for name in ['units423.log','focused.log']:
    assert 'PASS: 700224 landing/standing checks' in read(w/name)
assert 'FAIL: impact eye keeps standing clearance above the visible collar, not only the neck pivot' in read(w/'baseline-regression.log')
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    if n!='src/FirstPerson.cpp': assert sha(r/n)==h,n
assert sha(w/'before/src/FirstPerson.cpp')==previous['productionHashes']['src/FirstPerson.cpp']
assert sha(w/'before/src/TorsoClearance.h')==sha(repo/'src/TorsoClearance.h')
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
diff=[]
for n in ['src/FirstPerson.cpp','src/TorsoClearance.h','tools/landing_support_regression.inl','tools/landing_camera_test.cpp','README.md']:
    diff+=list(difflib.unified_diff(read(w/'before'/n).splitlines(True),read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
production={n:sha(r/n) for n in previous['productionHashes']}
production['src/TorsoClearance.h']=sha(r/'src/TorsoClearance.h')
save('validation.json',dict(
    change='Apply the standing fit\'s 25 cm clearance above the animated measured torso surface to the landing camera and body reference. Preserve the existing neck/automatic-fit reserve as a fallback.',
    scope='TR1-5 stabilized first-person full-body native fall landings',checks=checks,inHeadsetTested=False,
    regressionEvidence='The new rendered-mesh clearance assertion fails against the exact installed source and passes against the candidate, including at 423 units/metre. Foot contacts, supported knees, rigid limbs and tracked wrists remain valid.',
    observedSession='TR5 session reports auto scale 0.911, reserve 12.86 cm, approximately 19.6 cm eye-to-neck separation. Extracted deepest impact poses and recorded torso bounds imply only approximately 5 cm eye-to-visible-collar clearance despite the neck check passing. The candidate raises that limit to 25 cm, the existing standing-fit clearance.',
    preserved=['Existing leg support solver unchanged','Standing calibration unchanged','Native horizontal camera movement','Raw HMD tracking','Native animation palette for chest physics','Roomscale/glance inference','Native collision, damage and animation timing'],
    limitations='Torso bounds conservatively enclose the mesh and are not a measurement of facial eye vertices. Missing or stale outfit bounds use the previous fallback. In-headset confirmation remains necessary.',
    coverage='TR1-5 extracted impacts and standing recovery; measured/missing surface; automatic/manual scales; armed/unarmed; physical lowering and downward gaze. Independent camera tests cover stale outfit owners, stabilization disabled, full-body disabled, tracking modes, yaw and interpolation. Actual-scale integrated suite: 700224 checks.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes=production,
    testHashes={n:sha(r/n) for n in previous['testHashes']}))
s=read(old/'deploy.ps1').replace('landing-live-audit','landing-collar-eye')
s=s.replace('6981658 straight-up',str(checks['456'])+' straight-up').replace('3898326 locomotion',str(checks['123'])+' locomotion').replace('6626880 landing camera',str(checks['camera'])+' landing camera')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded measured-collar landing correction:',checks)
