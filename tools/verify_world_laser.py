"""Check read-only laser bindings and final hit coordinates in all supported DLLs."""
from pathlib import Path
import re
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

root = Path(__file__).resolve().parents[1]
source = (root / 'src/WorldLaserNative.h').read_text()
rows = [tuple(int(v, 16) for v in re.findall(r'0x[0-9A-F]+', line))
        for line in source.splitlines() if line.strip().startswith('{0x')]
paths = ['PDB/tomb4.dll', 'PDB/tomb5.dll', 'retail/tomb4.dll', 'retail/tomb5.dll']
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True
for path, (stamp, sight, active, light, sc, ac, lc, los, objects, los_call, objects_call) in zip(paths, rows):
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp
    for at, target, prefix, displacement, size in [
        (sc, sight, bytes.fromhex('44 39'), 3, 7),
        (ac, active, bytes.fromhex('c6 05'), 2, 7),
        (lc, light, bytes.fromhex('66 c7 05'), 3, 9),
    ]:
        assert data[at:at+len(prefix)] == prefix, (path, hex(at))
        assert at + size + struct.unpack_from('<i', data, at+displacement)[0] == target
    assert data[ac+6] == 1 and data[lc+7:lc+9] == b'\x01\x02'
    for call, target in ((los_call, los), (objects_call, objects)):
        assert data[call] == 0xe8 and call+5+struct.unpack_from('<i',data,call+1)[0] == target
    # The attachment bits are in lara.inv, not the separate loose-sight flag.
    source_game=(root/'src/GameDll.cpp').read_text()
    lara=int(re.search(rf'0x{stamp:08X}, L"tomb[45]\.dll", "[^"]+",\s*/\* lara\s*\*/\s*(0x[0-9A-F]+)',source_game)[1],16)
    for field in (0x163,0x166):
        matches=[m.start() for m in re.finditer(b'\xf6\x05....\x04',data,re.DOTALL)
                 if m.start()+7+struct.unpack_from('<i',data,m.start()+2)[0]==lara+field]
        assert matches,(path,hex(field),'native combined laser bit')
    # The native hit routine stores all source and final-hit components after
    # initializing the spotlight. Its destination argument is NOT this result.
    writes = set()
    for ins in decoder.disasm(data[lc:lc+600], lc):
        if ins.mnemonic == 'ret':
            break
        if ins.mnemonic == 'movss':
            op = ins.operands[0]
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                writes.add(ins.address + ins.size + op.mem.disp - light)
    assert {0x14, 0x18, 0x1c, 0x2c, 0x30, 0x34} <= writes, (path, writes)
    print(f'PASS: {path}: laser activation and collision-clipped world endpoints verified')
assert len(rows) == len(paths)
