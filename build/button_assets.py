from pathlib import Path
import struct
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
for g in (4,5):
 for p in sorted((root/str(g)/'DATA').glob('*.PDP')):
  data=p.read_bytes();at=0;t=[]
  for s in (40,6,8,2,4,2,18):
   n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=n*s
  for i in range(t[6][1]):
   o=struct.unpack_from('<IhhIIh',data,t[6][0]+i*18)
   if not ((306<=o[0]<=313 or 320<=o[0]<=322) if g==4 else (266<=o[0]<=273 or 280<=o[0]<=282)):continue
   if o[1]>3 and o[0] not in (268,270):continue
   a=struct.unpack_from('<Ihhiiii8h',data,t[0][0]+o[-1]*40)
   f=struct.unpack_from('<'+str(a[1]>>8)+'h',data,t[5][0]+a[0])
   print(g,p.stem,'slot',o[0],'joints',o[1],'base',o[-1],'idle',a[2],a[7:11],'frame',f)
