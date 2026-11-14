from pathlib import Path
import hashlib,json,difflib
root=Path('C:/dev/TombRaider456VR');work=root/'build/landing-camera-height';repo=Path('C:/dev/TombRaider123VR');stage=work/'123-work'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in [b'\xff\xfe',b'\xfe\xff'] else 'utf-8-sig').replace('\r\n','\n')
def save(name,obj):(work/name).write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8')
for file,success in [('test-arms-456.log','PASS: 6905722 straight-up jump arm checks'),('test-123.log','PASS: 3969766 locomotion, gun-control and camera hook checks'),('targeted-camera.log','PASS: 147120 landing camera checks')]:
 assert success in read(work/file),file
names=['src/FirstPerson.cpp','README.md','tests/remaining_parity_selftest.h','tests/firstperson_ik_selftest.h','tests/landing_floor_selftest.h']
manifest=[];patch=[]
for name in names:
 old=repo/name;new=stage/name
 manifest.append(dict(Relative=name,OriginalHash=sha(old),ModifiedHash=sha(new),Deleted=False))
 patch.extend(difflib.unified_diff(read(old).splitlines(True),read(new).splitlines(True),fromfile=name,tofile=name))
save('source123-manifest.json',manifest);(work/'source123-review.diff').write_text(''.join(patch),encoding='utf-8')
releases=json.loads(read(root/'build/landing-floor-fit/releases.json'))
for release in releases:
 release['Previous']=release['Expected'];release['Expected']=sha(work/release['Id']/'bin/TombRaiderVR.dll')
 assert sha(Path(release['Directory'])/'TombRaiderVR.dll')==release['Previous']
save('releases.json',releases)
save('validation.json',{
 'change':'Keep calibrated collision-relative eye anchor through native fall-landing compression and recovery, matching the airborne camera',
 'scope':'TR1-5 first person with movement stabilization and an existing standing calibration',
 'preserved':['Native falling collision-root travel','Physical HMD translation','Native landing mesh fix and tracked wrists','Standing calibration','Damage, animation timing and third-person behavior'],
 'targetedCameraChecks':147120,'tr456RenderChecks':6905722,'tr123Checks':3969766,
 'cameraCoverage':['TR4 and TR5','Full body IK on/off','Stabilization on/off','Calibrated/uncalibrated','Landing clips 24/31/82/92/99','Three headings','Physical head lowering','Five interpolation fractions','Neck compression through recovery'],
 'baselineTargeted':'Previous source fails: calibrated landing ignores neck compression while following interpolated collision height',
 'broadCameraSuiteLimitation':'The older broad first_person_tests executable stops at calibrated HMD height can request native crouch. The same assertion fails in an isolated copy of the pre-change source. This unrelated existing failure is not counted as a passing suite; dedicated landing-camera tests pass.',
 'renderCoverage':'TR1-5 extracted native landing palettes now rendered with the stable standing camera, preserving calibrated native leg transforms and exact tracked wrists',
 'build':'Both x64 Release builds passed; automatic deployment disabled',
 'productionHashes':{name:sha(root/name) for name in ['src/FirstPerson.cpp','src/FirstPersonBodyIK.h','src/BodyFollower.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h']},
 'testHashes':{name:sha(root/name) for name in ['tools/first_person_tests.cpp','tools/landing_camera_test.cpp','tools/landing_floor_regression.inl','tools/upjump_arms_test.cpp']},
 'inHeadsetTested':False
})
s=read(root/'build/landing-floor-fit/deploy.ps1').replace('landing-floor-fit','landing-camera-height')
s=s.replace("$repo123='C:\\dev\\TombRaider123VR'", "if(!(Select-String -LiteralPath (Join-Path $PSScriptRoot 'targeted-camera.log') -SimpleMatch 'PASS: 147120 landing camera checks' -Quiet)) { throw 'Landing camera tests have not passed.' }\n$repo123='C:\\dev\\TombRaider123VR'")
(work/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared checked releases, source diff, validation and guarded installation.')
