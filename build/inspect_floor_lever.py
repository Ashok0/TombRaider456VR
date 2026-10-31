"""Read-only snapshot of nearby TR4 objects; never writes process memory."""
import ctypes as C, ctypes.wintypes as W, struct, sys, json
from pathlib import Path
pid,base=map(int,sys.argv[1:3])
k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
k.ReadProcessMemory.restype=W.BOOL
k.CloseHandle.argtypes=[W.HANDLE]
h=k.OpenProcess(0x10|0x1000,False,pid)
if not h: raise C.WinError(C.get_last_error())
def read(at,size):
    buf=C.create_string_buffer(size);got=C.c_size_t()
    if not k.ReadProcessMemory(h,at,buf,size,C.byref(got)) or got.value!=size: raise C.WinError(C.get_last_error())
    return buf.raw
def value(at,fmt): return struct.unpack('<'+fmt,read(at,struct.calcsize('<'+fmt)))[0]
try:
    pe=value(base+0x3c,'I');stamp=value(base+pe+8,'I')
    assert stamp==0x68C12FDA,hex(stamp)
    lara=value(base+0x4f3f40,'Q');body=read(lara,9416);pos=struct.unpack_from('<iii',body,96)
    count=value(base+0x661754,'i');assert 0<count<32768
    items=value(base+0x69abe0,'Q');nearest=[]
    for i in range(count):
        item=read(items+i*9416,112);obj,state=struct.unpack_from('<hh',item,16)
        if obj<0:continue
        xyz=struct.unpack_from('<iii',item,96);dist=sum((a-b)**2 for a,b in zip(pos,xyz))
        if dist>2048**2:continue
        definition=read(base+0x69ac00+obj*3792,80)
        nearest.append(dict(index=i,object=obj,state=state,type=struct.unpack_from('<h',item,48)[0],
            animation=struct.unpack_from('<h',item,24)[0],frame=struct.unpack_from('<h',item,26)[0],
            room=struct.unpack_from('<h',item,28)[0],position=xyz,yaw=struct.unpack_from('<h',item,110)[0],
            distance=round(dist**.5),collision=hex(struct.unpack_from('<Q',definition,56)[0]-base),
            control=hex(struct.unpack_from('<Q',definition,24)[0]-base),joints=struct.unpack_from('<h',definition)[0],
            animationBase=struct.unpack_from('<h',definition,66)[0]))
    nearest.sort(key=lambda x:x['distance'])
    floor=next((x for x in nearest if x['object']==318),None)
    animations=[]
    if floor:
        anims=value(base+0x69abe8,'Q');changes=value(base+0x661108,'Q');ranges=value(base+0x6643c8,'Q');commands=value(base+0x661118,'Q')
        for i in range(floor['animationBase'],floor['animationBase']+4):
            a=read(anims+i*48,48);fields=struct.unpack_from('<hhiiii8h',a,8)
            record=dict(index=i,fields=fields,changes=[],commands=[])
            for c in range(fields[10]):
                change=struct.unpack('<hhh',read(changes+(fields[11]+c)*6,6))
                record['changes'].append(dict(fields=change,ranges=[struct.unpack('<hhhh',read(ranges+(change[2]+r)*8,8)) for r in range(change[1])]))
            record['commands']=list(struct.unpack('<'+str(fields[12]*3)+'h',read(commands+fields[13]*2,fields[12]*6))) if fields[12] else []
            animations.append(record)
    result=dict(laraPosition=pos,laraState=struct.unpack_from('<h',body,18)[0],laraAnimation=struct.unpack_from('<h',body,24)[0],nearby=nearest,floorAnimations=animations)
    Path(__file__).with_name('floor-lever-live.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
