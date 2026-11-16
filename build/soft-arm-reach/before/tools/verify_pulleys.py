"""Read-only native pulley/asset audit. Never loads or executes a game DLL."""
from pathlib import Path
import re, struct, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_REG, X86_OP_IMM, X86_REG_RIP
from verify_revolver_laser_mesh import vertices

repo=Path(__file__).resolve().parents[1]
root=Path(sys.argv[1])
source=(repo/'src/PhysicalPulley.inl').read_text()
rows=[tuple(int(v,16) for v in r) for r in re.findall(r'\{(0x\w+),(0x\w+),(0x\w+),(0x\w+),(0x\w+)\}',source.split('};',2)[1])]
assert len(rows)==4
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
references={}
for row,(game,path) in zip(rows,[(4,repo/'PDB/tomb4.dll'),(5,repo/'PDB/tomb5.dll'),(4,root/'4/tomb4.dll'),(5,root/'5/tomb5.dll')]):
    stamp,collision,lara,control,query=row
    pe=pefile.PE(str(path));m=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    assert pe.FILE_HEADER.TimeDateStamp==stamp
    assert m[collision:collision+5]==bytes((0x48,0x89,0x5c,0x24,0x18 if game==4 else 0x10))
    sig=bytes.fromhex('48 0f bf c1 45 33 d2' if game==4 else '48 89 5c 24 18')
    assert m[control:control+len(sig)]==sig
    for label,at,size in [('collision',collision,771 if game==4 else 538),('pull-event',lara,281 if game==4 else 320),('floor-query',query,269 if stamp in (0x68c12fda,0x68c12fe9) else 287)]:
        instructions=list(md.disasm(m[at:at+size],at));normalized=[]
        for ins in instructions:
            ops=[]
            for op in ins.operands:
                if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP:ops.append(('rip',op.size))
                elif op.type==X86_OP_MEM:ops.append(('mem',op.mem.base,op.mem.index,op.mem.scale,op.mem.disp,op.size))
                elif op.type==X86_OP_REG:ops.append(('reg',op.reg,op.size))
                elif op.type==X86_OP_IMM:ops.append(('branch',op.imm-at) if (ins.group(1) or ins.group(2)) and at<=op.imm<at+size else
                                                   ('external',) if ins.group(1) or ins.group(2) else ('imm',op.imm))
            normalized.append((ins.mnemonic,ops))
        if label=='collision':
            # The retail refusal sound and achievement calls use equivalent
            # mov/lea register setup. Compare the activation block itself;
            # neither the adapter nor these tests reimplement those callbacks.
            start=next(i for i,x in enumerate(instructions) if x.mnemonic=='mov' and x.op_str.endswith(', 0x153'))
            end=next(i for i,x in enumerate(instructions) if i>start and x.mnemonic=='mov' and x.op_str.startswith('word ptr [rip') and x.op_str.endswith(', dx'))
            normalized=normalized[start:end+1]
        if label=='floor-query':
            # Retail TR4 folds the initial floor-data scan's redundant movzx.
            # Its object-index collection loop is unchanged apart from padding.
            assert sum(i.mnemonic=='call' for i in instructions)==2
            assert any(i.mnemonic=='and' and i.op_str.endswith(', 0x1f') for i in instructions)
            start=next(i for i,x in enumerate(instructions) if x.mnemonic=='mov' and x.op_str.startswith('r11, qword ptr [rip'))
            normalized=[(name,[('branch',) if op[0]=='branch' else op for op in ops])
                        for name,ops in normalized[start:] if name not in ('nop','int3')]
        key=(game,label)
        if key in references:assert normalized==references[key],(game,label,'retail/native semantics changed')
        else:references[key]=normalized
        if label=='pull-event':
            assert not any(i.mnemonic=='call' for i in instructions)
            assert any(i.mnemonic=='add' and i.op_str.endswith(', 0x2c') for i in instructions)
            assert any(i.mnemonic=='mov' and i.op_str.startswith('word ptr [r8 + 0x30],') for i in instructions)
            assert any(i.mnemonic=='mov' and i.op_str.startswith('word ptr [r8 + 0x3c],') for i in instructions)
            assert instructions[-1].mnemonic=='ret'
    # Validate native object registration and the state-104 routine pointer.
    if path.parent==repo/'PDB':
        obj,at,size=(0x699cc0,0x85370,6860) if game==4 else (0x6971c0,0x82100,8106)
        slot=321 if game==4 else 282;registers={};writes={}
        for ins in md.disasm(m[at:at+size],at):
            ops=ins.operands
            if ins.mnemonic=='lea' and len(ops)==2 and ops[0].type==X86_OP_REG and ops[1].type==X86_OP_MEM and ops[1].mem.base==X86_REG_RIP:
                registers[ops[0].reg]=ins.address+ins.size+ops[1].mem.disp
            if ins.mnemonic=='mov' and len(ops)==2 and ops[0].type==X86_OP_MEM and ops[0].mem.base==X86_REG_RIP and ops[1].type==X86_OP_REG:
                writes[ins.address+ins.size+ops[0].mem.disp]=registers.get(ops[1].reg)
        assert writes[obj+slot*3792+56]==collision
        assert writes[obj+slot*3792+24]==control
        assert struct.pack('<Q',base+lara) in m
    print('PASS native pulley callbacks, +44 counter event, scoped hooks and floor query:',game,hex(stamp))

cases=[]
for game in (4,5):
    mesh=vertices(root/f'{game}/ITEM/PULLEY.TRM')
    joints=9 if game==4 else 1
    assert set(j[0] for j,w in mesh.values())==set(range(joints))
    lows=[-246.42,-503.42,-758.42,-1015.42,-1270.42,-1525.42,-1782.42,-2039.42,-2066.37]
    highs=[16.37,-240.63,-495.63,-752.63,-1007.63,-1262.63,-1519.63,-1776.63,-2033.63]
    for j in range(joints):
        points=[p for p,(b,w) in mesh.items() if b[0]==j]
        assert abs(min(p[2] for p in points)-(lows[j] if game==4 else -1538.75))<.02
        assert abs(max(p[2] for p in points)-(highs[j] if game==4 else 510.25))<.02
        assert max(abs(p[k]) for p in points for k in (0,1))<(17 if game==4 else 9)
    for path in sorted((root/str(game)/'DATA').glob('*.PDP')):
        d=path.read_bytes();at=0;t=[]
        for stride in (40,6,8,2,4,2,18):
            n=struct.unpack_from('<I',d,at)[0];at+=4;t.append((at,n));at+=n*stride
            assert at<=len(d)
        for i in range(t[6][1]):
            o=struct.unpack_from('<IhhIIh',d,t[6][0]+i*18)
            if o[0]!=(321 if game==4 else 282):continue
            assert o[1]==joints
            a=struct.unpack_from('<Ihhiiii8h',d,t[0][0]+o[-1]*40)
            assert a[2:7]==(0,0,0,0,0) and a[7]==0 and a[8]==(72 if game==4 else 58)
            assert a[9:11]==(o[-1],15 if game==4 else 0) and a[11]==a[13]==0
            size,rate=a[1]>>8,a[1]&255
            assert (size==19 and rate in (1,2)) if game==4 else (size,rate)==(11,1)
            words=struct.unpack_from('<'+str((a[8]//rate+1)*size)+'h',d,t[5][0]+a[0])
            frames=[words[i:i+size] for i in range(0,len(words),size)]
            assert all(f[6]==frames[0][6] and f[8:]==frames[0][8:] for f in frames)
            assert all(f[7]>=frames[i-1][7] for i,f in enumerate(frames) if i)
            assert frames[-1][7]-frames[0][7]==(507 if game==4 else 0)
            for anim,last in [(339,13),(340,58),(341,13)]:
                l=struct.unpack_from('<Ihhiiii8h',d,t[0][0]+anim*40)
                assert l[2]==104 and l[8]-l[7]==last
            cases.append((game,rate,size,len(words),*words))
            print('PASS command-free pulley and native Lara pull path:',game,path.name)
assert len(cases)==12
if len(sys.argv)>2:Path(sys.argv[2]).write_text('\n'.join(' '.join(map(str,row)) for row in cases)+'\n')
print('PASS: 4 native layouts, both chain/rope meshes, 12 installed animation paths')
