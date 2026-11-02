"""Read-only movement capture. Never writes or executes code in the game."""
import ctypes as C, ctypes.wintypes as W, struct, sys, json, time, collections
from pathlib import Path
pid, mod, tr4, tr5 = map(int,sys.argv[1:5])
duration=float(sys.argv[5]) if len(sys.argv)>5 else 35
k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];k.ReadProcessMemory.restype=W.BOOL
k.CloseHandle.argtypes=[W.HANDLE]
h=k.OpenProcess(0x10|0x1000,False,pid)
if not h:raise C.WinError(C.get_last_error())
def read(at,size):
    buf=C.create_string_buffer(size);got=C.c_size_t()
    if not k.ReadProcessMemory(h,at,buf,size,C.byref(got)) or got.value!=size:raise C.WinError(C.get_last_error())
    return buf.raw
def val(at,fmt):return struct.unpack('<'+fmt,read(at,struct.calcsize('<'+fmt)))[0]
rows=[]
try:
    # The capture is for the exact currently deployed back-hop DLL.
    for base,stamp in [(tr4,0x68c12fda),(tr5,0x68c12fe9)]:
        pe=val(base+0x3c,'I');assert val(base+pe+8,'I')==stamp
    started=time.monotonic()
    while time.monotonic()-started<duration:
        heading_item=val(mod+0x12c0e8,'Q')
        item=heading_item
        native=None
        for game,base,lptr,lara,inp,analog,cam in [(4,tr4,0x4f3f40,0x4f3d80,0x664e40,0x664e00,0x664d80),(5,tr5,0x4ee840,0x4ee680,0x65ab80,0x65aae0,0x66d020)]:
            candidate=val(base+lptr,'Q')
            if candidate and (not heading_item or candidate==heading_item):
                item=candidate
                native=(game,base,lara,inp,analog,cam);break
        if native:
            game,base,lara,inp,analog,cam=native
            body=read(item,120)
            state,goal,required,clip,frame,room=struct.unpack_from('<6h',body,18)
            raw=struct.unpack('<2f',read(mod+0x12a718,8))
            row=dict(t=round(time.monotonic()-started,3),game=game,active=val(mod+0x12a247,'B'),heading=val(mod+0x129a97,'B'),manual=val(mod+0x129aba,'B'),rg=val(mod+0x129abe,'B'),shift=val(mod+0x129fa7,'B'),jump=val(mod+0x129b57,'B'),stick=[round(x,3) for x in raw],state=state,goal=goal,required=required,clip=clip,frame=frame,room=room,pos=list(struct.unpack_from('<3i',body,96)),yaw=struct.unpack_from('<h',body,110)[0],head=round(val(mod+0x12a5d4,'f'),3),input=hex(val(base+inp,'Q')),axes=list(struct.unpack('<4h',read(base+analog,8))),weapon=list(struct.unpack('<2h',read(base+lara+2,4))),flags=hex(val(base+lara+68,'I')),gravity=bool(val(item+0x1820,'I')&8),physical=val(mod+0x12a176,'B'),camera=val(base+cam+32,'i'))
            rows.append(row)
        time.sleep(.02)
finally:k.CloseHandle(h)
out=Path(__file__).with_name('capture-'+time.strftime('%Y%m%d-%H%M%S')+'.json')
out.write_text(json.dumps(rows,indent=2))
print('Saved',out,len(rows),'samples')
groups=collections.Counter((r['game'],r['active'],r['rg'],r['state'],r['goal'],r['clip'],r['required'],tuple(r['weapon']),r['input']) for r in rows)
for key,n in groups.most_common(18):print(n,key)
active=[r for r in rows if r['rg'] and r['stick'][1]<-.4]
for r in active[::max(1,len(active)//8)][:9]:print(json.dumps(r))
