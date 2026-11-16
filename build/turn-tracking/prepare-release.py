from pathlib import Path
import hashlib,json,difflib,re
r=Path.cwd();w=r/'build/turn-tracking';old=r/'build/turn-reacquire'
repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def read(p):
    b=p.read_bytes()
    return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig',errors='replace').replace('\r\n','\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(n,v):(w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
checks={}
for name,file,label in [('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('camera','targeted-camera.log','landing camera checks'),('turn','inspection-turn.log','inspection-turn recovery checks'),('opposed','opposed.log','opposed stance checks'),('anchor','anchor.log','anchor checks'),('swivel','swivel.log','swivel/walking checks'),('inspection','inspection.log','sustained-inspection checks'),('continuation','continuation.log','turn-continuation checks')]:
    s=read(w/file);m=re.search(r'PASS: (\d+) '+label,s)
    assert m and 'FAIL:' not in s,(name,s[-500:]);checks[name]=int(m[1])
for collection in ['456','123']:
    s=read(w/f'build-{collection}.log')
    assert 'TombRaiderVR.vcxproj ->' in s and ' error ' not in s
previous=json.loads(read(old/'validation.json'))
for n,h in previous['productionHashes'].items():
    assert sha(w/'before'/n if n in ['src/ControllerBodyAnchor.h','src/ControllerBodyAnchor.inl','src/InspectionTurnEvidence.h'] else r/n)==h,n
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
for n in ['tools/opposed_stance_tests.cpp']:
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
assert 'Closed-loop slow turn: 0/111 samples recovered' in read(w/'baseline.log')
assert 'FAIL: reported completed half-turn no longer leaves Lara facing the opposite way' in read(w/'baseline.log')
assert 'Closed-loop slow turn: 111/111 samples recovered' in read(w/'opposed.log')
assert 'Replayed 8093 poses; 0 mismatches' in read(w/'planted.log')
assert 'turn samples 0, movement samples 0' in read(w/'planted.log')
save('validation.json',dict(change='Recover sustained opposed head/controller facing after slow or completed turns. Latch an accepted body pose, retain corroborated continuation, smoothly recenter, and retire recovery after settling.',checks=checks,inHeadsetTested=False,stationarySamples=8093,stationaryTurnSamples=0,stationaryTranslationSamples=0,closedLoopBaseline=dict(corrected=0,samples=111,maximumHeadBodyDifferenceDegrees=179.9),closedLoopCandidate=dict(corrected=111,samples=111,maximumHeadBodyDifferenceDegrees=26.6),preserved=['Connected arms and weapon reach','Standing height and camera placement','Native landing-animation handoff','Native holster and climbing contacts','Disabled hand-assisted yaw preference'],compromise='This safeguard requires head and controller frame both more than 120 degrees from the frozen body, agreeing within 25 degrees for 0.75 seconds. Catch-up is bounded and can continue after motion stops. Existing ordinary turn entry and planted-glance inference remain active.',broadSuiteLimitation=previous['broadSuiteLimitation'],captureSHA256=sha(w/'session.log'),productionHashes=production,testHashes=tests))
s=read(old/'deploy.ps1').replace('turn-reacquire','turn-tracking').replace('10854778',str(checks['456'])).replace('5416918',str(checks['123']))
s='\n'.join(line for line in s.splitlines() if 'geometry.log' not in line and 'integration.log' not in line)+'\n'
extra=''
for file,key,label in [('opposed.log','opposed','opposed stance checks')]:
    extra+=f"if(!(Select-String -LiteralPath (Join-Path $PSScriptRoot '{file}') -SimpleMatch 'PASS: {checks[key]} {label}' -Quiet)) {{ throw 'Roomscale turn tests have not passed.' }}\n"
s=s.replace("$repo123='C:\\dev\\TombRaider123VR'",extra+"$repo123='C:\\dev\\TombRaider123VR'")
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared opposed-stance recovery release:',checks)
