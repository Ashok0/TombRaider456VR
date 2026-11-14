from pathlib import Path
import re,json,hashlib,difflib
r=Path.cwd();w=r/'build/landing-final-eye';repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def read(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,o):(w/n).write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8')
checks=[('test-arms-456.log','PASS: 6905722 straight-up jump arm checks'),('test-123.log','PASS: 3969996 locomotion, gun-control and camera hook checks'),('targeted-camera.log','PASS: 7136640 landing camera checks')]
for n,success in checks:assert success in read(w/n),n
assert 'FAIL: rendered eye must not descend below the landing head eye after tracking is added' in read(w/'baseline-camera.log')
for n in ['456','123']:assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{n}.log')
previous=json.loads(read(r/'build/landing-head-follow/validation.json'))
for n in ['src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','src/BodyFollower.h','src/FirstPersonBodyIK.h']:
 assert sha(r/n)==previous['productionHashes'][n],n
manifest=[];diff=[]
for name in ['src/FirstPerson.cpp','src/FirstPersonStabilization.h','README.md','tests/firstperson_ik_selftest.h','tests/landing_floor_selftest.h']:
 old=repo/name;new=stage/name
 if sha(old)==sha(new):continue
 manifest.append(dict(Relative=name,OriginalHash=sha(old),ModifiedHash=sha(new),Deleted=False))
 diff+=list(difflib.unified_diff(read(old).splitlines(True),read(new).splitlines(True),fromfile=name,tofile=name))
save('source123-manifest.json',manifest);(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(r/'build/landing-head-follow/releases.json'))
for v in releases:
 v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll');assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
 v['RenderChecks']=6905722 if v['Id']=='456' else 3969996
save('releases.json',releases)
drops=[]
for line in read(w/'session.log').splitlines():
 if line.startswith('fp-eye:') and 'anim=24 ' in line:
  h=re.search(r'hmd=\(([^)]+)\) neutral=\(([^)]+)\)',line)
  if h:drops.append(float(h[2].split(',')[1])-float(h[1].split(',')[1]))
save('evidence.json',dict(hardLandingLogSamples=len(drops),physicalLoweringRangeMetres=[min(drops),max(drops)],bodyScale=.845,anchorY=-32,worldUnitsPerMetre=423,nominalEyeAboveHeadMetres=32*.845/423,baselineTestFails=True))
save('validation.json',dict(change='Compensate downward physical headset displacement in the landing scene anchor so the final tracked eye cannot descend beneath the animated landing eye',scope='TR1-5 stabilized first-person native fall landings',targetedCameraChecks=7136640,tr456RenderChecks=6905722,tr123Checks=3969996,inHeadsetTested=False,
 cameraCoverage=['Actual rendered HeadView displacement after Anchor','Native TR4/5 landing clips','Five interpolation fractions','Three scales and headings','Full body IK/stabilization on/off','Calibration present/absent','Neutral, 17cm and 40cm physical lowering, 20cm physical rise','Positional tracking disabled, first-person translation disabled, and head-at-camera mode','TR1-3 native landing hook tests with physical lowering and rise'],
 baseline='Installed implementation fails the new rendered-eye-height assertion; corrected implementation passes',broadSuiteLimitation=previous['broadSuiteLimitation'],sliding='Unchanged; this correction is limited to landing camera height',
 productionHashes={n:sha(r/n) for n in previous['productionHashes']},testHashes={n:sha(r/n) for n in previous['testHashes']}))
s=read(r/'build/landing-head-follow/deploy.ps1').replace('landing-head-follow','landing-final-eye').replace('3969766 locomotion','3969996 locomotion').replace('637200 landing camera','7136640 landing camera')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared verified landing final-eye deployment.')
