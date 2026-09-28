"""Verify the production Action icon hook/table against all four native DLLs."""
from pathlib import Path
import re
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

root = Path(__file__).resolve().parents[1]
source = (root / 'src/FirstPerson.cpp').read_text()
table = source.split('constexpr ActionIconDll kActionIconDlls[] = {', 1)[1].split('};', 1)[0]
rows = [tuple(int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', row))
        for row in re.findall(r'\{([^{}]+)\}', table)]
assert len(rows) == 4
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True
for path, row in zip(('PDB/tomb4.dll', 'PDB/tomb5.dll', 'retail/tomb4.dll', 'retail/tomb5.dll'), rows):
    stamp, draw, points, count, matrix, persp, cx, cy, near, far = row
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp, path
    # MSVC splits this function into seven contiguous unwind regions.
    regions = [e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION
               if draw <= e.struct.BeginAddress < draw+1107]
    end = regions[-1].EndAddress
    assert len(regions) == 7 and regions[0].BeginAddress == draw and end-draw in (1107,1108), path
    assert all(a.EndAddress == b.BeginAddress for a, b in zip(regions, regions[1:])), path
    assert data[draw:draw+5] == bytes.fromhex('4c 8b dc 41 56'), path
    instructions = list(decoder.disasm(data[draw:end], draw))
    assert instructions[-1].mnemonic == 'ret', path
    assert sum(i.size for i in instructions[:2]) == 5
    refs = {i.address+i.size+op.mem.disp for i in instructions for op in i.operands
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP}
    # Native array iterator starts at .y; matrix copied in three 16-byte rows.
    for address in (points+4, count, matrix, matrix+16, matrix+32, persp, cx, cy, near, far):
        assert address in refs, (path, hex(address))
    for address, size in ((points,240),(count,4),(matrix,48),(persp,4),(cx,4),(cy,4),(near,4),(far,4)):
        section = next(s for s in pe.sections if s.VirtualAddress <= address and
                       address+size <= s.VirtualAddress+s.Misc_VirtualSize)
        assert section.Characteristics & 0x80000000, (path, hex(address))
    # Still gated by native Action Indicators setting (bit 10) and menu flags.
    assert any(i.mnemonic == 'bt' and i.op_str == 'r14d, 0xa' for i in instructions)
    assert sum(i.mnemonic == 'comiss' for i in instructions) == 2
    print(f'{path}: Action icon prologue, projection data, array layout and native gates verified')
