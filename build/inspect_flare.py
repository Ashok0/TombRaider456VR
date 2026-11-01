"""Read-only flare gesture/native-state sample. Never modifies game memory."""
from pathlib import Path
import time,json
exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
mod=int(sys.argv[3]);duration=float(sys.argv[4]) if len(sys.argv)>4 else 1
try:
    flags={n:hex(value(mod+r,'Q')) if n=='profile' else value(mod+r,'B') for n,r in
        [('profile',0x1132D0),('active',0x1130EE),('heading',0x1128ED),('scene',0x112DBC),('flareHooks',0x112F1D)]}
    rows=[];start=time.perf_counter()
    while time.perf_counter()-start<duration:
        g=read(mod+0x10EEA0,1088);q=read(mod+0x10EA30,64);count=struct.unpack_from('<i',g,1024)[0]
        row=dict(t=round(time.perf_counter()-start,3),count=count,serial=struct.unpack_from('<Q',g,1032)[0],
            armed=g[1040],quiet=struct.unpack_from('<d',g,1048)[0],swing=struct.unpack_from('<d',g,1056)[0],
            pending=struct.unpack_from('<d',q,32)[0],identity=[hex(x) for x in struct.unpack_from('<3Q',q)],level=struct.unpack_from('<i',q,24)[0])
        if 0<count<=32:row['last']=struct.unpack_from('<6fd',g,(count-1)*32)
        profile=value(mod+0x1132D0,'Q')
        game=value(profile+4,'i') if profile else 0
        lb=base+(0x4EE680 if game==5 else 0x4F3D80)
        li=value(base+(0x4EE840 if game==5 else 0x4F3F40),'Q')
        row['game']=game;row['guns']=struct.unpack('<4h',read(lb+2,8))
        row['body']=struct.unpack('<5h',read(li+18,10)) if li else None
        row['flareAge']=value(lb+0x24,'h')
        vr=mod+0x10b530
        row['head']=struct.unpack('<12f',read(vr+296,48))
        row['hand']=struct.unpack('<12f',read(vr+396,48))
        row['tracked']=list(read(vr+492,2))
        row['poseSerial']=value(vr+8,'Q')
        rows.append(row);time.sleep(.01)
    result=dict(flags=flags,samples=rows)
    Path('build/flare-live-'+str(int(time.time()))+'.json').write_text(json.dumps(result,indent=2));print(json.dumps(dict(flags=flags,samples=len(rows),first=rows[:3],last=rows[-3:],counts=sorted(set(x['count'] for x in rows)),guns=sorted(set(tuple(x['guns']) for x in rows))),indent=2))
finally:k.CloseHandle(h)
