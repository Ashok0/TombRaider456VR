"""Read-only geometry/pose inspection of the installed v2 mirror build."""
from pathlib import Path
import sys,struct,json,time
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
mod=int(sys.argv[3])
def geom(at):
    data=read(at,120)
    ptrs=struct.unpack_from('<3Q',data)
    counts=struct.unpack_from('<6i',data,24)
    mapping,links,anim,poses=struct.unpack_from('<4Q',data,48)
    n=counts[1]
    return dict(address=hex(at),name=read(ptrs[0],128).split(b'\0')[0].decode(errors='replace') if ptrs[0] else None,
      mesh=hex(ptrs[2]),counts=counts,mapping=list(struct.unpack('<'+'i'*n,read(mapping,n*4))) if mapping and 0<n<512 else None,
      links=list(struct.unpack('<'+'i'*n,read(links,n*4))) if links and 0<n<512 else None)
try:
    app=value(base+0x66d0e0,'Q');lara=value(base+0x4ee840,'Q')
    result=dict(heads=[geom(base+0x52fd40+i*120) for i in range(4)],
      outfit=struct.unpack('<14i',read(value(app+0x4f8,'Q'),56)),
      active=value(mod+0x119295,'B'),room=value(lara+28,'h'),level=value(app+1264,'i'))
    pose=read(mod+0x11a1b0,1620)
    result['corrections']=[struct.unpack_from('<12f',pose,j*48) for j in range(33)]
    result['valid']=list(pose[33*48:33*48+33])
    result['bodyObject']=geom(base+0x697100+value(lara+16,'h')*3792+112)
    Path('build/mirror/live-face-v2.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
