"""Sample input/animation timing without changing the running game."""
from pathlib import Path
import time,subprocess
exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
mod=int(sys.argv[3]);duration=float(sys.argv[4])
try:
    symbols={}
    out=subprocess.check_output([sys.executable,'tools/pdbdump.py','build/vr-options-release/TombRaiderVR.dll'],text=True,stderr=subprocess.DEVNULL)
    for line in out.splitlines():
        x=line.split()
        if len(x)==4 and x[1]=='data':symbols[x[3].split('::')[-1]]=int(x[0],16)
    def sym(n,fmt):return struct.unpack('<'+fmt,read(mod+symbols[n],struct.calcsize('<'+fmt)))
    rows=[];start=time.perf_counter()
    while time.perf_counter()-start<duration:
        lara=value(base+0x4ee840,'Q')
        rows.append(dict(t=round(time.perf_counter()-start,3),manual=sym('g_manualLocal','2f'),
            valid=sym('g_haveManualInput','B')[0],shifted=sym('g_shifted','B')[0],
            state=struct.unpack('<5h',read(lara+18,10)),xyz=struct.unpack('<iii',read(lara+96,12)),
            flags=value(base+0x4ee680+68,'I'),input=hex(value(base+0x65ab80,'Q')),
            gun=value(base+0x4ee680+2,'h'),lock=value(base+0x1b6b20,'i')))
        time.sleep(.015)
    Path('build/movement-live.json').write_text(json.dumps(rows,indent=2))
    events=[];previous=None
    for row in rows:
        key=(any(row['manual']),tuple(row['state'][:4]))
        if key!=previous:events.append(row);previous=key
    print(json.dumps(dict(samples=len(rows),events=events),indent=2))
finally:k.CloseHandle(h)
