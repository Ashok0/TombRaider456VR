"""Read-only checks of installed button faces and native activation routes."""
from pathlib import Path
import re,struct,sys
from port_addresses import Image
from verify_revolver_laser_mesh import vertices
root=Path(sys.argv[1]);repo=Path(__file__).resolve().parents[1]
rows=[(4,0x696b4999,0x8d500,0x63ec06,repo/'PDB/tomb4.dll'),(5,0x696b499c,0x8c3f0,0x63d8db,repo/'PDB/tomb5.dll'),
 (4,0x68c12fda,0x8df10,0x63fb46,root/'4/tomb4.dll'),(5,0x68c12fe9,0x8c700,0x63d81b,root/'5/tomb5.dll')]
for g,stamp,collision,count,file in rows:
 im=Image(str(file));assert im.stamp==stamp
 assert im.mem[collision:collision+5]==bytes((0x48,0x89,0x5c,0x24,0x10 if g==4 else 8))
 assert any(i[3]==count and i[2]=='cmp m[rip]1,i3' for i in im.disasm(collision))
 print('PASS sequence handler signature and native availability counter:',g,hex(stamp))
source=(repo/'src/ButtonTouch.h').read_text()
profiles=re.findall(r'\{([01]),([12]),([01]),(true|false),"([^"]+)",\{([^}]+)\}\}',source)
assert len(profiles)==21
for game,joints,joint,sequence,name,rect in profiles:
 game=int(game)+4;joint=int(joint);left,right,top,bottom,z=map(float,rect.split(','))
 m=vertices(root/f'{game}/ITEM/{name}.TRM');points=[p for p,(j,w) in m.items() if j[0]==joint]
 assert points
 low=[min(p[k] for p in points) for k in range(3)];high=[max(p[k] for p in points) for k in range(3)]
 assert left>=low[0]-1 and right<=high[0]+1 and top>=low[1]-1 and bottom<=high[1]+1,(name,low,high)
 assert abs(z-low[2])<2,(name,z,low)
 print('PASS button press face:',game,name)
# Validate representative original poses/pivots; generated fixture matrices are
# read by the C++ tests so offsets are taken from assets, not invented in tests.
cases=[]
for game,level,slot in [(4,'JOBY1A',306),(4,'LOWSTRT',306),(4,'CSPLIT1',306),(4,'JOBY3B',307),(4,'KARNAK1',307),(4,'PALACES',307),
 (4,'JOBY2',341),(4,'JOBY2',342),(4,'JOBY2',343),(5,'ANDREA3',266),(5,'ANDY3',266),(5,'JOBY2',266),
 (5,'RICH2',267),(5,'RICH1',267),(5,'RICH3',268),(5,'RICH1',269),(5,'RICH2',270),(5,'RICH3',271)]:
 d=(root/f'{game}/DATA/{level}.PDP').read_bytes();at=0;t=[]
 for stride in (40,6,8,2,4,2,18):
  n=struct.unpack_from('<I',d,at)[0];at+=4;t.append((at,n));at+=n*stride
 objects=[struct.unpack_from('<IhhIIh',d,t[6][0]+i*18) for i in range(t[6][1])];obj=next(o for o in objects if o[0]==slot)
 a=struct.unpack_from('<Ihhiiii8h',d,t[0][0]+obj[-1]*40);f=struct.unpack_from('<'+str(a[1]>>8)+'h',d,t[5][0]+a[0])
 assert obj[1] in (1,2) and all(r==16384 for r in f[9:]),(level,slot,f)
 bones=(0,0,0) if obj[1]==1 else struct.unpack_from('<4i',d,t[4][0]+obj[3]*4)[1:]
 cases.append((game,slot,obj[1],a[2],*f[6:9],*bones))
 print('PASS authored idle/button pivot:',game,level,slot)
# TR5 sequence meshes/handler exist but none of the installed levels load them.
assert len(cases)==18
Path(sys.argv[2]).write_text('\n'.join(' '.join(map(str,c)) for c in cases)+'\n')
