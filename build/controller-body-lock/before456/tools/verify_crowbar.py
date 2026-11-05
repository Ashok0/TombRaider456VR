"""Read-only verification of TR4 crowbar native handlers/assets. No DLL execution."""
from pathlib import Path
import math, struct, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

root=Path(sys.argv[1]); repo=Path(__file__).resolve().parents[1]
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for path,stamp,collision,animate in (
    (repo/'PDB/tomb4.dll',0x696b4999,0x8cea0,0x120e0),
    (root/'4/tomb4.dll',0x68c12fda,0x8d8b0,0x11d10)):
    pe=pefile.PE(str(path));assert pe.FILE_HEADER.TimeDateStamp==stamp
    assert pe.get_data(collision,5)==bytes.fromhex('48 89 5c 24 10')
    assert pe.get_data(animate,5)==bytes.fromhex('48 8b c4 53 55')
    code=list(md.disasm(pe.get_data(collision,1372),collision))
    assert any(x.mnemonic=='mov' and x.op_str=='word ptr [rbx + 0x18], 0x1a4' for x in code)
    assert any(x.mnemonic=='mov' and x.op_str=='word ptr [rbx + 0x18], 0x1b4' for x in code)
    assert any(x.mnemonic=='cmp' and x.op_str=='ebp, -2' for x in code)
    assert any(x.mnemonic=='mov' and x.op_str.startswith('dword ptr [rbx + 0x12], 0x280028') for x in code)
    print('PASS native insertion, failed-pry exclusion and hook signatures:',hex(stamp))

def mul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def vec(a,v):return [sum(a[i][k]*v[k] for k in range(3)) for i in range(3)]
def wrist(words,bones):
    at=9;rotations=[]
    for j in range(15):
        w=words[at]&65535;at+=1;kind=w>>14;x=y=z=0
        if kind:
            angle=(w&16383)*math.tau/4096
            if kind==1:x=angle
            elif kind==2:y=angle
            else:z=angle
        else:
            packed=(w<<16)|(words[at]&65535);at+=1
            x=((packed>>20)&1023)*math.tau/1024;y=((packed>>10)&1023)*math.tau/1024;z=(packed&1023)*math.tau/1024
        cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
        rotations.append(mul(mul([[cy,0,sy],[0,1,0],[-sy,0,cy]],[[1,0,0],[0,cx,-sx],[0,sx,cx]]),
                             [[cz,-sz,0],[sz,cz,0],[0,0,1]]))
    r=rotations[0];p=list(words[6:9]);stack=[]
    for j,(flags,*v) in enumerate(bones,1):
        if flags&1:r,p=stack.pop()
        if flags&2:stack.append((r,p))
        delta=vec(r,v);p=[p[k]+delta[k] for k in range(3)];r=mul(r,rotations[j])
        if j==10:return [value for k in range(3) for value in (*r[k],p[k])]

expected=(6,12487,8,6,12585,8,5,12516,16384,5,12569,16384,
          5,12549,16431,5,12542,16722,6,12516,16416,6,12569,16416,3)
payload=bytearray(struct.pack('<I',3));found=[]
for path in sorted((root/'4/DATA').glob('*.PDP')):
    d=path.read_bytes();at=0;t=[]
    for stride in (40,6,8,2,4,2,18):
        n=struct.unpack_from('<I',d,at)[0];at+=4;t.append((at,n));at+=n*stride
    objs=[struct.unpack_from('<IhhIIh',d,t[6][0]+i*18) for i in range(t[6][1])]
    o=next((x for x in objs if x[0]==320),None)
    if not o:continue
    found.append(path.stem);assert o[1]==1
    assert any(x[0]==28 and x[1]==15 for x in objs) and any(x[0]==246 for x in objs)
    def raw(i):return d[t[0][0]+i*40:t[0][0]+(i+1)*40]
    def anim(i):return struct.unpack('<Ihhiiii8h',raw(i))
    use=anim(420);assert use[1:11]==((38<<8)|1,40,0,0,0,0,12475,12593,11,184)
    assert use[11]==0 and use[13]==9
    commands=struct.unpack_from('<25h',d,t[3][0]+use[14]*2);assert commands==expected
    failed=anim(436);assert failed[2]==40 and failed[8]-failed[7]==132
    # Failure also swaps the tool back, but does not activate the socket.
    failed_commands=struct.unpack_from('<28h',d,t[3][0]+failed[14]*2)
    assert failed_commands[-7:]==(6,14000,8,6,14112,8,3)
    payload.extend(path.stem.encode().ljust(64,b'\0'));payload.extend(raw(420));payload.extend(struct.pack('<25h',*commands))
    for i in (o[-1]+2,o[-1]+3):
        a=anim(i);assert a[2:7]==(2,0,0,0,0) and a[8]-a[7]==118 and a[11]==0 and a[13]==1
        assert struct.unpack_from('<h',d,t[3][0]+a[14]*2)[0]==4
        # The socket itself is static; treating it as LEVER_SWITCH would yield
        # no physical trajectory. Only Lara's temporary tool moves.
        rate=a[1]&255;size=a[1]>>8
        keys=[d[t[5][0]+a[0]+j*size*2:t[5][0]+a[0]+(j+1)*size*2] for j in range((a[8]-a[7])//rate+2)]
        assert len(set(keys))==1
        payload.extend(raw(i))
    lara=next(x for x in objs if x[0]==0)
    bones=[struct.unpack_from('<4i',d,t[4][0]+lara[3]*4+i*16) for i in range(14)]
    poses=[wrist(struct.unpack_from('<38h',d,t[5][0]+use[0]+i*76),bones) for i in range(119)]
    assert 25<poses[58][7]-poses[46][7]<40 and 100<poses[58][11]-poses[46][11]<120
    for p in poses:payload.extend(struct.pack('<12f',*p))
    print('PASS inserted-tool pry, native cleanup commands and both socket directions:',path.stem)
assert found==['HIGHSTRT','JOBY4A','TRAIN'],found
Path(sys.argv[2]).write_bytes(payload)
