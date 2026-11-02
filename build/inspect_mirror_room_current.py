"""Read-only room geometry snapshot; no writes or calls into the game."""
from pathlib import Path
import json,struct,sys
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
exe=int(sys.argv[3])
try:
    assert value(exe+0x17b3ac,'i')==1,'Expected active TR5'
    pe=value(base+0x3c,'I');assert value(base+pe+8,'I')==0x68c12fe9
    app=value(base+0x66d0e0,'Q');lara=value(base+0x4ee840,'Q')
    room=value(lara+28,'h');count=value(base+0x65b230,'h');rooms=value(base+0x65eb68,'Q')
    assert 0<=room<count<1024
    records=[]
    for index in range(count):
        data=read(rooms+index*304,304);chunks=struct.unpack_from('<i',data,256)[0];ptr=struct.unpack_from('<Q',data,264)[0]
        assert 0<=chunks<256
        records.append(dict(room=index,origin=struct.unpack_from('<iii',data,40),
          chunks=[struct.unpack('<BBH4f6I',read(ptr+j*44,44)) for j in range(chunks)]))
    result=dict(game=5,level=value(app+1264,'i'),room=room,roomCount=count,
      position=struct.unpack('<iii',read(lara+96,12)),yaw=value(lara+110,'h'),
      camera=struct.unpack('<12i',read(base+0x4ef880,48)),rooms=records)
    Path('build/mirror/current-room.json').write_text(json.dumps(result,indent=2))
    result['rooms']=[records[room]];print(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
