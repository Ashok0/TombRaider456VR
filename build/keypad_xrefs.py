import sys,subprocess
sys.path.insert(0,'tools')
from port_addresses import Image
im=Image('PDB/tomb5.dll')
symbols={}
dump=subprocess.check_output([sys.executable,'tools/pdbdump.py','PDB/tomb5.dll'],text=True,stderr=subprocess.DEVNULL)
for line in dump.splitlines():
 p=line.split(None,3)
 if len(p)==4 and p[0].startswith('0x'):symbols[int(p[0],16)]=p[3]
targets={0x1b6b90,0x1b6b98}
for start in im.starts:
 for rva,size,norm,rip,branch in im.disasm(start):
  if rip in targets:print(hex(rva),symbols.get(start,hex(start)),hex(start),symbols.get(rip),norm)
