"""Extract native standing/landing skeletons for the first-person renderer tests.
Usage: python tools/extract_landing_poses.py <TR4-6 directory> <TR1-3 directory> <output.inl>
Reads installed PDP assets only. TR1 uses fixed 40-word frames with a joint
count and low/high packed rotations; TR2-5 use variable compact rotations.
No game code is executed and no installed assets are modified.
"""
from pathlib import Path
import math,struct,sys
if len(sys.argv)!=4:raise SystemExit(__doc__)
root=Path(sys.argv[1]);root123=Path(sys.argv[2]);output=Path(sys.argv[3])
def mul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def vec(a,v):return [sum(a[i][k]*v[k] for k in range(3)) for i in range(3)]
def skeleton(w,bones,game):
    p=10 if game==1 else 9;rots=[]
    for j in range(15):
        v=w[p]&65535;p+=1;kind=0 if game==1 else v>>14;x=y=z=0
        if kind:
            angle=(v&4095)*math.tau/4096
            if kind==1:x=angle
            elif kind==2:y=angle
            else:z=angle
        else:
            packed=((w[p]&65535)<<16)|v if game==1 else (v<<16)|(w[p]&65535);p+=1
            x=((packed>>20)&1023)*math.tau/1024;y=((packed>>10)&1023)*math.tau/1024;z=(packed&1023)*math.tau/1024
        cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
        rots.append(mul(mul([[cy,0,sy],[0,1,0],[-sy,0,cy]],[[1,0,0],[0,cx,-sx],[0,sx,cx]]),[[cz,-sz,0],[sz,cz,0],[0,0,1]]))
    r=rots[0];p=list(w[6:9]);stack=[];out=[(r,p)]
    for j,(flags,*v) in enumerate(bones,1):
        if flags&1:r,p=stack.pop()
        if flags&2:stack.append((r,p))
        d=vec(r,v);p=[p[k]+d[k] for k in range(3)];r=mul(r,rots[j]);out.append((r,p))
    return out
cases=[]
for game,level in [(1,'LEVEL1'),(2,'ASSAULT'),(3,'JUNGLE'),(4,'ANGKOR1'),(5,'ANDREA1')]:
    collection=root if game>=4 else root123
    path=collection/str(game)/'DATA'/f'{level}.PDP'
    assert path.exists(),path
    data=path.read_bytes();at=0;t=[]
    for stride in [40 if game>=4 else 32,6,8,2,4,2,18]:
        n=struct.unpack_from('<I',data,at)[0];at+=4;t.append((at,n));at+=n*stride
    objects=[struct.unpack_from('<IhhIIh',data,t[6][0]+i*18) for i in range(t[6][1])]
    lara=next(o for o in objects if o[0]==0);assert lara[1]==15
    bones=[struct.unpack_from('<4i',data,t[4][0]+lara[3]*4+i*16) for i in range(14)]
    for clip in [103,24,31,82,92,99]:
        if game>=4:a=struct.unpack_from('<Ihhiiii8h',data,t[0][0]+clip*40)
        else:
            v=struct.unpack_from('<Ihhi i8h',data,t[0][0]+clip*32)
            a=v[:5]+(0,0)+v[5:]
        if a[2] not in [1,2]:continue
        rate=a[1]&255;size=40 if game==1 else (a[1]&65535)>>8
        for f in range(1 if clip==103 else (a[8]-a[7])//rate+1):
            words=struct.unpack_from('<'+str(size)+'h',data,t[5][0]+a[0]+f*size*2)
            if game==1:assert words[9]==15
            cases.append(dict(game=game,level=path.stem,clip=clip,frame=f*rate,pose=skeleton(words,bones,game)))
def f(x):return f'{x:.9f}f'
code='struct NativeLandingPose { int game,clip,frame;tr::motiongun::Frame joints[15]; };\nstatic const NativeLandingPose nativeLandingPoses[]={\n'
for c in cases:
    code+='{'+f'{c["game"]},{c["clip"]},{c["frame"]},'+'{'
    code+=','.join('{{{'+','.join('{'+','.join(map(f,row))+'}' for row in r)+'}},{'+','.join(map(f,p))+'}}' for r,p in c['pose'])
    code+='}},\n'
code+='};\n'
output.write_text(code,encoding='utf-8')
print('Extracted',len(cases),'native landing/standing keys',[(c['game'],c['level']) for c in cases if c['clip']==103])
