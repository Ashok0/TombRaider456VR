from pathlib import Path
import json, hashlib, difflib, re

r=Path.cwd(); w=r/'build/landing-leg-history'; repo=Path('C:/dev/TombRaider123VR'); stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(name,value): (w/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

checks={}
for id,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks')]:
    found=re.search(r'PASS: (\d+) '+label,read(w/file)); assert found,(id,read(w/file)[-500:])
    checks[id]=int(found[1])
for id in ('456','123'): assert 'TombRaiderVR.vcxproj ->' in read(w/f'build-{id}.log')
assert 'FAIL: landing knee cannot fold below authored kneeling support' in read(w/'baseline-regression.log')
previous=json.loads(read(r/'build/restore-landing-20261105/validation.json'))
for n in ['src/FirstPersonStabilization.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h']:
    assert sha(r/n)==previous['productionHashes'][n],n
def function(text,signature):
    start=text.index(signature); opening=text.index('{',start); depth=1; end=opening+1
    while depth:
        if text[end]=='{': depth+=1
        elif text[end]=='}': depth-=1
        end+=1
    return text[start:end]
assert function(read(r/'src/FirstPerson.cpp'),'bool Anchor(')==function(read(w/'before/src/FirstPerson.cpp'),'bool Anchor(')
assert read(r/'src/GroundTorso.h')==read(w/'before/src/GroundTorso.h')

originals=json.loads(read(w/'port-original-hashes.json'))
for n,h in originals.items(): assert sha(repo/n)==h,n
manifest=[]; diff=[]
for n in [*originals,'README.md','tests/collection_integration_selftest.h','tests/landing_support_checks.inl','tests/landing_support_selftest.h']:
    old=repo/n; new=stage/n
    assert new.exists(),n
    if old.exists() and sha(old)==sha(new): continue
    manifest.append(dict(Relative=n,OriginalHash=sha(old) if old.exists() else None,ModifiedHash=sha(new),Deleted=False))
    diff+=list(difflib.unified_diff(read(old).splitlines(True) if old.exists() else [],read(new).splitlines(True),fromfile=n,tofile=n))
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(r/'build/restore-landing-20261105/releases.json'))
for v in releases:
    v['Previous']=v['Expected']; v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll')
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
    v['RenderChecks']=checks[v['Id']]
save('releases.json',releases)
save('validation.json',dict(
    change='Bound additional landing hip compression and lean to the authored supported pose; use supported native knee poles in both full-body solve and torso reposing',
    scope='TR1-5 first-person native fall landings',checks=checks,inHeadsetTested=False,
    regressionEvidence='The same landing support test fails against the installed restoration preimages at knee support and passes with this correction.',
    cause='Standing eye reserve and subsequent standing torso fitting also affected native impact crouches; deep compression and extra tracked lean could exceed leg support and the generic forward knee pole could fold knees below the native support.',
    preserved=['Native animated camera dip/rise (Anchor source unchanged)','Full-body torso and exact tracked wrist solve','Current standing calibration','Roomscale/glance classifiers','Native collision, damage, animation and physics palette'],
    poseCoverage='Extracted TR1-5 impact keys plus standing entry/exit; automatic .845 scale with standing eye reserve, manual .7/1/1.2; physical height lowering 0/12/30 cm; armed/unarmed; foot contact height, knee support, rigid segments and controller wrist checks',
    broadSuiteLimitation=previous['broadSuiteLimitation'],
    productionHashes={n:sha(r/n) for n in [*previous['productionHashes'],'src/GroundTorso.h']},
    testHashes={n:sha(r/n) for n in [*previous['testHashes'],'tools/landing_support_checks.inl','tools/landing_support_regression.inl','tools/native_landing_poses.inl']}))
s=read(r/'build/restore-landing-20261105/deploy.ps1').replace('restore-landing-20261105','landing-leg-history')
s=s.replace('6637210 straight-up',str(checks['456'])+' straight-up').replace('3738838 locomotion',str(checks['123'])+' locomotion')
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared guarded landing-support release:',checks)
