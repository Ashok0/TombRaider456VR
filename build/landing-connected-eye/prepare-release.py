from pathlib import Path
import json, hashlib, difflib, re

r=Path.cwd(); w=r/'build/landing-connected-eye'; old=r/'build/landing-eye-alignment'
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
assert 'PASS: 333120 landing/standing checks' in read(w/'units423.log')
assert 'FAIL: final rendered eye never dips below the neck attached to the supported torso' in read(w/'baseline-regression.log')
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    if n not in ('src/FirstPerson.cpp','src/FirstPersonBodyIK.h'): assert sha(r/n)==h,n
for n in ['src/FirstPerson.cpp','src/FirstPersonBodyIK.h']:
    assert sha(w/'before'/n)==previous['productionHashes'][n],n
before=read(w/'before/src/FirstPersonBodyIK.h')
after=read(r/'src/FirstPersonBodyIK.h')
assert after==before.replace('    const Basis bend=LimitRotation(Align(spine,Add(spine,lean)),.34906585f);',
    '    // The impact animation already supplies its torso bend. Re-inferring it\n'
    '    // from the falling eye adds a second bend and separates camera from neck.\n'
    '    const Basis bend=authoredKnees ? IdentityBasis() : LimitRotation(Align(spine,Add(spine,lean)),.34906585f);')
originals=json.loads(read(w/'port-original-hashes.json'))
manifest=[]; diff=[]
for n,h in originals.items():
    assert sha(repo/n)==h,n
    manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
    diff+=list(difflib.unified_diff(read(repo/n).splitlines(True),read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/BodyFollower.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','src/GroundTorso.h']:
    assert sha(stage/n)==sha(repo/n),n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
save('validation.json',dict(
    change='Share the landing camera eye reference with body posture reconstruction; keep authored impact torso bend; bound final tracked eye by the scaled foot-supported landing pose.',
    scope='TR1-5 first-person native fall landings',checks=checks,inHeadsetTested=False,
    regressionEvidence='New final-eye-versus-connected-neck assertion fails against the exact installed preimages and passes against the candidate. The separately camera-driven head joint is deliberately not used as the reference.',
    causes=['Camera-only correction left the original pitched-forward eye offset in the body solver.', 'Additional inferred torso lean moved the neck away from the authored impact reference.', 'Physical headset lowering was still added below a landing pose whose supported pelvis could no longer follow it.'],
    preserved=['Supported leg reach, knee direction, foot contact and limb-length constraints (unchanged code)','Standing calibration','Native horizontal landing camera movement','Raw HMD pose and orientation','Tracked arms and native chest physics input','Roomscale/glance inference','Native collision, damage and animation timing'],
    compromise='During the native impact only, retain its authored torso bend and absorb downward headset translation in the scene anchor when it exceeds the supported pose depth. Native dip/rise remains; no standing-height hold or render-frame feedback is introduced.',
    coverage='Extracted TR1-5 impact palettes; automatic/manual scale; 0/12/15/30 cm physical lowering; armed/unarmed; downward HMD gaze in TR4/5; final eye relative to torso-attached neck, fixed foot contacts, supported knees, rigid legs, live wrists, recovery to standing; independent camera interpolation/heading/tracking-mode tests. An additional 333120 integrated checks pass at the user\'s actual 423 world units/metre.',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in previous['productionHashes']},
    testHashes={n:sha(r/n) for n in previous['testHashes']}))
s=read(old/'deploy.ps1').replace('landing-eye-alignment','landing-connected-eye')
s=s.replace('6757882 straight-up',str(checks['456'])+' straight-up').replace('3851158 locomotion',str(checks['123'])+' locomotion')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded connected landing eye correction:',checks)
