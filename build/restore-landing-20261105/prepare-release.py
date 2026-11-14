from pathlib import Path
import json,hashlib,difflib,re
r=Path.cwd();w=r/'build/restore-landing-20261105';repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def read(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,o):(w/n).write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8')
checks={}
for id,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks')]:
 text=read(w/file);found=re.search(r'PASS: (\d+) '+label,text);assert found,(id,text[-300:]);checks[id]=int(found[1])
for id in ('456','123'):assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{id}.log')
previous=json.loads(read(r/'build/landing-final-eye/validation.json'))
for n in ['src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','src/BodyFollower.h','src/FirstPersonBodyIK.h']:
 assert sha(r/n)==previous['productionHashes'][n],n
expected={}
for folder in ['landing-floor-fit','landing-camera-height','landing-head-follow','landing-final-eye']:
 for v in json.loads(read(r/'build'/folder/'source123-manifest.json')):expected[v['Relative']]=v['ModifiedHash']
manifest=[];diff=[]
for n in ['src/FirstPerson.cpp','src/FirstPersonStabilization.h','README.md','tests/collection_integration_selftest.h','tests/firstperson_ik_selftest.h','tests/remaining_parity_selftest.h']:
 old=repo/n;new=stage/n;assert sha(old)==expected[n],f'Unexpected external source edits: {n}'
 if sha(old)==sha(new):continue
 manifest.append(dict(Relative=n,OriginalHash=sha(old),ModifiedHash=sha(new),Deleted=False))
 diff+=list(difflib.unified_diff(read(old).splitlines(True),read(new).splitlines(True),fromfile=n,tofile=n))
save('source123-manifest.json',manifest);(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(r/'build/landing-final-eye/releases.json'))
for v in releases:
 v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll');assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous'];v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
save('validation.json',dict(change='Restore pre-November-10 full-body IK landing and native animated camera, using November 5 camera source and physical posture behavior',scope='TR1-5 first-person native fall landings',checks=checks,inHeadsetTested=False,
 removed=['Native-only scaled landing skeleton with separate arm fallback','Standing camera hold on impact','Scaled/yaw-only landing head anchor','Downward tracked-height cap on landing'],
 preserved=['Current roomscale and glance classifiers','Current standing height calibration','Controls and menus','Native collision, damage and animation timing'],
 knownTradeoff='The earlier temporary foot/lower-leg clipping on hard impacts may return. This is a restoration of preferred presentation, not a new floor-clipping solution.',
 cameraCoverage='Extracted TR4/5 landing poses, calibrated/uncalibrated, full-body IK and stabilization on/off, scales/headings/interpolation, disabled tracking modes, physical head lowering and rise; final rendered eye follows native animation plus enabled tracking',
 broadSuiteLimitation=previous['broadSuiteLimitation'],retiredTests='The floor-fit-only palette assertions describe the rejected native-only landing path and are no longer included in the active suites; original body/arm integration suites are retained.',
 productionHashes={n:sha(r/n) for n in previous['productionHashes']},testHashes={n:sha(r/n) for n in ['tools/first_person_tests.cpp','tools/landing_camera_test.cpp','tools/upjump_arms_test.cpp']}))
s=read(r/'build/landing-final-eye/deploy.ps1').replace('landing-final-eye','restore-landing-20261105').replace('6905722 straight-up',str(checks['456'])+' straight-up').replace('3969996 locomotion',str(checks['123'])+' locomotion').replace('7136640 landing camera',str(checks['camera'])+' landing camera')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig');print('Prepared guarded landing restoration:',checks)
