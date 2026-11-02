"""Read-only TRG material/geometry audit shared by mirror verification tools."""
from pathlib import Path
import struct,json,collections,sys

ROOT456=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered')
ROOT123=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered')

class Trg:
    def __init__(self,path):
        self.path=Path(path);self.data=d=self.path.read_bytes()
        assert struct.unpack_from('<I',d)[0]==0x03475254
        n=struct.unpack_from('<I',d,4)[0];at=8+n*24
        _,fog,boxes=struct.unpack_from('<hbb',d,at+24);assert fog>=0 and boxes>=0
        at+=28+fog*40+boxes*28
        nr=struct.unpack_from('<I',d,at)[0];at+=4;assert nr<4096
        self.rooms=[]
        for room in range(nr):
            n=struct.unpack_from('<I',d,at)[0];at+=4;assert n<4096
            chunks=[struct.unpack_from('<BBH4f6I',d,at+j*44) for j in range(n)];at+=n*44
            flags,nlights=struct.unpack_from('<II',d,at);at+=8+nlights*28
            self.rooms.append(chunks)
        ntiles=struct.unpack_from('<I',d,at)[0];at+=4
        self.tiles=struct.unpack_from('<'+str(ntiles)+'H',d,at);at+=((ntiles+1)//2)*4
        self.nindices,self.nvertices=struct.unpack_from('<II',d,at)
        self.indexAt=at+8;self.vertexAt=self.indexAt+self.nindices*4
        assert self.vertexAt+self.nvertices*20==len(d),(path,len(d),self.vertexAt,self.nvertices)
    def faces(self,room,part=2):
        d=self.data
        for c in self.rooms[room]:
            start,count=c[7+part*2:9+part*2]
            for index in range(start,start+count,3):
                ids=struct.unpack_from('<3I',d,self.indexAt+index*4)
                verts=[struct.unpack_from('<4h12B',d,self.vertexAt+i*20) for i in ids]
                yield dict(index=index,material=c[0],positions=[v[:3] for v in verts],
                           layers=[v[7] for v in verts],uv=[(v[11],v[15]) for v in verts])

if __name__=='__main__':
    trg=Trg(ROOT456/'5/DATA/RICH2.TRG')
    records=[]
    for room in (134,135):
        faces=list(trg.faces(room));groups=collections.defaultdict(list)
        for f in faces:
            p=f['positions'];axis=next((a for a in (0,2) if p[0][a]==p[1][a]==p[2][a]),None)
            if axis is not None:groups[(axis,p[0][axis],tuple(f['layers']))].append(f)
        for key,ff in groups.items():
            pp=[p for f in ff for p in f['positions']]
            records.append(dict(room=room,key=key,bounds=[(min(p[a] for p in pp),max(p[a] for p in pp)) for a in range(3)],faces=ff))
    out=Path('build/mirror/room-surfaces.json');out.write_text(json.dumps(dict(tiles=trg.tiles,surfaces=records),indent=2))
    print(json.dumps([{k:v for k,v in r.items() if k!='faces'} for r in records],indent=2))
