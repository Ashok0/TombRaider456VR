from pathlib import Path
import sys, struct, json
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from verify_revolver_laser_mesh import vertices
game=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
data=(game/'4/DATA/SEMER2.PDP').read_bytes()
at=0; tables=[]
for stride in (40,6,8,2,4,2,18):
 n=struct.unpack_from('<I',data,at)[0];at+=4;tables.append((at,n));at+=n*stride
for i in [321,322,323,573,574,575,576,577]:
 p=tables[0][0]+i*40
 fields=struct.unpack_from('<Ihhiiii8h',data,p)
 print('animation',i,fields)
 for j in range(fields[-4]):
  change=struct.unpack_from('<3h',data,tables[1][0]+(fields[-3]+j)*6)
  print(' change',change,[struct.unpack_from('<4h',data,tables[2][0]+(change[2]+k)*8) for k in range(change[1])])
 print('commands',struct.unpack_from('<'+str(fields[-2]*3)+'h',data,tables[3][0]+fields[-1]*2))
for path in sorted((game/'4/ITEM').glob('COG_SWITCH*.TRM')):
 mesh=vertices(path)
 print(path.name)
 for joint in sorted(set(j[0] for j,w in mesh.values())):
  points=[p for p,(j,w) in mesh.items() if j[0]==joint]
  print(' joint',joint,'count',len(points),'bounds',[(min(p[i] for p in points),max(p[i] for p in points)) for i in range(3)])
if len(sys.argv)>1:
 exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
 try:
  items=value(base+0x69abe0,'Q');item=items+73*9416;obj=base+0x69ac00+317*3792
  result={'position':struct.unpack('<iii4h',read(item+96,20)),
    'matrices':struct.unpack('<48f',read(item+7832,192)),
    'bind':struct.unpack('<64f',read(obj+1676,256)),
    'bones':read(obj,112).hex(),'geom':read(obj+112,112).hex()}
  print(json.dumps(result,indent=2));Path(__file__).with_name('cog-live.json').write_text(json.dumps(result,indent=2))
 finally:k.CloseHandle(h)
