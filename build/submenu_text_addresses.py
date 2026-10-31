from pathlib import Path
import pefile,json
from capstone import *
from capstone.x86 import *
root=Path('C:/dev/TombRaider456VR');src=Path('C:/dev/TombRaider123VR')
cs=Cs(CS_ARCH_X86,CS_MODE_64);cs.detail=True
out=[]
for g,a in [(1,0x8a940),(2,0xbe280),(3,0x10bc90)]:
 ref=pefile.PE(str(src/f'PDB/tomb{g}.dll'));mem=ref.get_memory_mapped_image()
 # Match the complete 300-byte native T_Print routine, masking only encoded
 # instruction-relative operands. Field offsets and control flow remain exact.
 mask=bytearray([255]*300)
 for i in cs.disasm(mem[a:a+300],a):
  if any(o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP for o in i.operands):
   for k in range(4):mask[i.address-a+i.disp_offset+k]=0
 pattern=mem[a:a+300]
 for flavor,path in [('pdb',src/f'PDB/tomb{g}.dll'),('retail',src/f'build/current-retail-verify/tomb{g}.dll'),('egs',src/f'build/current-egs-verify/tomb{g}.dll')]:
  pe=pefile.PE(str(path));m=pe.get_memory_mapped_image();matches=[];pos=0
  while True:
   pos=m.find(pattern[:15],pos)
   if pos<0:break
   if all(not mask[j] or m[pos+j]==pattern[j] for j in range(300)):matches.append(pos)
   pos+=1
  assert len(matches)==1,(g,flavor,matches)
  rva=matches[0]
  ins=list(cs.disasm(m[rva:rva+300],rva))
  count=next(i.address+i.size+o.mem.disp for i in ins for o in i.operands if i.mnemonic=='movzx' and o.type==X86_OP_MEM and o.size==2 and o.mem.base==X86_REG_RIP)
  out.append({'game':g,'stamp':pe.FILE_HEADER.TimeDateStamp,'text':rva,'count':count})
  print(g,flavor,hex(rva),hex(count))
(root/'build/submenu-text-addresses.json').write_text(json.dumps(out,indent=2))
