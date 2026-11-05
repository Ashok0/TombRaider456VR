"""Audit native holster selectors, outfit gates and shipped attachment geometry."""
from pathlib import Path
import struct
import sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM

root = Path(__file__).resolve().parents[1]
for path, draw, stamp in (
    ('PDB/tomb4.dll', 0xC40F0, 0x696B4999),
    ('PDB/tomb5.dll', 0xB8C50, 0x696B499C),
    ('retail/tomb4.dll', 0xC4D20, 0x68C12FDA),
    ('retail/tomb5.dll', 0xB8FB0, 0x68C12FE9),
):
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp, path
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    code = list(decoder.disasm(data[draw:draw + 3100], draw))
    # The five adjacent signed-byte selectors are read, never written, in draw.
    for offset in range(0x508, 0x50d):
        readers = [i for i in code if i.mnemonic == 'movsx' and any(
            o.type == X86_OP_MEM and o.mem.disp == offset for o in i.operands)]
        assert readers, (path, hex(offset))
        assert not any(i.mnemonic == 'mov' and i.operands[0].type == X86_OP_MEM
                       and i.operands[0].mem.disp == offset for i in code), path
    for offset in (0x10, 0x14):
        assert any(i.mnemonic == 'cmp' and i.operands[0].type == X86_OP_MEM
                   and i.operands[0].mem.disp == offset for i in code), (path, 'outfit slot')
    assert any(i.mnemonic == 'mov' and any(o.type == X86_OP_MEM and o.mem.disp == 0x1c
               for o in i.operands) for i in code), (path, 'weapon mesh mode')

    def names(first, entries):
        text = data.index(first.encode() + b'\0')
        table = data.index(struct.pack('<Q', pe.OPTIONAL_HEADER.ImageBase + text))
        for index, expected in entries.items():
            address = struct.unpack_from('<Q', data, table + index * 8)[0] - pe.OPTIONAL_HEADER.ImageBase
            assert data[address:address + len(expected) + 1] == expected.encode() + b'\0', (path, index)

    names('HOLSTER_EMPTY', {0:'HOLSTER_EMPTY', 1:'HOLSTER_PISTOL', 2:'HOLSTER_UZI',
          3:'HOLSTER_DEAGLE', 4:'HOLSTER_REVOLVER', 5:'HOLSTER_SHOTGUN',
          6:'HOLSTER_GRENADE', 7:'HOLSTER_CROSSBOW', 8:'HOLSTER_CROSSBOW_LASER',
          9:'HOLSTER_HK', 10:'HOLSTER_GRAPPLE'})
    # This table starts at REST (native index 1); check all three outfit banks.
    names('HAND_BARE_REST', {0:'HAND_BARE_REST', 4:'HAND_BARE_PISTOL', 5:'HAND_BARE_UZI',
          30:'HAND_GLOVES_REST', 34:'HAND_GLOVES_PISTOL', 35:'HAND_GLOVES_UZI',
          60:'HAND_XRAY_REST', 64:'HAND_XRAY_PISTOL', 65:'HAND_XRAY_UZI'})
    print(f'{path}: selectors, outfit gates, independent hand and holster IDs verified')

assets = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered\4\ITEM')
for name in ('PISTOL', 'UZI', 'DEAGLE', 'REVOLVER', 'SHOTGUN', 'GRENADE', 'CROSSBOW', 'HK', 'GRAPPLE'):
    data = (assets / f'HOLSTER_{name}.TRM').read_bytes()
    assert data[:4] == b'TRM\x02'
    materials = struct.unpack_from('<I', data, 4)[0]
    offset = 8 + materials * 44
    tiles = struct.unpack_from('<I', data, offset)[0]
    offset += 4 + ((tiles + 1) // 2) * 4
    bones, indices, vertices = struct.unpack_from('<III', data, offset)
    assert bones == 0, (name, 'canonical joint skinning')
    offset += 12 + ((indices + 1) // 2) * 4
    points = {}
    for i in range(vertices):
        v = offset + i * 24
        points.setdefault(data[v + 16], []).append(struct.unpack_from('<fff', data, v))
    slots = ((1, (-90, 15, -20)), (4, (90, 15, -20))) if name in ('PISTOL', 'UZI') else (
        ((4, (90, 15, -20)),) if name in ('DEAGLE', 'REVOLVER') else ((7, (58, -210, -65)),))
    for bone, grip in slots:
        assert bone in points, (name, bone)
        for axis in range(3):
            assert min(p[axis] for p in points[bone]) <= grip[axis] <= max(p[axis] for p in points[bone]), (name, axis)
    print(f'HOLSTER_{name}: grab points inside expected joint attachment bounds')
