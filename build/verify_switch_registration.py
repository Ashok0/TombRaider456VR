from pathlib import Path
import re, struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_REG, X86_REG_RIP

root=Path(__file__).resolve().parents[1]
game=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered')
rows=re.findall(r'\{(0x[0-9A-Fa-f]+(?:,0x[0-9A-Fa-f]+){13})\}',(root/'src/PhysicalSwitch.inl').read_text())
objects=[0x699CC0,0x6971C0,0x69AC00,0x697100]
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for n,row in enumerate(rows):
    stamp,collision,*_=map(lambda x:int(x,16),row.split(','))
    number=4+n%2
    path=root/'PDB'/f'tomb{number}.dll' if n<2 else game/str(number)/f'tomb{number}.dll'
    pe=pefile.PE(str(path));assert pe.FILE_HEADER.TimeDateStamp==stamp
    data=pe.get_memory_mapped_image();found=False
    for suffix in (0x05,0x0d):
        signature=bytes((0x48,0x8d,suffix));at=0
        while (at:=data.find(signature,at))>=0:
            if at+7+struct.unpack_from('<i',data,at+3)[0]==collision:
                code=list(md.disasm(data[at:at+1024],at));register=code[0].operands[0].reg
                for ins in code[1:]:
                    if ins.mnemonic=='mov' and len(ins.operands)==2:
                        dst,src=ins.operands
                        if dst.type==X86_OP_MEM and dst.mem.base==X86_REG_RIP and src.type==X86_OP_REG and src.reg==register:
                            offset=ins.address+ins.size+dst.mem.disp-objects[n]
                            if 0<=offset<1024*3792 and offset%3792==56: found=True;break
                    if ins.operands and ins.operands[0].type==X86_OP_REG and ins.operands[0].reg==register: break
            at+=1
    assert found,(path,'ordinary switch callback at object+56')
    print(f'PASS: {path}: ordinary switch object callback offset 56')
