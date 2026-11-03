from pathlib import Path
import sys,struct,json
import pefile
sys.path.insert(0,r'C:\dev\TombRaider123VR\tools')
from port_build import Image,old_symbols
root=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered')
records=[]
for game in (1,2,3):
 name=f'tomb{game}.dll';symbols=old_symbols(name)
 old=Image.__new__(Image);pe=pefile.PE(str(Path(r'C:\dev\TombRaider123VR\PDB')/name));old.data=pe.get_memory_mapped_image();old.base=pe.OPTIONAL_HEADER.ImageBase
 live=Image.__new__(Image);pe=pefile.PE(str(root/str(game)/name));live.data=pe.get_memory_mapped_image();live.base=pe.OPTIONAL_HEADER.ImageBase
 for function in ('ModernControlsAutoGrab',):
  rva,kind,size=symbols[function];expected=[i[1] for i in old._tokens(rva,rva+size)]
  hits=[]
  for entry in pe.DIRECTORY_ENTRY_EXCEPTION:
   a,b=entry.struct.BeginAddress,entry.struct.EndAddress
   if abs((b-a)-size)<=16 and [i[1] for i in live._tokens(a,a+size)]==expected:hits.append(a)
  assert len(hits)==1,(game,function,'not an exact unique semantic match',hits)
  records.append(dict(game=game,function=function,rva=hits[0],instructions=len(expected)))
  print(f'TR{game} installed {function}: exact normalized match at 0x{hits[0]:X}, {len(expected)} instructions')
 # Auto-grab uses native backward fall 46/state11; no turn180 command.
 tables=0
 for p in (root/str(game)).rglob('*.PDP'):
  if p.stem.upper().startswith('CUT'):continue
  d=p.read_bytes();count,=struct.unpack_from('<I',d)
  if count<114:continue
  a=d[4+46*32:4+47*32];state,=struct.unpack_from('<h',a,6);commands,=struct.unpack_from('<h',a,28)
  assert (state,commands)==(11,0),(p,state,commands)
  tables+=1
 assert tables
 print(f'TR{game}: {tables} animation banks confirm backward drop has no half-turn command')
Path('build/ledge-hang-camera123/native-audit.json').write_text(json.dumps(records,indent=2))


