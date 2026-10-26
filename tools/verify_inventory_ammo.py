"""Execute the real TR4/5 DoHotkeys code on isolated fixture memory.

No game process or save is touched. PE bytes are mapped privately without DLL
initialization; only memset/sprintf are replaced with test callbacks. This tests
actual ammo mutations, rather than assuming BACK changes ammo while armed.
"""
from pathlib import Path
import ctypes as C
import re
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP, X86_OP_IMM

root = Path(__file__).resolve().parents[1]
source = (root / 'src/Gamepad.cpp').read_text().split('namespace ammoinput {', 1)[1]
table = source.split('constexpr Build builds[]={', 1)[1].split('};', 1)[0]
rows = [tuple(int(x.strip(), 0) for x in row.split(','))
        for row in re.findall(r'\{([^{}]+)\}', table)]
layouts = (root / 'src/GameDll.cpp').read_text()
md = Cs(CS_ARCH_X86, CS_MODE_64);md.detail = True
kernel = C.WinDLL('kernel32', use_last_error=True)
kernel.VirtualAlloc.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.c_uint32]
kernel.VirtualAlloc.restype = C.c_void_p
kernel.VirtualFree.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32]
kernel.FlushInstructionCache.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
feedback = []
@C.CFUNCTYPE(C.c_void_p, C.c_void_p, C.c_int, C.c_size_t)
def memset(dest, value, count):
    C.memset(dest, value, count)
    return dest
@C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p, C.c_void_p)
def sprintf(dest, fmt, text):
    feedback.append(True)  # String contents/localization are not under test.
    return 0

for path, (stamp, hotkeys, app_rva, level_rva, saved_reg) in zip(
        ('PDB/tomb4.dll', 'PDB/tomb5.dll', 'retail/tomb4.dll', 'retail/tomb5.dll'), rows):
    pe = pefile.PE(str(root / path));data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp
    expected = bytes([0x40, saved_reg, 0x48, 0x83, 0xec, 0x30])
    assert data[hotkeys:hotkeys+6] == expected
    assert [i.size for i in md.disasm(expected, hotkeys)] == [2, 4]
    entries = {e.struct.BeginAddress:e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION}
    end = hotkeys
    for entry in entries.values():
        owner = entry
        while data[owner.UnwindData] >> 3 & 4:
            chain = owner.UnwindData+4+((data[owner.UnwindData+2]+1)&~1)*2
            owner = entries[struct.unpack_from('<I', data, chain)[0]]
        if owner.BeginAddress == hotkeys:
            end = max(end, entry.EndAddress)
    assert end > hotkeys
    code = list(md.disasm(data[hotkeys:end], hotkeys))
    layout = layouts.split(f'0x{stamp:08X}', 1)[1].split('},', 1)[0]
    def rva(label):
        return int(re.search(r'/\* '+re.escape(label)+r'\s*\*/ (0x[0-9A-Fa-f]+)', layout)[1],16)
    def ref(i):
        return next(i.address+i.size+op.mem.disp for op in i.operands
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP)
    counters = [ref(i) for i in code if i.mnemonic == 'mov' and i.op_str.endswith(', 0x1e')]
    assert len(counters) == 1
    counter = counters[0]
    sprintf_rva = None
    for index, ins in enumerate(code):
        if ins.mnemonic == 'mov' and ins.op_str.endswith(', 0x1e'):
            sprintf_rva = next(i.operands[0].imm for i in code[index:] if i.mnemonic == 'call')
    memset_rvas = {code[n+1].operands[0].imm for n,i in enumerate(code[:-1])
                  if i.mnemonic == 'mov' and i.op_str == 'r8d, 0x200' and code[n+1].mnemonic == 'call'}
    assert len(memset_rvas) == 1
    image = kernel.VirtualAlloc(None, pe.OPTIONAL_HEADER.SizeOfImage, 0x3000, 0x40)
    assert image
    try:
        C.memmove(image, data, len(data))
        def put(offset, fmt, value):
            blob=struct.pack(fmt,value);C.memmove(image+offset,blob,len(blob))
        for target, callback in [(sprintf_rva,sprintf),(next(iter(memset_rvas)),memset)]:
            patch=b'\x48\xb8'+struct.pack('<Q',C.cast(callback,C.c_void_p).value)+b'\xff\xe0'
            C.memmove(image+target,patch,len(patch))
        kernel.FlushInstructionCache(C.c_void_p(-1), image, len(data))
        app=C.create_string_buffer(4096);item=C.create_string_buffer(9416)
        put(app_rva,'<Q',C.addressof(app));put(rva('lara_item'),'<Q',C.addressof(item))
        game_mode=next(ref(i) for i in code if i.mnemonic=='cmp' and i.op_str.startswith('byte ptr [rip') and i.op_str.endswith(', 0'))
        put(game_mode,'B',0)  # fixture is gameplay, not the DLL's startup/title mode
        put(rva('BinocularRange'),'<i',0)
        # Modern controls avoid mesh-equipping paths; an already ready weapon
        # takes the same native cycle branch under either control scheme.
        C.c_int.from_buffer(app,0x880).value=5
        C.c_int.from_buffer(app,0x9e4).value=2
        app[0x9ed]=b'\x01'
        C.c_int16.from_buffer(item,0x12).value=2
        C.c_int16.from_buffer(item,0x26).value=1000
        native=C.CFUNCTYPE(None)(image+hotkeys)
        game=1 if level_rva else 0
        cases=[(4,1,0x162,2,1)]
        cases += [(5,4,0x165,3,1),(6,5,0x163,3,1)] if game==0 else [(5,0,0x164,3,11)]
        for gun,key,flags,variants,level in cases:
            for mode in (0,1):
                app[0x9ed]=bytes([mode])
                if level_rva: put(level_rva,'B',level)
                C.memset(image+rva('lara'),0,448)
                put(rva('lara')+2,'<h',4);put(rva('lara')+4,'<h',gun)
                for selected in range(variants):
                    for k in range(6): app[0x89c+k]=b'\0'
                    app[0x89c+key]=b'\x01'
                    before=1|4|(8<<selected)  # owned + attachment preserved
                    put(rva('lara')+flags,'B',before);put(counter,'<i',0)
                    feedback.clear();native()
                    actual=C.c_uint8.from_address(image+rva('lara')+flags).value
                    expected=1|4|(8<<((selected+1)%variants))
                    assert actual==expected, (path,gun,mode,selected,actual,expected)
                    assert feedback and C.c_int.from_address(image+counter).value==30
                    native()  # native cooldown rejects a repeated attempt
                    assert C.c_uint8.from_address(image+rva('lara')+flags).value==expected
                    assert len(feedback)==1
                # Same hotkey cannot cycle when the native gun is not ready.
                for status in (0,2,3):
                    put(rva('lara')+2,'<h',status);put(counter,'<i',0)
                    # Modern control mode exits before mesh changes for transitions.
                    app[0x9ed]=b'\x01'
                    before=C.c_uint8.from_address(image+rva('lara')+flags).value
                    native()
                    assert C.c_uint8.from_address(image+rva('lara')+flags).value==before
                    put(rva('lara')+2,'<h',4)
        print(f'{path}: exact hook bytes; native ammo cycles, wrap, cooldown, feedback and ready-state guards passed', flush=True)
    finally:
        kernel.VirtualFree(image,0,0x8000)

