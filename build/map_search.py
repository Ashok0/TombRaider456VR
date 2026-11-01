from pathlib import Path
import struct,pefile,sys
sys.path.insert(0,'tools')
from port_addresses import Image
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
a=Image('PDB/tomb5.dll');b=Image(str(root/'5/tomb5.dll'))
for start in (0x74f90,0x74bc0):
 key=a.key(start);matches=[s for s,e in zip(b.starts,b.ends) if abs((e-s)-(a.ends[a.starts.index(start)]-start))<64 and b.key(s)==key];print(hex(start),[hex(s) for s in matches])
p=pefile.PE('PDB/tomb5.dll');print('Search tables',struct.unpack('<12h',p.get_data(0x1283a0,24)))
for name in ('RICH1','RICH2','JOBY2','ANDY1','JOBY3'):
 data=(root/f'5/DATA/{name}.PDP').read_bytes();at=0;t=[]
 for s in (40,6,8,2,4,2,18):
  n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=n*s
 for i in range(t[6][1]):
  o=struct.unpack_from('<IhhIIh',data,t[6][0]+i*18)
  if o[0] not in (164,166):continue
  print(name,o[0],'bones',struct.unpack_from('<'+str((o[1]-1)*4)+'i',data,t[4][0]+o[3]*4))
