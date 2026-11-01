"""Read-only validation of TR5 screen contact geometry and native monitor calls."""
from pathlib import Path
import struct,sys
from port_addresses import Image
from verify_revolver_laser_mesh import vertices
repo=Path(__file__).resolve().parents[1];root=Path(sys.argv[1])
for path,stamp,collision,trigger,test,move in [
 (repo/'PDB/tomb5.dll',0x696b499c,0x75230,0x8a6e0,0xb480,0xbae0),
 (root/'5/tomb5.dll',0x68c12fe9,0x76910,0x8a9f0,0xb580,0xbbe0)]:
 im=Image(str(path));assert im.stamp==stamp
 assert im.mem[collision:collision+10]==bytes.fromhex('48 89 5c 24 08 48 89 6c 24 10')
 assert im.mem[trigger:trigger+8]==bytes.fromhex('40 53 56 57 48 83 ec 30')
 # .pdata splits the collision at an internal unwind boundary. Decode through
 # its real return to verify both activation and the original trigger branch.
 code=list(im.md.disasm(im.mem[collision:collision+900],collision))
 end=next(i.address+i.size for i in code if i.mnemonic=='ret')
 im.ends[im.starts.index(collision)]=end;ins=im.disasm(collision)
 calls=[i[4] for i in ins if i[2].startswith('call')]
 assert calls.count(trigger)==1 and calls.count(test)==2 and calls.count(move)==1
 raw='\n'.join(i.mnemonic+' '+i.op_str for i in code if i.address<end)
 for marker in ('0xc5','0x18','0x1820','0x2c], 0x20','0x12], 0x28','0x18], 0xc5'):
  assert marker in raw,(path,marker)
 print('PASS native monitor flags, alignment and trigger branch:',hex(stamp))
mesh=vertices(root/'5/ITEM/MONITOR_SCREEN.TRM')
# Glass corners and curved outer edge, excluding its stand, casing and wheels.
for x,y,z in [(-130.2,-626.7,-381.9),(128.2,-626.7,-381.9),(-130.2,-369.9,-381.9),(128.2,-369.9,-381.9),(-146,-628.4,-385.7),(144,-368.2,-385.7)]:
 assert min(sum((p[k]-(x,y,z)[k])**2 for k in range(3)) for p in mesh)<.1
print('PASS screen rectangle/plane matches installed glass vertices')
found=[]
for file in sorted((root/'5/DATA').glob('*.PDP')):
 data=file.read_bytes();at=0;t=[]
 for stride in (40,6,8,2,4,2,18):
  n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=n*stride
 for i in range(t[6][1]):
  obj=struct.unpack_from('<IhhIIh',data,t[6][0]+i*18)
  if obj[0]!=151:continue
  assert obj[1]==1
  anim=struct.unpack_from('<Ihhiiii8h',data,t[0][0]+obj[-1]*40)
  assert anim[1]==2561 and anim[7:9]==(0,0)
  words=struct.unpack_from('<10h',data,t[5][0]+anim[0])
  assert words==(-202,205,-924,-223,358,516,0,-223,0,-30720)
  lara=struct.unpack_from('<Ihhiiii8h',data,t[0][0]+197*40)
  assert lara[2]==40 and lara[8]>=lara[7]+24
  found.append(file.stem);print('PASS native screen pivot and interaction animation:',file.stem)
assert found==['RICH1','RICH2']
