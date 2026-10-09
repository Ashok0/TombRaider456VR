"""Check monkey-bar state IDs and control routines in debug and retail TR4/5.
Uses bundled PDB binaries as the named reference; reads binaries only.
Run from the repository root: python tools/verify_monkey_controls.py
"""
import re
import struct
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_GRP_JUMP, CS_GRP_CALL
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

# State -> (PDB RVA, symbol size), verified with pdbdump.py.
REFERENCE = {
    4: {75:(0x4e690,175),76:(0x4ebb0,540),77:(0x4efd0,76),78:(0x4f120,76),
        79:(0x4f430,455),82:(0x4f270,52),83:(0x4f3f0,52)},
    5: {75:(0x4cf40,175),76:(0x4d460,540),77:(0x4d880,76),78:(0x4d9d0,76),
        79:(0x4dce0,455),82:(0x4db20,52),83:(0x4dca0,52)},
}
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for game, routines in REFERENCE.items():
    ref=pefile.PE(f'PDB/tomb{game}.dll');reference=ref.get_memory_mapped_image()
    patterns={}
    for state,(rva,size) in routines.items():
        code=reference[rva:rva+size];mask=[False]*size
        for ins in md.disasm(code,ref.OPTIONAL_HEADER.ImageBase+rva):
            offset=ins.address-ref.OPTIONAL_HEADER.ImageBase-rva
            fields=[]
            if any(op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP for op in ins.operands):
                fields.append((ins.disp_offset,ins.disp_size))
            if ins.group(CS_GRP_JUMP) or ins.group(CS_GRP_CALL):
                fields.append((ins.imm_offset,ins.imm_size))
            for start,length in fields:
                for i in range(offset+start,offset+start+length): mask[i]=True
        patterns[state]=re.compile(b''.join(b'.' if wild else re.escape(bytes([byte])) for byte,wild in zip(code,mask)),re.DOTALL)
    for folder in ('PDB','retail'):
        path=Path(folder)/f'tomb{game}.dll';pe=pefile.PE(str(path));data=pe.get_memory_mapped_image()
        base=pe.OPTIONAL_HEADER.ImageBase;targets={}
        for state,pattern in patterns.items():
            matches=list(pattern.finditer(data))
            assert len(matches)==1,(path,state,'non-unique function',len(matches))
            targets[state]=base+matches[0].start()
        first=struct.pack('<Q',targets[75]);tables=[]
        for match in re.finditer(re.escape(first),data):
            table=match.start()-75*8
            if table>=0 and all(struct.unpack_from('<Q',data,table+state*8)[0]==target for state,target in targets.items()):
                tables.append(table)
        assert len(tables)==1,(path,'monkey control state table mismatch',tables)
        print(f'{path}: all seven monkey control routines match; state table RVA {tables[0]:08X}')
