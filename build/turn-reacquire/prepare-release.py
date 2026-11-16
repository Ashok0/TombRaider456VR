from pathlib import Path
import hashlib,json,difflib,re
r=Path.cwd();w=r/'build/turn-reacquire';old=r/'build/connected-arms'
repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig',errors='replace').replace('\r\n','\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v):(w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
checks={}
for name,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks'),('turn','inspection-turn.log','inspection-turn recovery checks'),('anchor','anchor.log','anchor checks'),('swivel','swivel.log','swivel/walking checks'),('inspection','inspection.log','sustained-inspection checks'),('continuation','continuation.log','turn-continuation checks')]:
    s=read(w/file);m=re.search(r'PASS: (\d+) '+label,s)
    assert m and 'FAIL:' not in s,(name,s[-500:]);checks[name]=int(m[1])
for collection in ['456','123']:
    s=read(w/f'build-{collection}.log')
    assert 'TombRaiderVR.vcxproj ->' in s and ' error ' not in s
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    assert sha(w/'before'/n if n=='src/ControllerBodyAnchor.h' else r/n)==h,n
orig=json.loads(read(w/'port-original-hashes.json'));manifest=[];diff=[]
for n,h in orig.items():
    if h:assert sha(repo/n)==h,n
    else:assert not (repo/n).exists(),n
    manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
    diff+=list(difflib.unified_diff(read(repo/n).splitlines(True) if h else [],read(stage/n).splitlines(True),fromfile=n,tofile=n))
# Collection-specific camera, locomotion, landing and rig sources remain intact.
for p in (stage/'src').rglob('*'):
    if p.is_file():
        n=p.relative_to(stage).as_posix()
        if n not in orig:assert sha(p)==sha(repo/n),n
assert sha(stage/'src/FirstPersonIK.h')==sha(r/'src/FirstPersonIK.h')
for n in ['src/ControllerBodyAnchor.h','src/InspectionTurnEvidence.h']:assert sha(stage/n)==sha(r/n),n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
diff=[]
for p in (w/'before').rglob('*'):
    if p.is_file():
        n=p.relative_to(w/'before').as_posix()
        diff+=list(difflib.unified_diff(read(p).splitlines(True),read(r/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/InspectionTurnEvidence.h','tools/inspection_turn_tests.cpp']:
    diff+=list(difflib.unified_diff([],read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
    v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll');v['RenderChecks']=checks[v['Id']]
    assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
save('releases.json',releases)
production={p.relative_to(r).as_posix():sha(p) for p in (r/'src').rglob('*') if p.is_file()}
tests={p.relative_to(r).as_posix():sha(p) for p in (r/'tools').glob('*') if p.is_file() and p.suffix in ['.cpp','.h','.inl']}
tests.update({p.relative_to(r).as_posix():sha(p) for p in w.glob('*') if p.suffix in ['.cpp','.inl','.cmd']})
assert 'FAIL: looking down cannot permanently disable physical turning' in read(w/'inspection-turn-baseline.log')
assert 'Replayed 8093 poses; 0 mismatches' in read(w/'planted.log')
assert 'turn samples 0, movement samples 0' in read(w/'planted.log')
save('validation.json',dict(change='Release downward inspection on sustained corroborated body turns while retaining the stance facing reference; discard inspection gestures on ordinary exit.',checks=checks,inHeadsetTested=False,stationarySamples=8093,stationaryTurnSamples=0,stationaryTranslationSamples=0,recordedTurnWindows=[[53.8,55.5],[59,60],[243,244.5]],preserved=['Connected arms and weapon reach','Standing height and camera placement','Native landing-animation handoff','Native holster and climbing contacts'],compromise='Deep downward inspection requires a broad sustained turn: about 80 degrees around a compact stable pivot, or 110 degrees with modest articulation. Short ambiguous turns can remain held. Upright entry is unchanged; feet remain inferred.',broadSuiteLimitation=previous['broadSuiteLimitation'],captureSHA256=sha(w/'session.log'),productionHashes=production,testHashes=tests))
s=read(old/'deploy.ps1').replace('connected-arms','turn-reacquire').replace('10854778',str(checks['456'])).replace('5416918',str(checks['123']))
s='\n'.join(line for line in s.splitlines() if 'geometry.log' not in line and 'integration.log' not in line)+'\n'
extra=''
for file,key,label in [('inspection-turn.log','turn','inspection-turn recovery checks'),('anchor.log','anchor','anchor checks'),('inspection.log','inspection','sustained-inspection checks')]:
    extra+=f"if(!(Select-String -LiteralPath (Join-Path $PSScriptRoot '{file}') -SimpleMatch 'PASS: {checks[key]} {label}' -Quiet)) {{ throw 'Roomscale turn tests have not passed.' }}\n"
s=s.replace("$repo123='C:\\dev\\TombRaider123VR'",extra+"$repo123='C:\\dev\\TombRaider123VR'")
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared turn-recovery release:',checks)
