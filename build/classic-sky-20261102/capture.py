"""Read-only sky counters/configuration in the running game."""
from pathlib import Path
import ctypes as C, ctypes.wintypes as W, struct, json, time
k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
k.ReadProcessMemory.restype=W.BOOL;k.CloseHandle.argtypes=[W.HANDLE]
h=k.OpenProcess(0x1010,False,58028)
if not h:raise C.WinError(C.get_last_error())
def read(at,fmt):
    n=struct.calcsize('<'+fmt);b=C.create_string_buffer(n);got=C.c_size_t()
    if not k.ReadProcessMemory(h,at,b,n,C.byref(got)) or got.value!=n:raise C.WinError(C.get_last_error())
    return struct.unpack('<'+fmt,b.raw)
try:
    exe=140698528841728;mod=140710259064832;tr4=140710235537408
    samples=[]
    for i in range(5):
        samples.append(dict(game=read(exe+0x17b3ac,'i')[0],flags=hex(read(exe+0x585580+2532,'I')[0]),
            level=read(exe+0x585580+1264,'i')[0],skyDraws=read(mod+0x12ce90,'I')[0],
            skyScope=read(mod+0x12cfbf,'B')[0],eye=read(mod+0x12d02c,'i')[0]))
        time.sleep(.25)
    Path(__file__).with_name('live-before.json').write_text(json.dumps(samples,indent=2))
    print(json.dumps(samples,indent=2))
finally:k.CloseHandle(h)
