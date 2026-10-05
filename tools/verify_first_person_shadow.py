"""Check the first-person HD shadow hook against all four supported TR4/5 DLLs."""
from pathlib import Path
import re
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
for path, (stamp, draw, returned) in zip(
        ('PDB/tomb4.dll', 'PDB/tomb5.dll', 'retail/tomb4.dll', 'retail/tomb5.dll'), rows):
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp, path
    layout = layouts.split(f'0x{stamp:08X}', 1)[1].split('},', 1)[0]
    def rva(label):
        return int(re.search(r'/\* '+re.escape(label)+r'\s*\*/ (0x[0-9A-Fa-f]+)', layout)[1], 16)
    joint = rva('GetJointAbs...')
    floor, height, lara = rva('GetFloor'), rva('GetHeight'), rva('lara_item')
    # Exact executable function start and a complete position-independent
    # instruction for the 5-byte trampoline, on every supported binary.
    assert any(e.struct.BeginAddress == draw for e in pe.DIRECTORY_ENTRY_EXCEPTION), path
    assert any(e.struct.BeginAddress == joint for e in pe.DIRECTORY_ENTRY_EXCEPTION), path
    prologue = next(decoder.disasm(data[joint:joint+16], joint))
    assert prologue.size == 5 and prologue.bytes == bytes.fromhex('48 89 5c 24 08'), path
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
    print(f'{path}: shadow torso caller, joint-7 argument, hook prologue and subsequent floor/height sampling verified')
