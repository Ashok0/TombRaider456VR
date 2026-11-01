from pathlib import Path
import sys,struct,json,collections
sys.path.insert(0,str(Path('tools').resolve()))
import pefile
from verify_revolver_laser_mesh import vertices
root=Path('C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
for game,names,objects in ((4,0x12b7b0,0x699cc0),(5,0x1283d0,0x6971c0)):
    pe=pefile.PE(f'PDB/tomb{game}.dll');mem=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    entries=[]
    for i in range(460):
        at=struct.unpack_from('<Q',mem,names+i*8)[0]-base
        if not 0<=at<len(mem):continue
        name=mem[at:mem.index(b'\0',at)].decode(errors='replace')
        if any(w in name for w in ('PULLEY','CHAIN','ROPE')): entries.append((i,name))
    print('SLOTS',game,entries)
    for filename in ['PULLEY.TRM']:
        v=vertices(root/str(game)/'ITEM'/filename)
        groups=collections.defaultdict(list)
        for p,(j,w) in v.items():groups[j[0]].append(p)
        print('MESH',game,filename,[(j,len(p),[round(min(v[k] for v in p),2) for k in range(3)],[round(max(v[k] for v in p),2) for k in range(3)]) for j,p in groups.items()])
        Path(f'build/pulley-mesh{game}.json').write_text(json.dumps({str(j):p for j,p in groups.items()}))
    slot=next(i for i,n in entries if n=='PULLEY')
    for path in sorted((root/str(game)/'DATA').glob('*.PDP')):
        d=path.read_bytes();at=0;t=[]
        for stride in (40,6,8,2,4,2,18):
            n=struct.unpack_from('<I',d,at)[0];at+=4;t.append((at,n));at+=n*stride
        for oi in range(t[6][1]):
            o=struct.unpack_from('<IhhIIh',d,t[6][0]+oi*18)
            if o[0]!=slot:continue
            end=min([struct.unpack_from('<IhhIIh',d,t[6][0]+x*18)[-1] for x in range(t[6][1]) if struct.unpack_from('<IhhIIh',d,t[6][0]+x*18)[-1]>o[-1]]+[t[0][1]])
            a=[]
            for ai in list(range(o[-1],end))+[339,340,341]:
                r=struct.unpack_from('<Ihhiiii8h',d,t[0][0]+ai*40)
                f=struct.unpack_from('<'+str(r[1]>>8)+'h',d,t[5][0]+r[0])
                a.append(dict(index=ai,state=r[2],first=r[7],last=r[8],jump=r[9:11],changes=r[11:13],commands=r[13:15],frame=f[:12]))
            print('LEVEL',game,path.name,'object',o,'animations',json.dumps(a))
