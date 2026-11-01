"""Offline keypad audit. Reads PE/TRM/PDP files; never loads native game code."""
from pathlib import Path
import struct,sys
from port_addresses import Image
from verify_revolver_laser_mesh import vertices
repo=Path(__file__).resolve().parents[1]
root=Path(sys.argv[1])
rows=[(0x696b499c,repo/'PDB/tomb5.dll',0x744a0,0x6e6f0,750,0x69270,703,0x10bf0,0),
      (0x68c12fe9,root/'5/tomb5.dll',0x75bc0,0x6fdb0,754,0x69d10,705,0x10cf0,-0xc0)]
def fingerprint(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h
aliases={'lea r21,m[107,0,1,1,0]4':'mov r21,i1',
         'lea r226,m[40,0,1,2,0]4':'mov r226,i2',
         'lea r22,m[40,0,1,10,0]4':'mov r22,i110'}
def whole(im,start,size):
    code=[x for s in im.starts if start<=s<start+size for x in im.disasm(s)]
    assert code[-1][0]+code[-1][1]==start+size and code[-1][2]=='ret '
    tokens=[aliases.get(x[2],x[2]) for x in code if not x[2].startswith('nop')]
    for i in range(len(tokens)-1):
        if tokens[i:i+2]==['movzx r22,r5','xor r24,r24']:tokens[i:i+2]=reversed(tokens[i:i+2])
    return code,tokens
reference=None;output=['// Verified full native keypad routines, not just their first unwind fragment.']
for stamp,file,collision,step,stepSize,finish,finishSize,trigger,delta in rows:
    im=Image(str(file));assert im.stamp==stamp
    sc,st=whole(im,step,stepSize);fc,ft=whole(im,finish,finishSize)
    if reference is None:reference=(st,ft)
    else:assert reference==(st,ft),'native keypad behavior changed'
    fields=[0x65ebf8,0x65ec40,0x1b6a58,0x1b6a5c,0x1b6a54,0x8488dc,
            0x1b6a6e,0x1b6a6f,0x1b6b7a,0x1b6b7b,0x1b6b98,0x8460bc]
    fields=[a+delta for a in fields]+[0x1b284c]+[a+delta for a in (0x55a1f5,0x55a205,0x6107d5,0x6107d7,0x612cfa,0x8460b3,0x8460ad)]
    refs={x[3] for x in sc+fc}
    assert all(a in refs for a in fields)
    assert any(x[4]==trigger for x in fc)
    mask=0xb62e0 if not delta else 0xb6430
    mc=list(im.md.disasm(im.mem[mask:mask+97],mask))
    assert mc[0].bytes==bytes.fromhex('48 0f bf 41 10') and mc[-1].mnemonic=='ret' and mc[-1].address==mask+96
    if not delta:referenceMask=[(i.mnemonic,i.op_str) for i in mc if 'rip' not in i.op_str and i.mnemonic[0]!='j']
    else:assert referenceMask==[(i.mnemonic,i.op_str) for i in mc if 'rip' not in i.op_str and i.mnemonic[0]!='j']
    # Known native launch branch: slot 249 in levels 11/12, OCB supplied as
    # combination, no inventory puzzle-item consumption.
    cc=[x for s in im.starts if collision<=s<collision+1812 for x in im.disasm(s)]
    assert any(x[3]==0x1b6b98+delta for x in cc)
    output.append('    {'+','.join([hex(stamp),hex(collision),hex(step),str(stepSize),hex(fingerprint(im.mem[step:step+stepSize]))+'ull',
                                  hex(finish),str(finishSize),hex(fingerprint(im.mem[finish:finish+finishSize]))+'ull',
                                  hex(mask),hex(fingerprint(im.mem[mask:mask+97]))+'ull',hex(trigger)]+[hex(a) for a in fields])+'},')
    print('PASS full keypad input/completion and globals:',hex(stamp))
generated='\n'.join(output)+'\n'
addressFile=repo/'src/KeypadAddresses.inl'
if '--write-addresses' in sys.argv:addressFile.write_text(generated)
else:assert addressFile.read_text()==generated
mesh=vertices(root/'5/ITEM/PUZZLE_HOLE8_1.TRM')
left=[14.63,-13.94,-42.52];right=[42.46,13.89,-14.69]
top=[-55.31,-25.79,3.88,33.44];bottom=[-26.35,3.17,32.84,62.40]
for key in range(12):
    points=[p for p,s in mesh.items() if s[0][0]==key+13]
    lo=[min(p[k] for p in points) for k in range(3)];hi=[max(p[k] for p in points) for k in range(3)]
    col,row=key%3,key//3
    for a,b in zip(lo,[left[col],-26,top[row]]):assert abs(a-b)<.01
    for a,b in zip(hi,[right[col],-11,bottom[row]]):assert abs(a-b)<.01
print('PASS all 12 rendered key faces and native key ordering')
poses=[]
for level in ('RICH1','RICH2'):
    d=(root/f'5/DATA/{level}.PDP').read_bytes();at=0;t=[]
    for stride in (40,6,8,2,4,2,18):
        n=struct.unpack_from('<I',d,at)[0];at+=4;t.append((at,n));at+=n*stride
    objects=[struct.unpack_from('<IhhIIh',d,t[6][0]+i*18) for i in range(t[6][1])]
    obj=next(o for o in objects if o[0]==249);assert obj[1]==25
    a=struct.unpack_from('<Ihhiiii8h',d,t[0][0]+obj[-1]*40)
    f=struct.unpack_from('<'+str(a[1]>>8)+'h',d,t[5][0]+a[0]);assert a[6]==a[7]
    poses.append((obj[-1],a[6],*f[6:9],f[9]))
    print('PASS keypad authored root:',level,poses[-1])
Path('build/keypad-poses.txt').write_text('\n'.join(' '.join(map(str,p)) for p in poses)+'\n')
# The input contact follows the same rest-hand meshes selected by the mod.
for variant in ('BARE','GLOVES','XRAY'):
    points=vertices(root/f'4/ITEM/HAND_{variant}_REST.TRM')
    for hand,joint in ((0,13),(1,10)):
        tip=(57 if hand else -57,79,10)
        dist=min(sum((p[k]-tip[k])**2 for k in range(3))**.5 for p,s in points.items() if s[0][0]==joint)
        assert dist<6,(variant,hand,dist)
print('PASS left/right contacts within 6 native units of visible rest fingertips')
