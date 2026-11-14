from pathlib import Path
import hashlib,json,difflib
r=Path.cwd();w=r/'build/landing-head-follow';repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def save(n,o):(w/n).write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8')
checks=[('test-arms-456.log','PASS: 6905722 straight-up jump arm checks'),('test-123.log','PASS: 3969766 locomotion, gun-control and camera hook checks'),('targeted-camera.log','PASS: 637200 landing camera checks')]
for n,s in checks:assert s in read(w/n),n
for n in ('456','123'):assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{n}.log')
assert sha(r/'src/PlantedPivotEvidence.h')=='9B5D53DBE76EDD4978714CC975B764E14BA216A5AAA599954D57BD95DC967835'
assert sha(r/'src/ControllerBodyAnchor.h')=='AA5928380F164C6B777C0B80AB72B80BBB01CF261F7990AFE295F84DDBDE8C86'
names=['src/FirstPerson.cpp','src/FirstPersonStabilization.h','README.md','tests/remaining_parity_selftest.h','tests/firstperson_ik_selftest.h','tests/landing_floor_selftest.h']
manifest=[];diff=[]
for name in names:
 old=repo/name;new=stage/name
 if sha(old)==sha(new):continue
 manifest.append(dict(Relative=name,OriginalHash=sha(old),ModifiedHash=sha(new),Deleted=False))
 diff+=list(difflib.unified_diff(read(old).splitlines(True),read(new).splitlines(True),fromfile=name,tofile=name))
save('source123-manifest.json',manifest);(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(r/'build/landing-camera-height/releases.json'))
for v in releases:
 v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll');assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
save('releases.json',releases)
save('validation.json',dict(change='Follow the scaled animated landing head using a yaw-only eye offset instead of holding standing height',scope='TR1-5 stabilized first-person fall landings',targetedCameraChecks=637200,tr456RenderChecks=6905722,tr123Checks=3969766,inHeadsetTested=False,
 cameraCoverage=['Native TR4/5 landing clips and recovery frames','Five interpolation fractions','Three body scales and headings','Full body IK and stabilization on/off','Standing calibration present/absent','Physical head lowering'],
 broadSuiteLimitation='The existing broad first_person_tests executable has an independently reproduced pre-change physical-crouch assertion failure. This is not counted as a passing suite.',
 sliding='NOT FIXED: production stance and body-following headers unchanged. Latest capture reproduces false steps and turns during user-labelled planted glances. Experimental raised-hand guard eliminates the captured glances but reduced combined walking/glancing travel and was rejected.',
 productionHashes={n:sha(r/n) for n in ['src/FirstPerson.cpp','src/FirstPersonStabilization.h','src/FirstPersonBodyIK.h','src/BodyFollower.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h']},
 testHashes={n:sha(r/n) for n in ['tools/first_person_tests.cpp','tools/landing_camera_test.cpp','tools/landing_floor_regression.inl','tools/upjump_arms_test.cpp']}))
s=read(r/'build/landing-camera-height/deploy.ps1').replace('landing-camera-height','landing-head-follow').replace('147120 landing camera checks','637200 landing camera checks')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded camera-only deployment. Experimental stance changes excluded.')
