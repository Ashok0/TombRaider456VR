"""Audit the four native hand tables and the run/rest selection used by VRIK."""
from pathlib import Path
import re
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM

root=Path(__file__).resolve().parents[1]
source=(root/'src/FirstPerson.cpp').read_text()
table=source.split('constexpr UnarmedHandDll kUnarmedHandDlls[] = {',1)[1].split('};',1)[0]
rows=[tuple(int(x,16) for x in re.findall(r'0x[0-9A-Fa-f]+',row)) for row in re.findall(r'\{([^{}]+)\}',table)]
assert len(rows)==4
for (stamp,hands),(path,draw) in zip(rows,[('PDB/tomb4.dll',0xC40F0),('PDB/tomb5.dll',0xB8C50),
                                        ('retail/tomb4.dll',0xC4D20),('retail/tomb5.dll',0xB8FB0)]):
    pe=pefile.PE(str(root/path));data=pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp==stamp,path
    decoder=Cs(CS_ARCH_X86,CS_MODE_64);decoder.detail=True
    code=list(decoder.disasm(data[draw:draw+2982],draw))
    # Entire 120-byte descriptor is copied from this table in the renderer.
    for offset in range(0,112,16):
        assert any(i.mnemonic=='movups' and any(o.type==X86_OP_MEM and o.mem.disp==hands+offset
                   for o in i.operands) for i in code),(path,'hand descriptor',offset)
    assert any(i.mnemonic=='movsd' and any(o.type==X86_OP_MEM and o.mem.disp==hands+112
               for o in i.operands) for i in code),(path,'descriptor tail')
    # Native run state 1 bypasses the default assignment of REST index 1.
    run=next(n for n,i in enumerate(code) if i.mnemonic=='cmp' and i.op_str=='cx, 1')
    branch=code[run+1];assert branch.mnemonic=='je',path
    target=branch.operands[0].imm
    assert any(i.address+i.size==target and i.mnemonic=='mov' and i.op_str=='ebp, 1'
               for i in code),(path,'walking rest selection')
    assert any((i.mnemonic=='mov' and i.op_str=='ebp, 2') or
               (i.mnemonic=='lea' and i.op_str=='ebp, [rdi + 1]') for i in code),path
    for bank in (0x1e,0x3c):
        assert any(i.mnemonic=='add' and i.op_str==f'ebp, {hex(bank)}' for i in code),(path,'outfit bank')
    # Recover HAND_NAMES from its actual string pointers, including retail.
    rest=data.index(b'HAND_BARE_REST\0')
    pointer=struct.pack('<Q',pe.OPTIONAL_HEADER.ImageBase+rest)
    names=data.index(pointer)-8
    for index,name in ((1,'HAND_BARE_REST'),(2,'HAND_BARE_RUN'),(31,'HAND_GLOVES_REST'),
                       (32,'HAND_GLOVES_RUN'),(61,'HAND_XRAY_REST'),(62,'HAND_XRAY_RUN')):
        address=struct.unpack_from('<Q',data,names+index*8)[0]-pe.OPTIONAL_HEADER.ImageBase
        assert data[address:address+len(name)+1]==name.encode()+b'\0',(path,index)
    print(f'{path}: RUN/REST indices, 120-byte hand descriptor and all three material banks verified')
