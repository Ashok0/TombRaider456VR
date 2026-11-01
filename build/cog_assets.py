from pathlib import Path
import sys, struct, collections, math
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from verify_revolver_laser_mesh import vertices
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
for g in (4,5):
 for p in sorted((root/str(g)/'ITEM').glob('COG_SWITCH*.TRM')):
  mesh=vertices(p);print(p.name)
  for j in sorted(set(j[0] for j,w in mesh.values())):
   points=[p for p,(b,w) in mesh.items() if b[0]==j]
   if len(points)<100:continue
   print('joint',j,'bounds',[(min(p[i] for p in points),max(p[i] for p in points)) for i in range(3)])
   levels=collections.Counter(round(p[1],1) for p in points)
   print('Y levels',levels.most_common(20))
   # Pegs are protrusions along the axle, beyond the wheel disc.
   mid=(min(p[1] for p in points)+max(p[1] for p in points))/2
   tips=[p for p in points if abs(p[1]-mid)>100]
   print('tips',[(tuple(round(x,1) for x in p)) for p in tips[:120]])
 for p in sorted((root/str(g)/'DATA').glob('*.PDP')):
  data=p.read_bytes();at=0;ts=[]
  for s in (40,6,8,2,4,2,18):
   n=struct.unpack_from('<I',data,at)[0];at+=4;ts.append((at,n));at+=n*s
  for i in range(ts[6][1]):
   o=struct.unpack_from('<IhhIIh',data,ts[6][0]+i*18)
   if o[0]!=(317 if g==4 else 277):continue
   print('object',g,p.name,o)
   for a in range(o[-1],o[-1]+2):print(' animation',a,struct.unpack_from('<Ihhiiii8h',data,ts[0][0]+a*40))
