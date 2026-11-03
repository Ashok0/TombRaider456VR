from pathlib import Path
import sys,json,struct,capstone,difflib
from capstone.x86 import X86_OP_MEM
import pefile
sys.path.insert(0,r'C:\dev\TombRaider123VR\tools')
from port_build import Image,old_symbols
root=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered');mapping=json.loads(Path(r'C:\dev\TombRaider123VR\build\current-october-verify\port_build.json').read_text());md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
for g in (1,2,3):
 name=f'tomb{g}.dll';p=pefile.PE(str(root/str(g)/name));d=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 oldp=pefile.PE(str(Path(r'C:\dev\TombRaider123VR\PDB')/name));old=Image.__new__(Image);old.data=oldp.get_memory_mapped_image();old.base=oldp.OPTIONAL_HEADER.ImageBase
 live=Image.__new__(Image);live.data=d;live.base=base;syms=old_symbols(name)
 begin=mapping[name]['symbols']['LaraAboveWater']['new'];tables=set()
 for i in md.disasm(d[begin:begin+2048],base+begin):
  if i.mnemonic=='call' and i.operands[0].type==X86_OP_MEM and i.operands[0].mem.scale==8:tables.add(i.operands[0].mem.disp)
 for table in tables:
  a=struct.unpack_from('<Q',d,table+2*8)[0]-base
  if not 0<a<len(d):continue
  expectedName=None
  for n in ('lara_as_stop','lara_col_stop'):
   b,_,s=syms[n];ref=[i[1] for i in old._tokens(b,b+s)];actual=[i[1] for i in live._tokens(a,a+s)]
   ratio=difflib.SequenceMatcher(None,ref,actual,autojunk=False).ratio()
   if ratio>.65:expectedName=n;print(g,n,hex(a),'normalized similarity',round(ratio,3),'old size',s)
  if expectedName:
   e=next((e.struct.EndAddress for e in p.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==a),a+2000)
   Path(f'build/ledge-hang-camera123/native/tr{g}-installed-{expectedName}.txt').write_text('\n'.join(f'{i.address-base:08X} {i.mnemonic} {i.op_str}' for i in md.disasm(d[a:e],base+a)))
