from pathlib import Path
import sys,re,struct,difflib
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_MEM,X86_OP_IMM

root=Path('C:/dev/TombRaider123VR')
sys.path.insert(0,str(root/'tools'))
from port_build import Image
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
source=(root/'src/GameDll.cpp').read_text(encoding='utf-8')
for game,key,ks,puzzle,ps in [(1,0x39f70,1002,0x3a360,1054),(2,0x675d0,935,0x67980,1150),(3,0x99140,950,0x99500,1323)]:
    oldPE=pefile.PE(str(root/f'PDB/tomb{game}.dll'))
    old=object.__new__(Image);old.data=oldPE.get_memory_mapped_image();old.base=oldPE.OPTIONAL_HEADER.ImageBase
    for directory in ('PDB','build/current-retail-verify','build/current-october-verify'):
        pe=pefile.PE(str(root/directory/f'tomb{game}.dll'))
        assert f'0x{pe.FILE_HEADER.TimeDateStamp:08X}' in source
        new=object.__new__(Image);new.data=pe.get_memory_mapped_image();new.base=pe.OPTIONAL_HEADER.ImageBase
        entries={e.struct.BeginAddress:e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION}
        owners={}
        for e in entries.values():
            owner=e
            while new.data[owner.UnwindData]>>3&4:
                at=owner.UnwindData+4+((new.data[owner.UnwindData+2]+1)&~1)*2
                owner=entries[struct.unpack_from('<I',new.data,at)[0]]
            owners[e.BeginAddress]=owner.BeginAddress
        for label,start,size,state in [('key',key,ks,42),('puzzle',puzzle,ps,43)]:
            reference=[i[1] for i in old._tokens(start,start+size)]
            candidates=set()
            # Native assignments set current state and STOP goal together.
            for match in re.finditer(b'\xc7[\x40-\x47]\x12'+re.escape(struct.pack('<I',0x20000|state)),new.data):
                offset=match.start()
                for e in entries.values():
                    if e.BeginAddress<=offset<e.EndAddress:candidates.add(owners[e.BeginAddress])
            matches=[]
            for candidate in candidates:
                end=max(e.EndAddress for e in entries.values() if owners[e.BeginAddress]==candidate)
                tokens=[i[1] for i in new._tokens(candidate,end)]
                score=difflib.SequenceMatcher(None,reference,tokens,autojunk=False).ratio()
                if score>.9:matches.append((candidate,end,score))
            assert len(matches)==1,(game,directory,label,matches)
            begin,end,score=matches[0]
            writes=[i for i in md.disasm(new.data[begin:end],begin) if i.mnemonic=='mov' and len(i.operands)==2 and
                i.operands[0].type==X86_OP_MEM and i.operands[0].mem.disp==18 and
                i.operands[1].type==X86_OP_IMM and i.operands[1].imm==(0x20000|state)]
            assert writes,(game,directory,label)
            print(f'PASS TR{game} {pe.FILE_HEADER.TimeDateStamp:08X} {label}: state={state} STOP goal=2; native match {score:.3f}')
