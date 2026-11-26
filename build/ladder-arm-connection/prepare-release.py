from pathlib import Path
import hashlib,json,difflib,re
r=Path.cwd();w=r/'build/ladder-arm-connection';old=r/'build/turn-pivot-response';repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):
 b=p.read_bytes()
 return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig',errors='replace').replace('\r\n','\n')
def save(n,v):(w/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
labels=[('456','test-arms-456.log','straight-up jump arm checks'),('123','test-123.log','locomotion, gun-control and camera hook checks'),('integration','integration.log','connected arm integration checks')]
checks={}
for name,file,label in labels:
 s=read(w/file);m=re.search(r'PASS: (\d+) '+label,s)
 assert m and 'FAIL:' not in s,(name,s[-700:]);checks[name]=int(m[1])
for c in ['456','123']:
 s=read(w/f'build-{c}.log');assert 'TombRaiderVR.vcxproj ->' in s and ' error ' not in s
previous=json.loads(read(old/'validation.json'))
allowed={'src/FirstPerson.cpp'}
for n,h in previous['productionHashes'].items():
 assert sha(w/'before'/n if n in allowed else r/n)==h,n
original=json.loads(read(w/'original123.json'));manifest=[];diff=[]
testAllowed={'tests/ik_collection_parity_selftest.h'}
for n,h in original.items():
 assert sha(repo/n)==h,n
 if sha(stage/n)==h:continue
 assert n in allowed or n in testAllowed or n in {'README.md','TombRaiderVR.ini'},n
 manifest.append(dict(Relative=n,OriginalHash=h,ModifiedHash=sha(stage/n),Deleted=False))
 diff+=list(difflib.unified_diff(read(repo/n).splitlines(True),read(stage/n).splitlines(True),fromfile=n,tofile=n))
for n in ['src/FirstPersonIK.h','src/ControllerBodyAnchor.h','src/CoherentBodyMotion.h']:
 assert sha(stage/n)==sha(r/n),n
save('source123-manifest.json',manifest)
(w/'source123-review.diff').write_text(''.join(diff),encoding='utf-8')
diff=[]
for p in (w/'before').rglob('*'):
 if p.is_file():
  n=p.relative_to(w/'before').as_posix()
  if sha(p)==sha(r/n):continue
  diff+=list(difflib.unified_diff(read(p).splitlines(True),read(r/n).splitlines(True),fromfile=n,tofile=n))
(w/'source456-review.diff').write_text(''.join(diff),encoding='utf-8')
releases=json.loads(read(old/'releases.json'))
for v in releases:
 v['Previous']=v['Expected'];v['Expected']=sha(w/v['Id']/'bin/TombRaiderVR.dll');v['RenderChecks']=checks[v['Id']]
 assert sha(Path(v['Directory'])/'TombRaiderVR.dll')==v['Previous']
save('releases.json',releases)
production={p.relative_to(r).as_posix():sha(p) for p in (r/'src').rglob('*') if p.is_file()}
tests={p.relative_to(r).as_posix():sha(p) for p in (r/'tools').glob('*') if p.is_file() and p.suffix in ['.cpp','.h','.inl']}
tests.update({p.relative_to(r).as_posix():sha(p) for p in w.glob('*') if p.suffix in ['.cpp','.inl','.cmd']})
save('validation.json',dict(change='Ladder shoulders share full torso clearance; preserve native rung grips with axial arm extension.',checks=checks,inHeadsetTested=False,preserved=['Installed INIs','Camera and height logic','Roomscale and analog rotation','Native rung wrist pose and hand size','Native pelvis, leg animation and shadows'],compromise='Arms extend axially when torso clearance puts the native rung beyond reach. Extreme clearance can visibly lengthen arms.',broadSuiteLimitation=previous['broadSuiteLimitation'],productionHashes=production,testHashes=tests))
s=read(old/'deploy.ps1').replace('turn-pivot-response','ladder-arm-connection')
s='\n'.join(line for line in s.splitlines() if not line.startswith('if(!(Select-String'))+'\n'
extra=''
for name,file,label in labels:
 extra+=f"if(!(Select-String -LiteralPath (Join-Path $PSScriptRoot '{file}') -SimpleMatch 'PASS: {checks[name]} {label}' -Quiet)) {{ throw '{name} validation has not passed.' }}\n"
s=s.replace("$repo123='C:\\dev\\TombRaider123VR'",extra+"$repo123='C:\\dev\\TombRaider123VR'")
(w/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared ladder-arm-connection release:',checks)
print('TR1-3 changed files:',len(manifest))
