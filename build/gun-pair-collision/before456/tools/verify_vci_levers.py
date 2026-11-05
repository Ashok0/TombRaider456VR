"""Verify VCI flip handles, native OCB-4 activation and authored pose fixtures.
Reads installed assets and PE files only; never executes game code.
"""
from pathlib import Path
import math,struct,sys
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from verify_revolver_laser_mesh import vertices

root=Path(sys.argv[1]);repo=Path(__file__).resolve().parents[1]
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for path,stamp,collision,control in [(repo/'PDB/tomb5.dll',0x696b499c,0x8a990,0x8a770),
                                    (root/'5/tomb5.dll',0x68c12fe9,0x8aca0,0x8aa80)]:
    pe=pefile.PE(str(path));assert pe.FILE_HEADER.TimeDateStamp==stamp
    code=list(md.disasm(pe.get_data(collision,1125),collision))
    # Both sides of the OCB-4 branch select the original up/down use clips.
    for anim in (0xc3,0xc4):
        i=next(i for i,x in enumerate(code) if x.mnemonic=='mov' and x.op_str==f'word ptr [rbx + 0x18], {hex(anim)}')
        assert any(x.mnemonic=='cmp' and x.op_str=='cx, 4' for x in code[max(0,i-5):i])
    control_code=list(md.disasm(pe.get_data(control,535),control))
    assert any(x.mnemonic=='mov' and x.op_str.endswith(', 0x112') for x in control_code) # airlock slot only
    assert any(x.mnemonic=='or' and '0x2c]' in x.op_str for x in control_code)
    print('PASS native VCI OCB-4 up/down clips and shared switch control:',hex(stamp))

def mul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def vec(a,v):return [sum(a[i][k]*v[k] for k in range(3)) for i in range(3)]
def rotations(words):
    at=9;out=[]
    for joint in range(2):
        first=words[at]&65535;at+=1;kind=first>>14;x=y=z=0
        if kind:
            angle=(first&16383)*2*math.pi/4096
            if kind==1:x=angle
            elif kind==2:y=angle
            else:z=angle
        else:
            packed=(first<<16)|(words[at]&65535);at+=1
            x=((packed>>20)&1023)*2*math.pi/1024;y=((packed>>10)&1023)*2*math.pi/1024;z=(packed&1023)*2*math.pi/1024
        cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
        out.append(mul(mul([[cy,0,sy],[0,1,0],[-sy,0,cy]],[[1,0,0],[0,cx,-sx],[0,sx,cx]]),
                       [[cz,-sz,0],[sz,cz,0],[0,0,1]]))
    return out

payload=bytearray(struct.pack('<I',2))
for level in ('RICH1','RICH3'):
    name='SWITCH_TYPE1_'+level
    mesh=vertices(root/f'5/ITEM/{name}.TRM')
    bar=[p for p,(j,w) in mesh.items() if j[0]==1 and p[1]<-70]
    low=[min(p[k] for p in bar) for k in range(3)];high=[max(p[k] for p in bar) for k in range(3)]
    expected=((-56.97,-92.36,-52.92),(56.97,-71.52,-32.08)) if level=='RICH1' else ((-57,-91,-51),(51,-73,-34))
    assert all(abs(a-b)<.02 for a,b in zip(low+high,expected[0]+expected[1]))
    assert sum(j[0]==1 for j,w in mesh.values())>40
    d=(root/f'5/DATA/{level}.PDP').read_bytes();at=0;t=[]
    for stride in (40,6,8,2,4,2,18):
        n=struct.unpack_from('<I',d,at)[0];at+=4;t.append((at,n));at+=n*stride
    o=next(struct.unpack_from('<IhhIIh',d,t[6][0]+i*18) for i in range(t[6][1]) if struct.unpack_from('<I',d,t[6][0]+i*18)[0]==266)
    assert o[1]==2
    bone=struct.unpack_from('<4i',d,t[4][0]+o[3]*4);assert bone==(0,-4,-2,-42)
    poses=[None]*168
    for i in range(4):
        a=struct.unpack_from('<Ihhiiii8h',d,t[0][0]+(o[-1]+i)*40)
        assert a[2]==(1 if i in (0,3) else 0) and a[3:7]==(0,0,0,0)
        assert a[7:11]==[(0,0,o[-1],0),(1,1,o[-1]+1,1),(2,84,o[-1]+1,1),(85,167,o[-1],0)][i]
        assert (a[1]&255)==1 and a[13]==(0 if i<2 else 1)
        if i<2:
            change=struct.unpack_from('<3h',d,t[1][0]+a[12]*6);assert a[11]==1 and change[:2]==(i,1)
            r=struct.unpack_from('<4h',d,t[2][0]+change[2]*8)
            assert r[2:]==(o[-1]+2+i,2 if i==0 else 85)
        else:assert a[11]==0 and struct.unpack_from('<h',d,t[3][0]+a[14]*2)[0]==4
        size=a[1]>>8
        for frame in range(a[7],a[8]+1):
            f=struct.unpack_from('<'+str(size)+'h',d,t[5][0]+a[0]+(frame-a[7])*size*2)
            r=rotations(f);origin=f[6:9];offset=vec(r[0],bone[1:]);child=[origin[k]+offset[k] for k in range(3)]
            matrices=[]
            for rotation,position in [(r[0],origin),(mul(r[0],r[1]),child)]:
                matrices.extend(v for k in range(3) for v in (*rotation[k],position[k]))
            poses[frame]=matrices
    assert all(p is not None for p in poses)
    # Verify actual endpoint travel, not just that the animation changes state.
    contact=(0,-82+2,-42.5+42) # mesh bar centre through canonical inverse bind
    points=[]
    for frame in (0,1):
        m=poses[frame][12:];points.append([sum(m[k*4+j]*contact[j] for j in range(3))+m[k*4+3] for k in range(3)])
    assert 155<points[1][1]-points[0][1]<165 and abs(points[1][2]-points[0][2])<4
    payload.extend(name.encode().ljust(64,b'\0'));payload.extend(struct.pack('<3f',*bone[1:]));payload.extend(struct.pack('<6h',-84,86,-675,-459,426,511))
    for p in poses:payload.extend(struct.pack('<24f',*p))
    print('PASS measured flip handle, both directions, inverse bind and 168 authored poses:',name)
Path(sys.argv[2]).write_bytes(payload)
