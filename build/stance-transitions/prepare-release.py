from pathlib import Path
import json,hashlib,difflib,re
r=Path.cwd();w=r/'build/stance-transitions';old=r/'build/glance-turn-entry';repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def read(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v):(w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
checks={}
for name,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks'),('inspection','inspection.log','inspection/turn/walking checks'),('sustained','lowered-inspection.log','sustained-inspection checks'),('continuation','continuation.log','turn-continuation checks'),('swivel','swivel.log','swivel/walking checks'),('anchor','anchor.log','anchor checks')]:
 s=read(w/file);m=re.search(r'PASS: (\d+) '+label,s);assert m and 'FAIL:' not in s,(name,s[-400:]);checks[name]=int(m[1])
assert 'Replayed 8093 poses; 0 mismatches' in read(w/'planted-session.log')
assert 'turn samples 0, movement samples 0' in read(w/'planted-session.log')
assert 'Replayed 8093 poses; 7104 mismatches' in read(w/'planted-baseline.log')
assert 'FAIL: hands alone cannot indefinitely renew turn intent' in read(w/'continuation-baseline.log')
for id in ['456','123']:
 s=read(w/f'build-{id}.log');assert 'TombRaiderVR.vcxproj ->' in s and ' error ' not in s
previous=json.loads(read(old/'validation.json'))
changed=['src/ControllerBodyAnchor.h','src/BodyFollower.h']
for n,h in previous['productionHashes'].items():
 if n in changed:assert sha(w/'before'/n)==h,n
 else:assert sha(r/n)==h,n
orig=json.loads(read(w/'port-original-hashes.json'));manifest=[];diff=[]
for n,h in orig.items():
 if h:assert sha(repo/n)==h,n
 else:assert not (repo/n).exists(),n
 manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
 diff+=list(difflib.unified_diff(read(repo/n).splitlines(True) if h else [],read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/FirstPerson.cpp','src/FirstPersonBodyIK.h','src/GroundTorso.h','src/TorsoClearance.h','src/ControllerBodyAnchor.inl']:
 assert sha(stage/n)==sha(repo/n),n
# BodyFollower contains collection-specific crouch helpers: port only this class.
a=read(repo/'src/BodyFollower.h');b=read(stage/'src/BodyFollower.h');c=read(r/'src/BodyFollower.h')
start='struct RoomscaleHeadingRecovery {';end='struct BodyFollower {'
assert a[:a.index(start)]==b[:b.index(start)] and a[a.index(end):]==b[b.index(end):]
assert b[b.index(start):b.index(end)]==c[c.index(start):c.index(end)]
for n in ['src/ControllerBodyAnchor.h','src/InspectionStepEvidence.h']:assert sha(r/n)==sha(stage/n),n
save('source123-manifest.json',manifest);(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
diff=[]
for n in changed+['src/InspectionStepEvidence.h','tools/lowered_inspection_tests.cpp','README.md']:
 before=w/'before'/n;diff+=list(difflib.unified_diff(read(before).splitlines(True) if before.exists() else [],read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
 v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll');v['RenderChecks']=checks[v['Id']]
 assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
save('releases.json',releases)
tests=list(previous['testHashes'])+['tools/turn_continuation_tests.cpp','tools/planted_session_tests.cpp']
save('validation.json',dict(change='Apply planted-inspection confidence to both position and yaw; require sustained shared steps to release it; reject unstable close-hand angular evidence and hand-only turn continuation.',checks=checks,inHeadsetTested=False,stationarySamples=8093,stationaryTranslationSamples=0,stationaryTurnSamples=0,baselineTranslationSamples=1434,baselineTurnSamples=1880,regressionEvidence='User explicitly labelled all three behaviors in the entire recording as feet planted. Replay isolates physical inference from stick/artificial input, keeps each segment anchor fixed, and tests active yaw targets as well as motion flags. Recorded true walking/turning and broad existing movement tests remain required.',preserved=['Landing animation/VRIK handoff','Standing height/calibration','Camera/torso clearance','Collection-specific crouch behavior'],compromise='Deep inspection requires returning upright or a corroborated shared step before inferred body movement resumes. Closely spaced hands (<12 cm horizontally) additionally require midpoint/head agreement for yaw. Clear straight steps qualify after 3 cm with <1 mm mismatch; arm sway needs 6 cm within 0.2-0.6 s. Feet remain inferred, not tracked.',broadSuiteLimitation=previous['broadSuiteLimitation'],productionHashes={n:sha(r/n) for n in list(previous['productionHashes'])+['src/InspectionStepEvidence.h']},testHashes={n:sha(r/n) for n in tests},captureSHA256=sha(w/'session.log')))
s=read(old/'deploy.ps1').replace('glance-turn-entry','stance-transitions');(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared reviewed stance-transition release:',checks)
