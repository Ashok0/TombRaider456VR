from pathlib import Path
import sys,json,collections,hashlib,re,struct,time
sys.path.insert(0,str(Path('tools').resolve()))
from mirror_asset_audit import Trg,ROOT123,ROOT456
out={'scope':'Installed TR1-5 TRG room tables and TR6 GMX string labels. This is a structural/label audit, not visual certification of every surface.','trg':[],'gmx':[],'errors':[]}
for root in (ROOT123,ROOT456):
 for path in sorted(root.rglob('*.TRG')):
  try:
   t=Trg(path);materials=collections.Counter();additive=[]
   for room,chunks in enumerate(t.rooms):
    for c in chunks:materials[c[0]]+=1
    if sum(c[-1] for c in chunks):additive.append(room)
   out['trg'].append(dict(collection=root.name,asset=str(path.relative_to(root)),rooms=len(t.rooms),indices=t.nindices,vertices=t.nvertices,materials=dict(materials),additiveRooms=additive,sha256=hashlib.sha256(t.data).hexdigest()))
  except Exception as e:out['errors'].append(dict(path=str(path),error=str(e)))
 print(root.name, 'parsed',len(out['trg']),'errors',len(out['errors']),flush=True)
for path in sorted((ROOT456/'6/DATA/MAPS').rglob('*.GMX')):
 d=path.read_bytes();labels=set();animations=0;binary=0
 for m in re.finditer(b'mirror|reflec',d,re.I):
  lo=max(0,m.start()-100);hi=min(len(d),m.end()+100)
  a=m.start()
  while a>lo and 32<=d[a-1]<127:a-=1
  b=m.end()
  while b<hi and 32<=d[b]<127:b+=1
  s=d[a:b].decode('ascii','replace')
  if any(x in s.upper() for x in ('LARA_','KURTIS_','ANIM','CLIMB','WALK','RUN','TURN','SIDESTEP','LADDER','BADDIE_','COPA_','DOG_','BOXER1_','OBSCURA_','SAS_','PAT_','BUG_','KAREL_')):animations+=1
  elif len(s)>len(m.group())+2:labels.add(s)
  else:binary+=1
 out['gmx'].append(dict(asset=str(path.relative_to(ROOT456/'6/DATA/MAPS')),bytes=len(d),animationHits=animations,otherLabels=sorted(labels),unclassifiedHits=binary,sha256=hashlib.sha256(d).hexdigest()))
Path('build/mirror/collection-asset-audit.json').write_text(json.dumps(out,indent=2))
print('TRG',len(out['trg']),'rooms',sum(r['rooms'] for r in out['trg']),'GMX',len(out['gmx']),'errors',out['errors'])
print('Remaining unclassified GMX labels:',[(r['asset'],r['otherLabels']) for r in out['gmx'] if r['otherLabels']])
