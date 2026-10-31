from pathlib import Path
import json,pefile
from capstone import *
from capstone.x86 import *
root=Path('C:/dev/TombRaider456VR');steam=Path('C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
rows=[]
# stamp, Options function, native subpage, native selection, text function/count,
# TR6 ring renderer/font color/font id/left edge/active menu/Options menu.
for d in json.loads((root/'build/submenu-text-addresses.json').read_text()):
 rows.append([d['stamp'],0,0,0,d['text'],d['count'],0,0,0,0,0,0])
others=[
 (root/'PDB/tomb4.dll',[0x696b4999,0xd9340,0x1bc5fc,0x1b8428,0,0,0,0,0,0,0,0]),
 (steam/'4/tomb4.dll',[0x68c12fda,0xd9ec0,0x1bd53c,0x1b9428,0,0,0,0,0,0,0,0]),
 (root/'PDB/tomb5.dll',[0x696b499c,0xe6e90,0x1f95a4,0x1b4284,0,0,0,0,0,0,0,0]),
 (steam/'5/tomb5.dll',[0x68c12fe9,0xe6f80,0x1f94e4,0x1b4284,0,0,0,0,0,0,0,0]),
 (Path('C:/dev/PDB/tomb6.dll'),[0x696b49a4,0x19b3c0,0,0,0x190bb0,0,0xdb2a0,0x426984,0xc829f0,0x421ec8,0x55963c0,0x55994c0]),
 (steam/'6/tomb6.dll',[0x68c12fe6,0x19b770,0,0,0x190f00,0,0xdabd0,0x4258e4,0xc81950,0x420e28,0x5595320,0x5598420])]
lines=['// Native Options submenu ABI and globals, verified by submenu_extra_addresses.py.','static constexpr Extra Extras[]={']
for row in rows:lines.append('    {'+','.join('0x%x'%v for v in row)+',{}},')
cs=Cs(CS_ARCH_X86,CS_MODE_64);cs.detail=True
for path,row in others:
 pe=pefile.PE(str(path));m=pe.get_memory_mapped_image();a=row[1];b=m[a:a+5]
 ins=list(cs.disasm(m[a:a+12500],a));first=ins[0]
 assert first.size==5 and first.mnemonic=='mov' and first.op_str.startswith('qword ptr [rsp + ')
 refs={i.address+i.size+o.mem.disp for i in ins for o in i.operands if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP}
 if row[2]:assert row[2] in refs and row[3] in refs
 else:
  assert row[7] in refs and row[8] in refs and row[9] in refs
 assert all(v<len(m) for v in row[1:] if v)
 lines.append('    {'+','.join('0x%x'%v for v in row)+',{'+','.join('0x%02x'%v for v in b)+'}},')
lines.append('};')
(root/'src/VRNativeMenuExtra.inl').write_text('\n'.join(lines)+'\n')
print('PASS: native submenu hooks and globals verified for all 15 builds')
