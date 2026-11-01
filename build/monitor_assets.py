from pathlib import Path
import sys,struct
sys.path.insert(0,'tools')
from verify_revolver_laser_mesh import vertices
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered/5')
for name in ('MONITOR_SCREEN','SECURITY_SCREENS','ANIMATING7_RICH2_RICH1'):
 p=root/'ITEM'/f'{name}.TRM';m=vertices(p);print(name,'vertices',len(m),'bounds',[(min(v[k] for v in m),max(v[k] for v in m)) for k in range(3)])
 print('points',list(m)[:30])
for p in sorted((root/'DATA').glob('*.PDP')):
 data=p.read_bytes();at=0;t=[]
 for s in (40,6,8,2,4,2,18):
  n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=n*s
 for i in range(t[6][1]):
  o=struct.unpack_from('<IhhIIh',data,t[6][0]+i*18)
  if o[0]!=151:continue
  a=struct.unpack_from('<Ihhiiii8h',data,t[0][0]+o[-1]*40)
  f=struct.unpack_from('<'+str(a[1]>>8)+'h',data,t[5][0]+a[0])
  print(p.name,o,'anim',a,'frame',f)
