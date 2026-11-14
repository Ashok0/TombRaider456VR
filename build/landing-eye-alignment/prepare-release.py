from pathlib import Path
import json, hashlib, difflib, re

r=Path.cwd(); w=r/'build/landing-eye-alignment'; old=r/'build/landing-leg-history'
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
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    if n!='src/FirstPerson.cpp': assert sha(r/n)==h,n
expected={v['Relative']:v['ModifiedHash'] for v in json.loads(read(old/'source123-manifest.json'))}
manifest=[]; diff=[]
for n in ['src/FirstPerson.cpp','tests/landing_support_selftest.h','README.md']:
    assert sha(repo/n)==expected[n],n
    manifest.append(dict(Relative=n,OriginalHash=sha(repo/n),ModifiedHash=sha(stage/n),Deleted=False))
    diff+=list(difflib.unified_diff(read(repo/n).splitlines(True),read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/FirstPersonBodyIK.h','src/BodyFollower.h']:
    assert sha(stage/n)==sha(repo/n)==expected[n],n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
save('validation.json',dict(
    change='During native fall landings, cap only the downward animated eye offset at the head joint plus configured upward eye offset; preserve native horizontal anchor and live HMD tracking.',
    scope='TR1-5 first-person head joint, movement stabilization enabled',checks=checks,inHeadsetTested=False,
    cause='The 140-unit forward camera offset rotates downward with native impact head pitch; extracted TR5 keys place it up to 107.76 units below the head joint before physical tracking.',
    preserved=['Successful supported landing hip/knee solver (source hashes unchanged)','Standing calibration','Native horizontal landing camera movement','Full physical HMD translation','Tracked torso and arms','Roomscale/glance inference','Native collision, damage and animation timing'],
    cameraCoverage='Native TR4/5 landing keys, root interpolation, world headings, manual scales, calibration on/off, full-body IK on/off, movement stabilization on/off, physical head lowering/rise and translation-disabled modes; actual HeadView translation verified.',
    bodyCoverage='Native TR1-5 impact palettes driven by the corrected production camera, armed/unarmed, automatic and manual scale, 0/12/30 cm physical lowering; foot contacts, supported knees, rigid legs and exact controller wrists remain valid.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in previous['productionHashes']},
    testHashes={n:sha(r/n) for n in previous['testHashes']}))
s=read(old/'deploy.ps1').replace('landing-leg-history','landing-eye-alignment')
s=s.replace('6753634 straight-up',str(checks['456'])+' straight-up').replace('3843862 locomotion',str(checks['123'])+' locomotion').replace('6117120 landing camera',str(checks['camera'])+' landing camera')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded landing eye correction:',checks)
