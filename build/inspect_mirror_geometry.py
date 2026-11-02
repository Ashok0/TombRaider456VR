from pathlib import Path
import struct,collections,json
root=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered')
d=(root/'5/DATA/RICH2.TRG').read_bytes()
assert struct.unpack_from('<I',d)[0]==0x03475254
n=struct.unpack_from('<I',d,4)[0];at=8+n*24
lights,fog,boxes=struct.unpack_from('<hbb',d,at+24);at+=28+fog*40+boxes*28
rooms=struct.unpack_from('<I',d,at)[0];at+=4;records=[]
for i in range(rooms):
 n=struct.unpack_from('<I',d,at)[0];at+=4
 chunks=[struct.unpack_from('<BBH4f6I',d,at+j*44) for j in range(n)];at+=n*44
 flags,nlights=struct.unpack_from('<II',d,at);at+=8+nlights*28
 records.append(chunks)
tiles=struct.unpack_from('<I',d,at)[0];at+=4+((tiles+1)//2)*4
indices,vertices=struct.unpack_from('<II',d,at);at+=8
ix=struct.unpack_from('<'+str(indices)+'I',d,at);at+=indices*4
stride=(len(d)-at)//vertices
assert at+vertices*stride==len(d)
print('rooms/index/vertex/stride',rooms,indices,vertices,stride)
chunks=records[134];print('room chunks',chunks)
verts=[struct.unpack_from('<3h',d,at+i*stride) for i in range(vertices)]
output=[]
for chunk in chunks:
 for part in range(3):
  start,count=chunk[7+part*2:9+part*2]
  faces=[([verts[j] for j in ix[i:i+3]],i) for i in range(start,start+count,3)]
  output+=faces
  if faces:print('range',part,count,'bounds',[(min(p[k] for tri,_ in faces for p in tri),max(p[k] for tri,_ in faces for p in tri)) for k in range(3)])
Path('build/mirror-room-triangles.json').write_text(json.dumps(output))
# Group large, flat wall faces to locate the actual glass and surrounding frame.
planes=collections.defaultdict(list)
for tri,index in output:
 for axis in (0,2):
  if max(p[axis] for p in tri)-min(p[axis] for p in tri)<.05:
   planes[(axis,round(tri[0][axis],1))].append((tri,index))
for (axis,plane),faces in sorted(planes.items(),key=lambda v:len(v[1]),reverse=True)[:22]:
 print('plane',axis,plane,len(faces),[(round(min(p[k] for t,_ in faces for p in t),2),round(max(p[k] for t,_ in faces for p in t),2)) for k in range(3)])

