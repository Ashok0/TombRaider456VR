"""Offline facial skeleton audit; produces fixtures from the shipped head meshes."""
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1] if len(sys.argv)>1 else
            r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered')
out = Path('build/mirror');out.mkdir(parents=True,exist_ok=True)
fixture = ['// Generated from shipped TRM files; no game DLL is loaded.']
for name in ('LARA_HEAD','LARA_YOUNG_HEAD','LARA_CREEPY_HEAD'):
    data=(root/f'4/ITEM/{name}.TRM').read_bytes()
    magic,chunks=struct.unpack_from('<II',data)
    assert magic==0x024d5254
    at=8+chunks*44
    tiles=struct.unpack_from('<I',data,at)[0];at+=4+((tiles+1)//2)*4
    bones=struct.unpack_from('<I',data,at)[0];at+=4
    assert bones==33
    poses=struct.unpack_from('<'+str(bones*12)+'f',data,at);at+=bones*48
    total=struct.unpack_from('<I',data,at)[0];at+=4
    mapping=struct.unpack_from('<'+str(total)+'i',data,at);at+=total*4
    links=struct.unpack_from('<'+str(total)+'i',data,at);at+=total*4
    animated=struct.unpack_from('<I',data,at)[0];at+=4
    animation_joints=struct.unpack_from('<'+str(animated)+'i',data,at);at+=animated*4
    frames,start=struct.unpack_from('<HH',data,at);at+=4+frames*animated*48
    indices,vertices=struct.unpack_from('<II',data,at);at+=8+((indices+1)//2)*4
    assert at+vertices*24==len(data)
    assert mapping[:7]==(8,7,4,1,14,11,0) and mapping[7:33]==tuple(range(39,65))
    # Each visible facial helper's parent chain reaches the native head slot.
    for slot in range(7,bones):
        current=slot;visited=set()
        while mapping[current]!=14:
            assert current not in visited and 0<=current<total
            visited.add(current);current=links[current]
        assert current==4
    skin=[]
    for vertex in range(vertices):
        joints=struct.unpack_from('<4B',data,at+vertex*24+16)
        weights=struct.unpack_from('<4B',data,at+vertex*24+20)
        skin.append((joints[:3],weights[:3]))
    assert all(sum(w)==255 and all(j<33 for j,weight in zip(js,w) if weight) for js,w in skin)
    helper_vertices=sum(any(mapping[j]>=15 and weight for j,weight in zip(js,w)) for js,w in skin)
    assert helper_vertices>0
    if name=='LARA_HEAD':
        fixture.append('constexpr int32_t AuthoredFaceMap[33]={'+','.join(map(str,mapping[:33]))+'};')
        fixture.append('constexpr unsigned char AuthoredFaceSkin[][6]={')
        fixture.extend('{'+','.join(map(str,js+w))+'},' for js,w in skin)
        fixture.append('};')
    print(f'PASS {name}: {bones} skin bones, 26 facial helpers inherit head 14, {helper_vertices}/{vertices} vertices use them')
(out/'face-fixture.h').write_text('\n'.join(fixture)+'\n')
