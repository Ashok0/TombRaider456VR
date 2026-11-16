from pathlib import Path
import json, hashlib, difflib, re
r=Path.cwd(); w=r/'build/glance-turn-entry'; old=r/'build/ladder-exit-facing'
repo=Path('C:/dev/TombRaider123VR'); stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v): (w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
checks={}
for id,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks'),('inspection','existing-inspection.log','inspection/turn/walking checks'),('sustained','lowered-inspection.log','sustained-inspection checks')]:
    m=re.search(r'PASS: (\d+) '+label,read(w/file));assert m,(id,read(w/file)[-500:]);checks[id]=int(m[1])
assert checks['inspection']==11320 and checks['sustained']==4138
for id in ['456','123']:assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{id}.log')
assert 'FAIL: repeated stationary glances cannot become a body turn after several seconds' in read(w/'baseline-inspection.log')
assert 'Replayed 6335 poses; 0 mismatches' in read(w/'replay.log')
assert 'Replayed 1163 poses; 0 mismatches' in read(w/'test-replay.log')
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
for n in changed+['src/ControllerBodyAnchor.inl','tools/body_anchor_replay.cpp','tools/close_hands_motion_regression.inl','README.md']:
    diff+=list(difflib.unified_diff(read(w/'before'/n).splitlines(True),read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
testHashes={n:sha(r/n) for n in previous['testHashes']};testHashes.update({n:sha(r/n) for n in ['tools/inspection_heading_tests.cpp','tools/lowered_inspection_tests.cpp','tools/close_hands_motion_regression.inl','tools/body_anchor_replay.cpp']})
save('validation.json',dict(
    change='Sustained body-facing hold for deeply lowered downward inspection, including correlated head/hand motion; independent shared straight travel releases the hold.',
    scope='Shared TR1-5 controller body-facing inference; native ladder, landing animation and camera code unchanged',checks=checks,inHeadsetTested=False,
    regressionEvidence='Latest installed-code session replays 6335 poses with no mismatches. Full stationary sequence of 1163 poses fails under baseline and passes candidate. False turn has gaze Y -0.908, raw physical lowering 0.242m and closely correlated head/controller turn, defeating the prior head-leading gate. Existing recorded walking/turning regression passes with actual calibrated physical height. New log format roundtrips all 1163 samples.',
    preserved=['Native landing animation/VRIK handoff','Standing height and body-size calibration','Camera clearance implementation','Existing translation pivot detector defaults','Roomscale walking in all four directions','Coordinated physical turns with level/downward gaze','Native simulation and ladder animation'],
    compromise='Deep lowered inspection holds facing without timed expiry or center-gaze release. Looking up, straightening, movement stick or independent shared straight physical travel release it. A real turn begun while bent over inspecting may need looking up/straightening first; feet are not tracked.',
    coverage='1163 samples of latest stationary glance, 30 seconds repeated synthetic glances, earlier 181-sample post-ladder glance, existing labelled close-hand physical walking/turning with calibrated height, level/downward turns at 60/90/120 Hz, four-direction shared translation, targeted mod and camera regressions.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in list(previous['productionHashes'])+['src/ControllerBodyAnchor.inl']},testHashes=testHashes))
s=read(old/'deploy.ps1').replace('ladder-exit-facing','glance-turn-entry')
s=s.replace('10854778 straight-up',str(checks['456'])+' straight-up').replace('5416918 locomotion',str(checks['123'])+' locomotion')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded planted-inspection facing correction:',checks)
