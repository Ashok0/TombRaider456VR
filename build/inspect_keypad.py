"""Read-only nearby TR5 keypad snapshot; no writes or calls into the game."""
from pathlib import Path
exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
    assert value(base+value(base+0x3c,'I')+8,'I')==0x68C12FE9
    lara=value(base+0x4ee840,'Q');pos=struct.unpack('<iii',read(lara+96,12))
    app=value(base+0x66d0e0,'Q');items=value(base+0x66d0e8,'Q');count=value(base+0x65b234,'i')
    rows=[]
    for i in range(count):
        item=items+i*9416;slot=value(item+16,'h')
        if slot<0:continue
        xyz=struct.unpack('<iii',read(item+96,12));dist=sum((a-b)**2 for a,b in zip(pos,xyz))**.5
        if dist>2200:continue
        obj=base+0x697100+slot*3792;nameptr=value(obj+112,'Q')
        name=read(nameptr,128).split(b'\0')[0].decode(errors='replace') if nameptr else ''
        rows.append(dict(index=i,slot=slot,name=name,distance=round(dist),position=xyz,yaw=value(item+110,'h'),
            ocb=value(item+48,'h'),room=value(item+28,'h'),states=struct.unpack('<5h',read(item+18,10)),
            flags=hex(value(item+44,'H')),status=hex(value(item+0x1820,'I')),
            joints=value(obj,'h'),bones=value(obj+140,'i'),collision=hex(value(obj+56,'Q')-base),
            inverseBind=struct.unpack('<16f',read(obj+1676,64)),matrix=struct.unpack('<12f',read(item+7832,48))))
    rows.sort(key=lambda x:x['distance'])
    out=dict(laraPosition=pos,level=value(app+1264,'i'),inventory=value(app+2148,'i'),nearby=rows)
    Path('build/keypad-live.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
finally:k.CloseHandle(h)
