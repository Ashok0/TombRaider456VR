from pathlib import Path
import json,re,pefile
from capstone import *
from capstone.x86 import *
root=Path('C:/dev/TombRaider456VR');repo=Path('C:/dev/TombRaider123VR')
# Handler/selection/subpage recovered through native inventory callsites;
# MC_texts recovered through 13+ agreeing text-construction references.
known={
 (1,'retail'):[0x9a410,0x175bf4,0x11fdb8,0x135b50],
 (1,'egs'):[0x9af50,0x177988,0x120dbc,0x1378e0],
 (2,'retail'):[0xc4150,0x17410c,0x15e2e0,0x174110],
 (2,'egs'):[0xc4100,0x1b5f2c,0x15f2c8,0x175e80],
 (3,'retail'):[0x11c880,0x216080,0x1b7900,0x1cdba0],
 (3,'egs'):[0x11d6c0,0x217e20,0x1b8900,0x1cf930],
 (1,'pdb'):[0x9a2b0,0x174cb4,0x11ee78,0x134c10,0x134bb8],
 (2,'pdb'):[0xc46a0,0x1741cc,0x15e3a0,0x1741d0,0x174178],
 (3,'pdb'):[0x11ade0,0x213140,0x1b49c0,0x1cac60,0x1cabbc]}
cs=Cs(CS_ARCH_X86,CS_MODE_64);cs.detail=True
textRows={d['stamp']:d for d in json.loads((root/'build/submenu-text-addresses.json').read_text())}
result=[]
def rip(ins):
 return [ins.address+ins.size+o.mem.disp for o in ins.operands if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP]
for (g,flavor),fields in known.items():
 path=repo/f'PDB/tomb{g}.dll' if flavor=='pdb' else repo/f'build/current-{flavor}-verify/tomb{g}.dll'
 pe=pefile.PE(str(path));mem=pe.get_memory_mapped_image();stamp=pe.FILE_HEADER.TimeDateStamp
 fn,subpage,selection,nativeTexts=fields[:4]
 end=next(e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==fn)
 ins=list(cs.disasm(mem[fn:end],fn));refs={r for i in ins for r in rip(i)}
 assert subpage in refs and selection in refs
 assert len({r for r in refs if nativeTexts<=r<nativeTexts+160})>=13
 assert ins[0].size==5 and ins[0].mnemonic=='mov' and ins[0].op_str=='qword ptr [rsp + 8], rbx'
 masks={o.imm for i in ins if i.mnemonic=='test' for o in i.operands if o.type==X86_OP_IMM}
 assert {0x2000,0x4000,0x8000,0x10000,0x100000,0x200000}<=masks
 if flavor=='pdb':first=fields[4]
 else:
  candidates=[rip(i)[0] for i in ins if i.address<fn+0x100 and i.mnemonic=='mov' and i.operands[0].type==X86_OP_REG and i.operands[0].reg==X86_REG_R15D and rip(i)]
  assert len(candidates)==1;candidates2=[i for i in ins if i.mnemonic=='lea' and 'r15 + 0xa' in i.op_str]
  assert candidates2;first=candidates[0]
 assert first in refs
 text=textRows[stamp];prints=list(cs.disasm(mem[text['text']:text['text']+300],text['text']))
 pools=[rip(i)[0] for i in prints if i.mnemonic=='lea' and i.op_str.startswith('r10, [rip')]
 buffers=[rip(i)[0] for i in prints if i.mnemonic=='lea' and i.op_str.startswith('rax, [rip')]
 assert len(pools)==len(buffers)==1
 assert buffers[0]+32768<len(mem) and pools[0]+5632<len(mem)
 assert any(i.mnemonic=='shl' and i.op_str=='eax, 9' for i in prints)
 result.append(dict(game=g,flavor=flavor,stamp=stamp,options=fn,subpage=subpage,selection=selection,nativeTexts=nativeTexts,firstVisible=first,textPool=pools[0],textBuffers=buffers[0],bytes=list(mem[fn:fn+5]),text=text['text'],textCount=text['count']))
p=root/'src/VRNativeMenuExtra.inl';s=p.read_text()
for d in result:
 values=[d['stamp'],d['options'],d['subpage'],d['selection'],d['text'],d['textCount'],0,0,0,0,0,0]
 line='    {'+','.join(hex(v) for v in values)+',{'+','.join(hex(v) for v in d['bytes'])+'},'+','.join(hex(d[k]) for k in ['nativeTexts','firstVisible','textPool','textBuffers'])+'},'
 s,n=re.subn(r'^    \{'+hex(d['stamp'])+r',.*$',line,s,flags=re.M);assert n==1
p.write_text(s)
(root/'build/options123-nested/address-audit.json').write_text(json.dumps(result,indent=2))
print('PASS: all 9 TR1-3 PDB/Steam/EGS native Options hooks, input bits and text pools verified.')
