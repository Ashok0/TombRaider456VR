"""Read-only cog hook, installed animation and peg-mesh checks. No game DLL execution."""
from pathlib import Path
import math, re, struct, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM
from verify_revolver_laser_mesh import vertices

repo=Path(__file__).resolve().parents[1]
root=Path(sys.argv[1])
source=(repo/'src/PhysicalCogWheel.inl').read_text()
rows=re.findall(r'\{(0x[0-9A-Fa-f]+),(0x[0-9A-Fa-f]+),(0x[0-9A-Fa-f]+)\}',source)
assert len(rows)==4
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for n,row in enumerate(rows):
    stamp,collision,control=map(lambda s:int(s,16),row)
    game=4+n%2
    path=repo/f'PDB/tomb{game}.dll' if n<2 else root/f'{game}/tomb{game}.dll'
    pe=pefile.PE(str(path));assert pe.FILE_HEADER.TimeDateStamp==stamp
    for addr in (collision,control):assert pe.get_data(addr,5)==bytes.fromhex('48 89 5c 24 08')
    # Control's exact native pulse is Lara animation 321 / frame first+10;
    # it sets the linked door's item_flags[0] to 40, and owns stopping/removal.
    code=list(md.disasm(pe.get_data(control,308 if game==4 else 331),control))
    immediates={o.imm for i in code for o in i.operands if o.type==X86_OP_IMM}
    assert {321,10,40,11}<=immediates
    assert any(i.mnemonic=='mov' and '0x38], 0x28' in i.op_str for i in code)
    assert any(i.mnemonic=='mov' and '0x14], 2' in i.op_str for i in code)
    print('PASS native cog collision/control:',game,hex(stamp))

def read_tables(path):
    data=path.read_bytes();at=0;tables=[]
    for stride in (40,6,8,2,4,2,18):
        n=struct.unpack_from('<I',data,at)[0];at+=4;tables.append((at,n));at+=n*stride
        assert at<=len(data)
    return data,tables
def matmul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def rotations(words,joints):
    result=[];at=9
    for _ in range(joints):
        first=words[at];at+=1;kind=first>>14
        x=y=z=0
        if kind:
            angle=(first&0x3fff)*2*math.pi/4096
            if kind==1:x=angle
            elif kind==2:y=angle
            else:z=angle
        else:
            packed=(first<<16)|words[at];at+=1
            x=((packed>>20)&1023)*2*math.pi/1024
            y=((packed>>10)&1023)*2*math.pi/1024
            z=(packed&1023)*2*math.pi/1024
        cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
        result.append(matmul(matmul([[cy,0,sy],[0,1,0],[-sy,0,cy]],[[1,0,0],[0,cx,-sx],[0,sx,cx]]),
                             [[cz,-sz,0],[sz,cz,0],[0,0,1]]))
    return result

paths=[]
for game in (4,5):
    for path in sorted((root/str(game)/'DATA').glob('*.PDP')):
        data,t=read_tables(path)
        for o in range(t[6][1]):
            slot,joints,mesh,bones,frame,base=struct.unpack_from('<IhhIIh',data,t[6][0]+o*18)
            if slot!=(317 if game==4 else 278):continue
            assert joints in (3,4)
            anim=lambda i:struct.unpack_from('<Ihhiiii8h',data,t[0][0]+i*40)
            idle,loop,lara=anim(base),anim(base+1),anim(321)
            assert idle[2]==0 and idle[7]==idle[8]
            assert loop[2]==1 and loop[8]-loop[7]==53 and loop[9:11]==(base+1,loop[7])
            assert loop[1]&255==1 and loop[3:7]==(0,0,0,0)
            assert lara[2]==96 and lara[8]-lara[7]==53
            joint=1 if joints==4 else 2;angles=[];start=None
            for i in range(54):
                words=struct.unpack_from('<'+str(loop[1]>>8)+'H',data,t[5][0]+loop[0]+i*(loop[1]>>8)*2)
                rotation=rotations(words,joints)[joint]
                if start is None:start=rotation
                x=[r[0] for r in rotation];u=[r[0] for r in start];v=[r[2] for r in start]
                angles.append(math.atan2(sum(x[k]*v[k] for k in range(3)),sum(x[k]*u[k] for k in range(3))))
            direction=-1 if angles[-1]<0 else 1;angles=[a*direction for a in angles]
            assert math.pi/8*.85<angles[-1]<math.pi/8*1.1
            assert all(a>=-.01 and (i==0 or a>=angles[i-1]-.01) for i,a in enumerate(angles))
            # Catch accidental free-running anticipation: most first-half poses
            # are stationary, although the native door pulse happens at +10.
            assert abs(angles[10])<.01 and angles[40]>.1
            paths.append(angles)
            print('PASS authored pull:',game,path.name,'joint',joint,'degrees',round(math.degrees(angles[-1]),3))
assert len(paths)==5

for game,name,joint,center,full in [
    (4,'COG_SWITCH_SEMER2.TRM',1,(-8,510,6),True),
    (4,'COG_SWITCH_ANG_RACE.TRM',1,(-8,510,6),True),
    (4,'COG_SWITCH_CORTYARD.TRM',2,(5,185,208),False),
    (5,'COG_SWITCH.TRM',2,(0,187,202),False)]:
    points=[tuple(p[i]-center[i] for i in range(3)) for p,(j,w) in vertices(root/str(game)/'ITEM'/name).items() if j[0]==joint]
    for k in range(16 if full else 9):
        a=-k*math.pi/8;c,s=math.cos(a),math.sin(a)
        shaft=[p for p in points if abs(-s*p[0]+c*p[2])<28 and abs(p[1])<28 and c*p[0]+s*p[2]>480]
        assert len(shaft)>=8,(name,k)
        assert max(c*p[0]+s*p[2] for p in shaft)>765
        assert min(c*p[0]+s*p[2] for p in shaft)<550
    print('PASS peg contact lies inside radial handles:',name)

# Feed measured pose angles to the C++ regression (not hand-authored fixtures).
if len(sys.argv)>2:
    Path(sys.argv[2]).write_text('\n'.join(' '.join(f'{v:.9f}' for v in a) for a in paths)+'\n')
