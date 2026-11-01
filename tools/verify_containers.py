"""Read installed container meshes/animations and make native-matrix test fixtures.
No game DLL is loaded or executed.
"""
from pathlib import Path
import math,struct,sys
import pefile
from verify_revolver_laser_mesh import vertices
from port_addresses import Image
root=Path(sys.argv[1]);repo=Path(__file__).resolve().parents[1]
a=Image(str(repo/'PDB/tomb5.dll'));b=Image(str(root/'5/tomb5.dll'))
for x,y in ((0x74f90,0x76670),(0x74bc0,0x762a0),(0x20cf0,0x210c0)):
 assert a.key(x)==b.key(y),(hex(x),hex(y))
assert a.mem[0x74f90:0x74f95]==b.mem[0x76670:0x76675]==bytes.fromhex('48 89 5c 24 10')
assert a.mem[0x20cf0:0x20cf5]==b.mem[0x210c0:0x210c5]==bytes.fromhex('48 89 4c 24 08')
assert struct.unpack_from('<4h',a.mem,0x1283b0)==(180,100,153,83)
print('PASS both native search/collision/matrix layouts; native pickup frames')
def mul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def vec(a,b):return [sum(a[i][k]*b[k] for k in range(3)) for i in range(3)]
def rotations(w,n):
 out=[];p=9
 for j in range(n):
  word=w[p];p+=1;kind=word>>14;x=y=z=0
  if kind:
   angle=(word&0xfff)*2*math.pi/4096
   if kind==1:x=angle
   elif kind==2:y=angle
   else:z=angle
  else:
   packed=(word<<16)|w[p];p+=1;x=((packed>>20)&1023)*2*math.pi/1024;y=((packed>>10)&1023)*2*math.pi/1024;z=(packed&1023)*2*math.pi/1024
  cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
  out.append(mul(mul([[cy,0,sy],[0,1,0],[-sy,0,cy]],[[1,0,0],[0,cx,-sx],[0,sx,cx]]),[[cz,-sz,0],[sz,cz,0],[0,0,1]]))
 return out
cases=[]
for level,slot,name,handles in [
 ('RICH2',166,'SEARCH_OBJECT2_RICH2',[(5.22,-160.76,28)]),
 ('ANDY1',166,'SEARCH_OBJECT2_ANDY1',[(.66,-164.5,23)]),
 ('JOBY3',166,'SEARCH_OBJECT2_JOBY3_JOBY5',[(-1.38,-162.5,20)]),
 ('JOBY5',166,'SEARCH_OBJECT2_JOBY3_JOBY5',[(-1.38,-162.5,20)]),
 ('RICH2',164,'SEARCH_OBJECT1_RICH2',[(-49.96,-1805,-228),(49.96,-1805,-228)]),
 ('RICH1',164,'SEARCH_OBJECT1_RICH1',[(-47.6,-1814,-231),(45.6,-1814,-231)]),
 ('JOBY2',164,'SEARCH_OBJECT1_JOBY2',[(-51.44,-1804,-237),(41.44,-1804,-237)]),
 ('JOBY5',164,'SEARCH_OBJECT1_JOBY5',[(-51.44,-1801.32,-237),(41.44,-1801.32,-237)])]:
 data=(root/f'5/DATA/{level}.PDP').read_bytes();at=0;t=[]
 for stride in (40,6,8,2,4,2,18):
  n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=stride*n
 objects=[struct.unpack_from('<IhhIIh',data,t[6][0]+i*18) for i in range(t[6][1])]
 obj=next(o for o in objects if o[0]==slot);joints=obj[1];base=obj[-1]
 idle,use=[struct.unpack_from('<Ihhiiii8h',data,t[0][0]+i*40) for i in (base,base+1)]
 assert idle[2]==0 and idle[7:9]==(0,0)
 assert use[1]&255==4 and use[2:7]==(1,0,0,0,0) and use[7:11]==(1,200 if slot==166 else 313,base,0)
 bones=[(0,0,0)]+[struct.unpack_from('<4i',data,t[4][0]+(obj[3]+j*4)*4)[1:] for j in range(joints-1)]
 mesh=vertices(root/f'5/ITEM/{name}.TRM')
 for j,h in enumerate(handles):
  pts=[p for p,(b,w) in mesh.items() if b[0]==(1 if slot==166 else j+1)]
  assert min(math.dist(p,h) for p in pts)<20,(name,j)
 def frame(anim,key):
  size=anim[1]>>8;w=struct.unpack_from('<'+str(size)+'H',data,t[5][0]+anim[0]+key*size*2)
  r=rotations(w,joints);translation=[v if v<32768 else v-65536 for v in w[6:9]]
  result=[]
  for j in range(joints):
   rotation=r[0] if j==0 else mul(r[0],r[j]);v=vec(r[0],bones[j]);origin=[translation[k]+v[k] for k in range(3)]
   result.extend(sum((rotation[k]+[origin[k]] for k in range(3)),[]))
  return result
 poses=[]
 for f in range(221):
  if f==0:m=frame(idle,0)
  else:
   native=min(f,use[8]);key=(native-use[7])//4;mix=((native-use[7])%4)/4
   m=frame(use,key)
   if mix:
    nxt=frame(use,key+1);m=[x+(y-x)*mix for x,y in zip(m,nxt)]
  poses.append(m)
 for j,h in enumerate(handles):
  joint=0 if slot==166 else j+1;local=[h[k]-bones[joint][k] for k in range(3)]
  def point(m):return [sum(m[joint*12+k*4+v]*local[v] for v in range(3))+m[joint*12+k*4+3] for k in range(3)]
  p0=point(poses[0]);distance=max(math.dist(p0,point(m)) for m in poses[:101 if slot==166 else 221])
  assert (150 if slot==166 else 400)<distance<1500,(level,distance)
 cases.append((slot,joints,name,bones,poses))
 print('PASS installed handle geometry and opening paths:',level,name)
out=Path(sys.argv[2]);buf=bytearray(struct.pack('<I',len(cases)))
for slot,joints,name,bones,poses in cases:
 buf+=struct.pack('<ii64s',slot,joints,name.encode())
 for bone in bones:buf+=struct.pack('<3f',*bone)
 for p in poses:buf+=struct.pack('<'+str(len(p))+'f',*p)
out.write_bytes(buf)
