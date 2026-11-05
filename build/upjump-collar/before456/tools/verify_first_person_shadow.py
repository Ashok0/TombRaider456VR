"""Verify native shadow pass, floor anchor and hair origin in all four TR4/5 DLLs."""
from pathlib import Path
import re
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

root = Path(__file__).resolve().parents[1]
source = (root / 'src/FirstPerson.cpp').read_text()
table = source.split('constexpr ShadowDll kShadowDlls[] = {', 1)[1].split('};', 1)[0]
rows = [tuple(int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', row))
        for row in re.findall(r'\{([^{}]+)\}', table)]
assert len(rows) == 4
layouts = (root / 'src/GameDll.cpp').read_text()
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True
for path, (stamp, draw, returned, render_pass, view_rel, cast) in zip(
        ('PDB/tomb4.dll', 'PDB/tomb5.dll', 'retail/tomb4.dll', 'retail/tomb5.dll'), rows):
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp, path
    layout = layouts.split(f'0x{stamp:08X}', 1)[1].split('},', 1)[0]
    def rva(label):
        return int(re.search(r'/\* '+re.escape(label)+r'\s*\*/ (0x[0-9A-Fa-f]+)', layout)[1], 16)
    joint = rva('GetJointAbs...')
    floor, height, lara = rva('GetFloor'), rva('GetHeight'), rva('lara_item')
    # Verify the native floor-shadow helper on every supported binary.
    assert any(e.struct.BeginAddress == draw for e in pe.DIRECTORY_ENTRY_EXCEPTION), path
    assert any(e.struct.BeginAddress == joint for e in pe.DIRECTORY_ENTRY_EXCEPTION), path
    prologue = next(decoder.disasm(data[joint:joint+16], joint))
    assert prologue.size == 5 and prologue.bytes == bytes.fromhex('48 89 5c 24 08'), path
    # The floor anchor must remain unhooked: camera-fit translation no longer
    # belongs to either the floor shadow or the projected character silhouette.
    assert 'g_hShadowJoint' not in source and 'Detour_ShadowJoint' not in source
    # DrawToShadow constructs the light projection before the native scene
    # camera is replaced. Verify the real hook bytes and origin-building path.
    signature_name = 'tr5' if 'tomb5' in path else 'tr4Retail' if 'retail' in path else 'tr4Debug'
    signature = source.split(f'const uint8_t {signature_name}[]={{',1)[1].split('}',1)[0]
    expected = bytes(int(x,16) for x in re.findall(r'0x[0-9A-Fa-f]+',signature))
    assert data[cast:cast+len(expected)] == expected, (path, 'caster prologue')
    stolen = list(decoder.disasm(expected,cast))
    assert sum(i.size for i in stolen) == len(expected), path
    assert all(not any(op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP for op in i.operands)
               for i in stolen), path
    assert any(e.struct.BeginAddress == cast for e in pe.DIRECTORY_ENTRY_EXCEPTION), path
    caster = list(decoder.disasm(data[cast:cast+1100],cast))
    assert any(i.mnemonic=='mov' and i.op_str.endswith(', 4') and any(
        op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP and
        i.address+i.size+op.mem.disp==render_pass for op in i.operands) for i in caster), path
    assert any(i.mnemonic=='call' and i.op_str=='qword ptr [rax + 0x2f0]' for i in caster), path
    assert sum(i.mnemonic=='sub' and i.op_str.startswith('eax,') for i in caster) >= 3, path
    # The caller builds the shadow BEFORE the scene initializer that contains
    # the mod's camera-replacement return site. Check this in retail too.
    scene_return = rva('w2v scene return')
    ordered = False
    for offset in range(len(data)-12):
        if data[offset] != 0xe8:
            continue
        target = offset+5+struct.unpack_from('<i',data,offset+1)[0]
        if target != cast:
            continue
        tail = list(decoder.disasm(data[offset+5:offset+14],offset+5))
        if len(tail)>=2 and tail[0].mnemonic=='xor' and tail[0].op_str=='ecx, ecx' and tail[1].mnemonic=='call':
            initializer = tail[1].operands[0].imm
            ordered |= initializer < scene_return < initializer+2000
    assert ordered, (path, 'shadow precedes first-person scene-camera substitution')
    creature = rva('DrawCreatureHD')
    skin = list(decoder.disasm(data[creature:creature+1000], creature))
    refs = [i for i in skin if i.mnemonic == 'mov' and i.op_str.startswith('edx, dword ptr [rip')
            and i.address+i.size+i.operands[1].mem.disp == render_pass]
    assert len(refs) == 2, (path, 'native global render-pass reads')
    assert any(i.mnemonic == 'cmp' and i.op_str == 'edx, 4' for i in skin), path
    # Hair is separate from GetJoints and adds view_rel to every strand's
    # translation. Verify all three coordinates against its actual code.
    hair = rva('DrawHair')
    hair_code = list(decoder.disasm(data[hair:hair+1200], hair))
    hair_refs = {i.address+i.size+op.mem.disp for i in hair_code for op in i.operands
                 if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP}
    assert all(view_rel+offset in hair_refs for offset in (0, 4, 8)), (path, 'hair render origin')
    section = next(s for s in pe.sections if s.VirtualAddress <= render_pass < s.VirtualAddress+s.Misc_VirtualSize)
    assert section.Characteristics & 0x80000000, (path, 'render pass is writable data')
    instructions = list(decoder.disasm(data[draw:returned+0x50], draw))
    call = next(i for i in instructions if i.address+i.size == returned)
    assert call.mnemonic == 'call' and call.size == 5 and call.operands[0].imm == joint, path
    before = [i for i in instructions if i.address < call.address]
    assert any(i.mnemonic == 'cmp' and any(
        op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP and
        i.address+i.size+op.mem.disp == lara for op in i.operands) for i in before), path
    assert any((i.mnemonic, i.op_str) in (('lea', 'r8d, [rax + 7]'), ('mov', 'r8d, 7'))
               for i in before[-7:]), path
    # Returned point is subsequently sampled against native floor geometry.
    calls = [i.operands[0].imm for i in instructions if i.address >= returned and i.mnemonic == 'call']
    assert calls[:2] == [floor, height], (path, calls)
    print(f'{path}: shadow torso caller, joint-7 argument, caster hook, shadow-before-scene ordering, native pass and untouched floor/hair paths verified')
