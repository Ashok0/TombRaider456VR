"""Validate native mirror hook boundaries/call semantics offline; never load DLLs."""
from pathlib import Path
import json
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from mirror_asset_audit import ROOT456

md = Cs(CS_ARCH_X86, CS_MODE_64)
records = []
for game, path, stamp, inner, mirror, plane in [
    (4, Path(r'C:\dev\PDB\tomb4.dll'), 0x696B4999, 0xC40F0, 0x650E0, 0x63DC00),
    (4, ROOT456/'4/tomb4.dll', 0x68C12FDA, 0xC4D20, 0x64E30, 0x63EB40),
    (5, Path(r'C:\dev\PDB\tomb5.dll'), 0x696B499C, 0xB8C50, 0, 0),
    (5, ROOT456/'5/tomb5.dll', 0x68C12FE9, 0xB8FB0, 0, 0),
]:
    pe = pefile.PE(str(path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp
    bounds = {f.struct.BeginAddress: f.struct.EndAddress for f in pe.DIRECTORY_ENTRY_EXCEPTION}
    assert inner in bounds
    ins = list(md.disasm(data[inner:inner+320], inner))
    if game == 4:
        neg = next(i for i, x in enumerate(ins) if (x.mnemonic, x.op_str) == ('neg', 'edx'))
        carry = next(i for i in range(neg+1, len(ins)) if ins[i].mnemonic == 'sbb')
        register = ins[carry].op_str.split(',')[0]
        assert ins[carry].op_str == f'{register}, {register}'
        mask = next(i for i in range(carry+1, len(ins))
                    if ins[i].mnemonic == 'and' and ins[i].op_str == f'{register}, 2')
        assert any(x.mnemonic == 'mov' and x.op_str == f'r8d, {register}' for x in ins[mask+1:])
    else:
        assert any(x.mnemonic == 'movsxd' and x.op_str.startswith('rdx,') for x in ins)
        assert any(x.mnemonic == 'xor' and x.op_str == 'r8d, r8d' for x in ins)
    record = dict(game=game, timestamp=hex(stamp), innerDraw=hex(inner), flagSelectsJointPass2=game == 4)
    if mirror:
        assert bounds[mirror] - mirror == 2780
        stolen = data[mirror:mirror+10]
        prologue = list(md.disasm(stolen, mirror))
        assert len(prologue) == 2 and sum(x.size for x in prologue) == 10
        assert stolen[:6] == bytes.fromhex('48 83 ec 28 8b 05')
        assert mirror + 10 + struct.unpack_from('<i', stolen, 6)[0] == plane
        body = list(md.disasm(data[mirror:bounds[mirror]], mirror))
        call_index = next(i for i, x in enumerate(body) if x.mnemonic == 'call')
        assert any(x.mnemonic == 'mov' and x.op_str == 'edx, 1' for x in body[call_index-4:call_index])
        # The first call is DrawLara(item,1); it has a real PE function boundary.
        assert int(body[call_index].op_str, 16) in bounds
        source = Path('src/FirstPerson.cpp').read_text()
        expected = '{' + f'0x{stamp:08X},0x{mirror:X},' + '{' + ','.join(f'0x{x:02x}' for x in stolen) + '}}'
        assert expected in ''.join(source.split()), 'Production native mirror address/prologue differs'
        record.update(mirrorDraw=hex(mirror), prologue=stolen.hex(), planeGlobal=hex(plane), stolenBytes=10)
    records.append(record)
Path('build/mirror/native-hook-audit.json').write_text(json.dumps(records, indent=2)+'\n')
print('PASS native TR4 mirror boundaries/RIP relocation and distinct TR4/5 draw signatures in all four supported DLLs')
