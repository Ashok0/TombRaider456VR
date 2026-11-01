"""Read-only elevator/profile snapshot. No calls into or writes to the game."""
from pathlib import Path
import subprocess,time
exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
mod=int(sys.argv[3])
try:
    symbols={}
    out=subprocess.check_output([sys.executable,'tools/pdbdump.py','build/vr-options-release/TombRaiderVR.dll'],text=True,stderr=subprocess.DEVNULL)
    for line in out.splitlines():
        x=line.split()
        if len(x)==4 and x[1]=='data':symbols[x[3].split('::')[-1]]=int(x[0],16)
    items=value(base+0x66d0e8,'Q');count=value(base+0x65b234,'i');panels=[]
    for i in range(count):
        item=items+i*9416
        if value(item+16,'h')!=268:continue
        obj=base+0x697100+268*3792
        panels.append(dict(index=i,type=value(item+48,'h'),room=value(item+28,'h'),status=hex(value(item+0x1820,'I')),
            flags=hex(value(item+44,'H')),states=struct.unpack('<5h',read(item+18,10)),itemFlags=struct.unpack('<4h',read(item+56,8)),
            position=struct.unpack('<iii3h',read(item+96,18)),bones=value(obj+112+28,'i'),
            inverseBind=struct.unpack('<16f',read(obj+1676,64)),matrix=struct.unpack('<12f',read(item+7832,48))))
    gates={n:value(mod+symbols[n],fmt) for n,fmt in {'g_buttonDll':'Q','g_switchDll':'Q','g_buttonLevel':'i','g_active':'B','g_scenePoseValid':'B','g_switchGripClaimed':'B','g_cogClaimed':'B','g_containerClaimed':'B'}.items()}
    rows=[]
    for t in range(20):
        hands=[]
        for hand in range(2):
            at=mod+symbols['g_buttonHands']+hand*104
            hands.append(dict(world=struct.unpack('<3f',read(at,12)),time=value(at+16,'Q'),valid=value(at+24,'B'),latched=value(at+32,'B')))
        rows.append(hands);time.sleep(.1)
    result=dict(gates=gates,panels=panels,hands=rows)
    Path('build/elevator-touch-live.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
