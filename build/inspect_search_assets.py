from pathlib import Path
import struct,sys,collections
sys.path.insert(0,'tools')
from verify_revolver_laser_mesh import vertices
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
for p in sorted((root/'5/ITEM').glob('SEARCH_OBJECT*.TRM')):
 print(p.name)
 mesh=vertices(p)
 for j in sorted(set(j[0] for j,w in mesh.values())):
  pts=[p for p,(b,w) in mesh.items() if b[0]==j]
  print(' joint',j,'n',len(pts),'bounds',[(round(min(p[i] for p in pts),1),round(max(p[i] for p in pts),1)) for i in range(3)])
for p in sorted((root/'5/DATA').glob('*.PDP')):
 data=p.read_bytes();at=0;t=[]
 for s in (40,6,8,2,4,2,18):
  n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=n*s
 for i in range(t[6][1]):
  o=struct.unpack_from('<IhhIIh',data,t[6][0]+i*18)
  if o[0] not in (164,166,168,170):continue
  print(p.name,'obj',o)
  for a in range(o[-1],min(t[0][1],o[-1]+2)):
   anim=struct.unpack_from('<Ihhiiii8h',data,t[0][0]+a*40)
   print(' anim',a,anim)
   if p.stem=='RICH2' or o[0]==168:
    for f in range(0,anim[8]-anim[7]+1,10):
     size=anim[1]>>8;idx=f//(anim[1]&255)
     words=struct.unpack_from('<'+str(size)+'h',data,t[5][0]+anim[0]+idx*size*2)
     print(' frame',f,words)
