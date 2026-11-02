import subprocess,sys,pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from pathlib import Path
img=r'C:\dev\PDB\tomb6.dll';p=pefile.PE(img,fast_load=True);d=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
out=subprocess.run([sys.executable,'tools/pdbdump.py',img],capture_output=True,text=True).stdout
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
refs=[]
for line in out.splitlines():
 q=line.split(None,3)
 if len(q)!=4 or q[1]!='func':continue
 rva=int(q[0],16);size=int(q[2]);name=q[3]
 for i in md.disasm(d[rva:rva+size],rva):
  if 'rip' in i.op_str and 0x426bd0<=i.address+i.size+i.disp<0x43a6d0:
   refs.append((name,hex(i.address),i.mnemonic,i.op_str,hex(i.address+i.size+i.disp)))
Path('build/mirror/tr6-table-xrefs.json').write_text(__import__('json').dumps(refs,indent=2))
print(refs)
