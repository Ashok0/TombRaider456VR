"""Check inventory replacement call ordering and hook bytes in all TR4/5 builds."""
from pathlib import Path
import re
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM

root = Path(__file__).resolve().parents[1]
source = (root / 'src/InventoryScene.cpp').read_text()
table = source.split('constexpr Build builds[]={', 1)[1].split('};', 1)[0]
rows = [tuple(int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', row))
        for row in re.findall(r'\{([^{}]+)\}', table)]
assert len(rows) == 4
layouts = (root / 'src/GameDll.cpp').read_text()
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True
for path, (stamp, background, rooms, flush, app, home) in zip(
        ('PDB/tomb4.dll', 'PDB/tomb5.dll', 'retail/tomb4.dll', 'retail/tomb5.dll'), rows):
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp, path
    entries = {e.struct.BeginAddress: e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION}
    functions = {}
    for entry in entries.values():
        # MSVC splits functions into chained unwind regions when registers are
        # saved mid-function. Resolve their owner before checking call order.
        owner = entry
        while data[owner.UnwindData] >> 3 & 4:
            count = data[owner.UnwindData+2]
            chain = owner.UnwindData+4+((count+1)&~1)*2
            owner = entries[struct.unpack_from('<I', data, chain)[0]]
        functions[owner.BeginAddress] = max(functions.get(owner.BeginAddress, 0), entry.EndAddress)
    def instructions(start):
        assert start in functions, (path, hex(start), 'function boundary')
        return list(decoder.disasm(data[start:functions[start]], start))
    def calls(code):
        return [i.operands[0].imm for i in code if i.mnemonic == 'call' and i.operands[0].type == X86_OP_IMM]
    layout = layouts.split(f'0x{stamp:08X}', 1)[1].split('},', 1)[0]
    def rva(label):
        return int(re.search(r'/\* '+re.escape(label)+r'\s*\*/ (0x[0-9A-Fa-f]+)', layout)[1], 16)
    expected = bytes.fromhex('48 83 ec 48 48 8b 05') + struct.pack('<i', app-background-11)
    assert data[background:background+11] == expected, (path, 'background hook bytes')
    stolen = list(decoder.disasm(expected, background))
    assert [i.size for i in stolen] == [4, 7]
    assert stolen[1].disp_offset == 3 and stolen[1].disp_size == 4
    assert stolen[1].address+stolen[1].size+stolen[1].disp == app
    assert data[rooms:rooms+5] == bytes.fromhex('48 89 5c 24')+bytes([home])
    assert data[flush:flush+7] == bytes.fromhex('40 53 55 56 57 41 54')
    scene = instructions(rooms)
    assert rva('PrintRoomsList') in calls(scene), (path, 'full scene renderer')
    instructions(flush)
    instructions(background)
    # Find callers in executable sections, then decode only their enclosing
    # functions. Inventory must initialize the camera/target immediately before
    # this background call, including the nested native options loop.
    candidates = set()
    starts = sorted(functions)
    import bisect
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        start = section.VirtualAddress
        blob = data[start:start+section.Misc_VirtualSize]
        for match in re.finditer(b'\xe8', blob):
            at = start+match.start()
            if at+5+struct.unpack_from('<i', data, at+1)[0] == background:
                index = bisect.bisect_right(starts, at)-1
                if index >= 0 and at < functions[starts[index]]:
                    candidates.add(starts[index])
    ordered = 0
    for caller in candidates:
        code = instructions(caller)
        for index, ins in enumerate(code):
            if ins.mnemonic != 'call' or ins.operands[0].type != X86_OP_IMM or ins.operands[0].imm != background:
                continue
            if index and code[index-1].mnemonic == 'call' and code[index-1].operands[0].type == X86_OP_IMM:
                initializer = code[index-1].operands[0].imm
                # This is the actual initializer containing our FP camera hook,
                # not a coincidentally adjacent call to an unrelated routine.
                if initializer in functions and initializer < rva('w2v scene return') < functions[initializer]:
                    ordered += 1
    assert ordered >= 2, (path, 'inventory/options initialize scene before background', ordered)
    # Flushing submits queued geometry, without presenting a frame or ticking
    # gameplay. Present is the app vtable +0x88 call in InventoryDrawPhase.
    assert not any(i.mnemonic == 'call' and '+ 0x88]' in i.op_str
                   for i in scene+instructions(flush)), (path, 'unexpected nested present')
    assert rva('AnimateLara') not in calls(scene)+calls(instructions(flush))
    print(f'{path}: hook relocation, native draw/flush entries and {ordered} initialize-before-background paths verified')
