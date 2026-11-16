"""Read-only check of native key/puzzle camera state contracts in four builds."""
from pathlib import Path
import struct
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_MEM,X86_OP_IMM,X86_REG_RIP

root=Path(__file__).resolve().parents[1]
rows=[('PDB/tomb4.dll',0x696b4999,0x748e0,0x74de0,0x4f2e40,260),
      ('PDB/tomb5.dll',0x696b499c,0x74010,0x744a0,0x4ee740,242),
      ('retail/tomb4.dll',0x68c12fda,0x754e0,0x759e0,0x4f3d80,260),
      ('retail/tomb5.dll',0x68c12fe9,0x75730,0x75bc0,0x4ee680,242)]
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for path,stamp,key,puzzle,lara,first in rows:
    pe=pefile.PE(str(root/path));data=pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp==stamp
    entries={e.struct.BeginAddress:e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION}
    def code(start):
        end=start
        for entry in entries.values():
            owner=entry
            while data[owner.UnwindData]>>3&4:
                at=owner.UnwindData+4+((data[owner.UnwindData+2]+1)&~1)*2
                owner=entries[struct.unpack_from('<I',data,at)[0]]
            if owner.BeginAddress==start:end=max(end,entry.EndAddress)
        assert end>start
        return list(md.disasm(data[start:end],start))
    def state_writes(insns):
        return {i.operands[1].imm for i in insns if i.mnemonic=='mov' and len(i.operands)==2 and
            i.operands[0].type==X86_OP_MEM and i.operands[0].mem.disp==18 and i.operands[0].size==2 and
            i.operands[1].type==X86_OP_IMM}
    keyCode=code(key);puzzleCode=code(puzzle)
    assert 42 in state_writes(keyCode) and {43,89}<=state_writes(puzzleCode)
    assert any(len(i.operands)==2 and ((i.mnemonic=='sub' and i.operands[1].type==X86_OP_IMM and
        i.operands[1].imm==first) or (i.mnemonic=='lea' and i.operands[1].type==X86_OP_MEM and
        i.operands[1].mem.disp==-first)) for i in puzzleCode)
    assert any(i.mnemonic=='neg' and i.op_str=='ax' for i in puzzleCode)
    assert any(i.mnemonic=='movzx' and len(i.operands)==2 and i.operands[1].type==X86_OP_MEM and
        i.operands[1].mem.disp==48 for i in puzzleCode)
    assert any(i.mnemonic=='mov' and i.operands[0].type==X86_OP_MEM and
        i.operands[0].mem.base==X86_REG_RIP and i.address+i.size+i.operands[0].mem.disp==lara+348
        for i in puzzleCode)
    print(f'PASS {path}: USEKEY=42, USEPUZZLE=43, custom state=89, negative OCB animation and InteractedItem')
