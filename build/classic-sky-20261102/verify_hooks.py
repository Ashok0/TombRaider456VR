from pathlib import Path
import json,struct,pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
out=Path(__file__).parent;md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True;checks=0
for name in ['123-address-audit.json','456-address-audit.json']:
    for r in json.loads((out/name).read_text()):
        pe=pefile.PE(r['path'],fast_load=True);data=pe.get_memory_mapped_image();entry=int(r['entry'],16)
        assert pe.FILE_HEADER.TimeDateStamp==int(r['stamp'],16);checks+=1
        prologue=bytes.fromhex(r['prologue']);assert data[entry:entry+len(prologue)]==prologue;checks+=1
        instructions=list(md.disasm(prologue,entry));assert sum(i.size for i in instructions)==len(prologue);checks+=1
        if 'skyReturn' in r:
            ret=int(r['skyReturn'],16);assert data[ret-5]==0xe8 and ret+struct.unpack_from('<i',data,ret-4)[0]==entry;checks+=1
        if r.get('rip',-1)>=0:
            assert any(i.address-entry+i.disp_offset==r['rip'] and i.disp_size==4 for i in instructions);checks+=1
        if r.get('flush'):
            flush=int(r['flush'],16);assert any(s.VirtualAddress<=flush<s.VirtualAddress+s.Misc_VirtualSize and s.Characteristics&0x20000000 for s in pe.sections);checks+=1
        if r.get('skyRoom'):
            sky=int(r['skyRoom'],16);assert sky%8==0 and any(s.VirtualAddress<=sky<s.VirtualAddress+s.Misc_VirtualSize and s.Characteristics&0x80000000 for s in pe.sections);checks+=1
print(f'{checks} binary checks passed across 12 supported Classic/TR6 hook bindings.')
