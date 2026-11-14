from pathlib import Path
import json, hashlib, difflib, re

r=Path.cwd(); w=r/'build/landing-native-animation'; old=r/'build/landing-collar-eye'
repo=Path('C:/dev/TombRaider123VR'); stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v): (w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
def function(s,name):
    start=s.index(name); end=s.index('{',start); depth=1; end+=1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}'); end+=1
    return s[start:end]

checks={}
for id,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks')]:
    match=re.search(r'PASS: (\d+) '+label,read(w/file)); assert match,(id,read(w/file)[-500:])
    checks[id]=int(match[1])
for id in ('456','123'): assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{id}.log')
assert 'PASS: 4217568 landing/standing checks' in read(w/'units423.log')
assert 'FAIL: native impact suspends cached body and torso inference' in read(w/'baseline-regression.log')
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    if n!='src/FirstPerson.cpp': assert sha(r/n)==h,n
assert sha(w/'before/src/FirstPerson.cpp')==previous['productionHashes']['src/FirstPerson.cpp']
assert function(read(w/'before/src/FirstPerson.cpp'),'bool Anchor(')==function(read(r/'src/FirstPerson.cpp'),'bool Anchor(')
assert function(read(repo/'src/FirstPerson.cpp'),'bool Anchor(')==function(read(stage/'src/FirstPerson.cpp'),'bool Anchor(')
originals=json.loads(read(w/'port-original-hashes.json'))
manifest=[]; diff=[]
for n,h in originals.items():
    assert sha(repo/n)==h,n
    manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
    diff+=list(difflib.unified_diff(read(repo/n).splitlines(True),read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/FirstPersonBodyIK.h','src/BodyFollower.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','src/GroundTorso.h','src/TorsoClearance.h']:
    assert sha(stage/n)==sha(repo/n),n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
diff=[]
for n in ['src/FirstPerson.cpp','tools/landing_support_regression.inl','README.md']:
    diff+=list(difflib.unified_diff(read(w/'before'/n).splitlines(True),read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
save('validation.json',dict(
    change='Suspend full-body/arm IK and tracked weapons throughout native fall impact/recovery. Render the native joint palette at calibrated uniform size; reset transient IK state on entry and exit without recalibrating height.',
    scope='TR1-5 first-person fall landing clips already selected by UseLandingCamera; ordinary quick jump recovery unchanged',checks=checks,inHeadsetTested=False,
    regressionEvidence='Installed preimage fails the native-impact suspension assertion. Candidate preserves every native body/arm joint angle and position at the selected size through all extracted impact/recovery keys, then resumes the standing solver. Render hooks also preserve the native pose.',
    preserved=['Exact prior camera Anchor implementation in both collections','Standing height and body-size calibration','Raw HMD tracking and physical movement input gates','Native animation palette for chest physics','Roomscale/glance inference implementation','Native collision, damage and animation timing','Existing ordinary jump behavior'],
    compromise='Visible hands and guns follow the canned animation instead of controller targets until recovery finishes. Transient pose state is re-seeded rather than interpolating a stale kneeling IK pose. In-headset handoff confirmation remains necessary.',
    coverage='TR1-5 extracted impact/recovery and standing; native joint positions and rotations, measured/missing torso surface, automatic/manual scale, armed/unarmed, physical head lowering and downward gaze; actual render hooks; unchanged collar camera clearance; 4217568 integrated checks at 423 units/metre.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in previous['productionHashes']},
    testHashes={n:sha(r/n) for n in previous['testHashes']}))
s=read(old/'deploy.ps1').replace('landing-collar-eye','landing-native-animation')
s=s.replace('7337434 straight-up',str(checks['456'])+' straight-up').replace('4062678 locomotion',str(checks['123'])+' locomotion')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded native-animation landing builds:',checks)
