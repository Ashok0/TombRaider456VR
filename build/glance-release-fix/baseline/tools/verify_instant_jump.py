"""Execute native TR4/5 compression and takeoff on private fixture memory.

Usage: python tools/verify_instant_jump.py <game-directory>
Loads PDB-build code and installed animation tables, without touching a game
process. Compares skipped preparation with the complete native animation path.
"""
import ctypes as C
import math
from pathlib import Path
import struct
import subprocess
import sys
import pefile

ROOT = Path(__file__).resolve().parents[1]
kernel = C.WinDLL('kernel32', use_last_error=True)
kernel.VirtualAlloc.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.c_uint32]
kernel.VirtualAlloc.restype = C.c_void_p
kernel.VirtualFree.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32]
kernel.FlushInstructionCache.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
clear = True
floor_calls = 0

@C.CFUNCTYPE(C.c_int16, C.c_void_p, C.c_int16, C.c_int32)
def floor_front(item, angle, distance):
    global floor_calls
    floor_calls += 1
    assert distance == 256
    return 0 if clear else -1024

@C.CFUNCTYPE(C.c_double, C.c_double, C.c_double)
def atan2(y, x):
    return math.atan2(y, x)

@C.CFUNCTYPE(C.c_double, C.c_double, C.c_double)
def fmod(x, y):
    return math.fmod(x, y)

@C.CFUNCTYPE(C.c_float, C.c_float)
def sinf(x):
    return math.sin(x)

@C.CFUNCTYPE(C.c_float, C.c_float)
def cosf(x):
    return math.cos(x)

@C.CFUNCTYPE(C.c_int, C.c_int, C.c_void_p, C.c_int)
def sound(*args):
    return 0

for game in (4, 5):
    path = ROOT / f'PDB/tomb{game}.dll'
    output = subprocess.check_output([sys.executable, str(ROOT/'tools/pdbdump.py'), str(path)], text=True)
    symbols = {}
    for line in output.splitlines():
        fields = line.split(None, 3)
        if len(fields)==4 and fields[0].startswith('0x'):
            symbols[fields[3]] = int(fields[0],16)
    pe = pefile.PE(str(path))
    data = pe.get_memory_mapped_image()
    image = kernel.VirtualAlloc(None, pe.OPTIONAL_HEADER.SizeOfImage, 0x3000, 0x40)
    assert image
    try:
        C.memmove(image, data, len(data))
        for name, callback in [('LaraFloorFront',floor_front),('atan2',atan2),('fmod',fmod),
                               ('sinf',sinf),('cosf',cosf),('SoundEffect',sound)]:
            patch = b'\x48\xb8'+struct.pack('<Q',C.cast(callback,C.c_void_p).value)+b'\xff\xe0'
            C.memmove(image+symbols[name],patch,len(patch))
        kernel.FlushInstructionCache(C.c_void_p(-1),image,len(data))
        app=C.create_string_buffer(4096)
        item=C.create_string_buffer(9416)
        def global_field(name,kind=C.c_int16,offset=0):
            return kind.from_address(image+symbols[name]+offset)
        def field(offset,kind=C.c_int16):
            return kind.from_buffer(item,offset)
        global_field('app',C.c_void_p).value=C.addressof(app)
        global_field('lara_item',C.c_void_p).value=C.addressof(item)
        C.c_uint32.from_buffer(app,0x9e4).value=2
        compress=C.CFUNCTYPE(None,C.c_void_p,C.c_void_p)(image+symbols['lara_as_compress'])
        animate=C.CFUNCTYPE(None,C.c_void_p)(image+symbols['AnimateLara'])
        # One representative level; verify_jump_priority checks every PDP.
        for asset in sorted((Path(sys.argv[1])/str(game)/'DATA').glob('*.PDP')):
            raw=asset.read_bytes()
            if struct.unpack_from('<I',raw)[0]>104:
                break
        tables={};cursor=0
        for name,size in [('anims',40),('changes',6),('ranges',8),('commands',2)]:
            count,=struct.unpack_from('<I',raw,cursor);cursor+=4
            tables[name]=raw[cursor:cursor+count*size];cursor+=count*size
        buffers={name:C.create_string_buffer(value) for name,value in tables.items() if name!='anims'}
        # Route footstep effects to the inert sound callback. Native FX tables
        # contain absolute pointers; the private image is not a loaded module.
        offset=0
        while offset<len(tables['commands']):
            opcode=struct.unpack_from('<h',tables['commands'],offset)[0]
            if opcode==6: C.c_int16.from_buffer(buffers['commands'],offset).value=5
            offset+=2+2*{1:3,2:2,3:0,4:0,5:2,6:2}[opcode]
        runtime=C.create_string_buffer(len(tables['anims'])//40*48)
        for i in range(len(tables['anims'])//40):
            C.memmove(C.addressof(runtime)+i*48+8,tables['anims'][i*40+4:(i+1)*40],36)
        buffers['anims']=runtime
        for name,buffer in buffers.items():
            global_field(name,C.c_void_p).value=C.addressof(buffer)
        def metadata(clip):
            return struct.unpack_from('<8h',tables['anims'],clip*40+24)
        def reset(clip,state,direction,armed,yaw):
            C.memset(item,0,len(item));C.memset(image+symbols['lara'],0,448)
            field(18).value=field(20).value=state
            field(24).value=clip;field(26).value=metadata(clip)[0]
            field(38).value=1000;field(0x6e).value=yaw
            global_field('lara',offset=2).value=4 if armed else 0
            global_field('lara',offset=4).value=1
            global_field('lara',C.c_int32,0x144).value=-1
            global_field('lara',offset=0xde).value=yaw
            global_field('input',C.c_uint64).value=direction|0x10
            # FP suppresses unarmed direction rotation during compression.
            global_field('analogInput').value=global_field('analogInput',offset=2).value=0
            global_field('analogInput',offset=4).value=yaw
        checks=0
        for armed in (False,True):
            for yaw in (0,7300,-28000):
                for direction in (0,1,2,4,8):
                    for clear in (False,True):
                        results=[]
                        for skip in (False,True):
                            reset(73,15,direction,armed,yaw)
                            floor_calls=0;compress(item,None)
                            goal=field(20).value
                            takeoff={15:91,3:76,25:74,26:80,27:78}[goal]
                            assert goal==({0:15,1:3,2:25,4:27,8:26}[direction] if clear else 15)
                            assert floor_calls==(1 if direction else 0)
                            if skip:
                                field(18).value=28 if goal==15 else goal
                                field(24).value=takeoff;field(26).value=metadata(takeoff)[1]
                            ticks=0
                            while not field(0x1820,C.c_uint32).value&8:
                                animate(item);ticks+=1
                                assert ticks<20,(game,'takeoff timeout')
                            results.append(tuple(field(o).value for o in (18,20,24,26,34,36))+(field(100,C.c_int32).value,))
                            assert (ticks==1) if skip else (ticks>8)
                            assert field(0x6e).value==yaw
                        assert results[0]==results[1],(game,direction,armed,clear,results)
                        checks+=1
        for clip in (16,18):
            results=[]
            for skip in (False,True):
                reset(clip,3,1,False,0)
                if skip: field(26).value=metadata(clip)[1]
                ticks=0
                while not field(0x1820,C.c_uint32).value&8:
                    animate(item);ticks+=1
                    assert ticks<10
                results.append(tuple(field(o).value for o in (18,20,24,26,34,36)))
                assert ticks==(1 if skip else 7)
            assert results[0]==results[1]
            checks+=1
        print(f'TR{game}: {checks} native takeoffs verified: one animation tick, original airborne clips/velocity/gravity, direction and floor rejection preserved',flush=True)
    finally:
        kernel.VirtualFree(image,0,0x8000)
