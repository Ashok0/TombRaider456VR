from pathlib import Path
import sys, subprocess, re, json, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP
root=Path('C:/dev/TombRaider456VR');r123=Path('C:/dev/TombRaider123VR')
cs=Cs(CS_ARCH_X86,CS_MODE_64);cs.detail=True;cs.skipdata=True
partial=root/'build/classic-turn/partial.json'
rows=json.loads(partial.read_text()) if partial.exists() else []
for game in range(1,6):
 repo=r123 if game<4 else root
 ref=repo/f'PDB/tomb{game}.dll'
 out=subprocess.check_output([sys.executable,str(root/'tools/pdbdump.py'),str(ref),'phd_SetItemMatrix','DrawLara','CalculateObjectLightingLara'],text=True)
 syms={p[3]:(int(p[0],16),int(p[2])) for line in out.splitlines() if len(p:=line.split(None,3))==4 and p[0].startswith('0x')}
 fn,size=syms['phd_SetItemMatrix'];draw,drawsize=syms['DrawLara']
 pe=pefile.PE(str(ref));mem=pe.get_memory_mapped_image()
 pattern=[re.escape(bytes([b])) for b in mem[fn:fn+size]]
 for i in cs.disasm(mem[fn:fn+size],fn):
  offsets=[]
  if any(o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP for o in i.operands):offsets+=range(i.disp_offset,i.disp_offset+i.disp_size)
  if i.mnemonic=='call':offsets+=range(i.imm_offset,i.imm_offset+i.imm_size)
  for offset in offsets:pattern[i.address-fn+offset]=b'.'
 regex=re.compile(b''.join(pattern),re.DOTALL)
 paths=[('pdb',ref)]
 if game<4:paths += [(flavor,repo/f'build/current-{flavor}-verify/tomb{game}.dll') for flavor in ['retail','egs']]
 else:paths += [('retail',Path(f'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered/{game}/tomb{game}.dll'))]
 for flavor,path in paths:
  if any(r['game']==game and r['flavor']==flavor for r in rows):continue
  target=pefile.PE(str(path));data=target.get_memory_mapped_image()
  matches=list(regex.finditer(data));assert len(matches)==1,(game,flavor,'signature',len(matches))
  address=matches[0].start();assert data[address:address+5]==bytes.fromhex('48 89 5c 24 08')
  # Locate Lara's one call by the exact preceding LEA of pos_prev into RCX.
  previous=0x6c if game<4 else 0x1840
  current=0x58 if game<4 else 0x60
  text=next(s for s in target.sections if s.Name.startswith(b'.text'))
  instructions=list(cs.disasm(data[text.VirtualAddress:text.VirtualAddress+text.Misc_VirtualSize],text.VirtualAddress))
  returns=[]
  for n,i in enumerate(instructions):
   if i.mnemonic!='call' or i.op_str!=hex(address):continue
   near=instructions[max(0,n-24):n]
   if not near or near[-1].mnemonic!='lea' or not near[-1].op_str.startswith('rcx, [') or not near[-1].op_str.endswith(f'+ {hex(previous)}]'):continue
   if not any(j.mnemonic=='lea' and j.op_str.startswith('rdx, [') and j.op_str.endswith(f'+ {hex(current)}]') for j in near):continue
   after=instructions[n+1:n+9]
   if game<4 and not any(j.mnemonic=="test" and j.op_str=="eax, eax" for j in after):continue
   if game==4 and not any(j.mnemonic=="test" and j.op_str=="ebx, ebx" for j in after):continue
   if game==5:
    if flavor=='pdb':
     xray,xraySize=syms['DrawLara__6']
     if xray<=i.address<xray+xraySize:continue # Scripted X-ray camera suspends first person.
    hd,hdSize=syms['DrawLara__HD']
    if flavor=='retail':hd=0x17490 # Existing verified CrouchDrawDll row.
    if hd<=i.address<hd+hdSize:continue
    if after[0].mnemonic!='call':continue
    lighting=int(after[0].op_str,16)
    if flavor=='pdb' and lighting!=syms['CalculateObjectLightingLara'][0]:continue
    # The four Classic outfit draws call the same Lara lighting routine next.
    if flavor!='pdb':
     assert data[hd:hd+7]==bytes.fromhex('48 8b c4 48 89 58 08')
     hdIns=list(cs.disasm(data[hd:hd+hdSize],hd))
     lightCalls=[hdIns[k+1] for k,j in enumerate(hdIns[:-1]) if j.mnemonic=='call' and j.op_str==hex(address)]
     assert len(lightCalls)==1 and lightCalls[0].mnemonic=='call'
     if lighting!=int(lightCalls[0].op_str,16):continue
   returns.append(i.address+i.size)
  target.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]])
  functions=[(e.struct.BeginAddress,e.struct.EndAddress) for e in target.DIRECTORY_ENTRY_EXCEPTION]
  print('raw',game,flavor,[hex(x) for x in returns],flush=True)
  returns=[ret for ret in returns if game>=4 or any(start<ret<end and mem[draw:draw+8] in data[max(0,start-512):start+8] for start,end in functions)]
  assert len(returns)==(3 if game==5 else 1),(game,flavor,returns)
  if flavor=='pdb':
   allowed=[syms[name] for name in ['DrawLara__1','DrawLara__4','DrawLara__5']] if game==5 else [(draw,drawsize)]
   assert all(any(start<ret<start+length for start,length in allowed) for ret in returns)
  rows.append(dict(game=game,flavor=flavor,stamp=target.FILE_HEADER.TimeDateStamp,setItemMatrix=address,drawReturn=returns[0],drawReturns=returns,path=str(path)))
  partial.write_text(json.dumps(rows,indent=2))
  print(game,flavor,hex(address),hex(returns[0]))
(root/'build/classic-turn/address-audit.json').write_text(json.dumps(rows,indent=2))
for low,high,destination in [(1,3,root/'build/vr-options123/src/ClassicBodyAddresses.inl'),(4,5,root/'src/ClassicBodyAddresses.inl')]:
 destination.write_text('// Verified phd_SetItemMatrix and Classic DrawLara call sites.\nconstexpr ClassicBodyDll kClassicBodyDlls[]={\n'+''.join('    {0x%X,0x%X,{%s}}, // TR%d %s\n'%(r['stamp'],r['setItemMatrix'],','.join(hex(x) for x in r.get('drawReturns',[r['drawReturn']])),r['game'],r['flavor']) for r in rows if low<=r['game']<=high)+'};\n')
print('PASS: 13 Classic body matrix hooks and Lara-only call sites verified.')



