"""Verify TR4 floor-lever handler, calls and handle point in both local binaries."""
from pathlib import Path
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_IMM,X86_OP_MEM,X86_REG_RIP
root=Path(__file__).resolve().parents[1]
retail=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered\4\tomb4.dll')
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for path,stamp,collision,test,move,animate,items in [
    (root/'PDB/tomb4.dll',0x696B4999,0x8CAA0,0xE510,0xEB70,0x120E0,0x699CA0),
    (retail,0x68C12FDA,0x8D4B0,0xE110,0xE770,0x11D10,0x69ABE0)]:
    pe=pefile.PE(str(path));assert pe.FILE_HEADER.TimeDateStamp==stamp
    assert pe.get_data(collision,5)==bytes.fromhex('48 89 5c 24 08')
    ends={e.struct.BeginAddress:e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION}
    end=ends[collision]
    while end in ends:end=ends[end]
    code=list(md.disasm(pe.get_data(collision,end-collision),collision))
    calls={i.operands[0].imm for i in code if i.mnemonic=='call' and i.operands[0].type==X86_OP_IMM}
    memory={i.address+i.size+o.mem.disp for i in code for o in i.operands if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP}
    immediates={o.imm&0xffffffff for i in code for o in i.operands if o.type==X86_OP_IMM}
    assert {test,move,animate}<=calls and items in memory
    assert (-700&0xffffffff) in immediates and 324 in immediates
    print(f'PASS: {path}: floor hook, native activation calls, handle offset -700, Lara rail animation 324')
