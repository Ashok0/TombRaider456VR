import json,math
from pathlib import Path
rows=json.loads(Path('build/hands-out-capture/inputs.json').read_text());by={r['frame']:r for r in rows}
def add(a,b):return [x+y for x,y in zip(a,b)]
def sub(a,b):return [x-y for x,y in zip(a,b)]
def mul(a,n):return [x*n for x in a]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def mag(a):return math.sqrt(dot(a,a))
def norm(a):return mul(a,1/max(1e-9,mag(a)))
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def basis(row):
 x=norm(sub(row['r'],row['l']));y=norm(sub(row['h'],mul(add(row['r'],row['l']),.5)));z=norm(cross(x,y));return [x,cross(z,x),z]
def rot(v,a,b):return [sum(dot(v,a[j])*b[j][i] for j in range(3)) for i in range(3)]
for lo,hi in [(1677,1737),(1737,1797),(1797,1857),(1900,1930),(2007,2067),(1170,1200)]:
 a,b=by[lo],by[hi];ba,bb=basis(a),basis(b);R=lambda v:rot(v,ba,bb)
 ds=[sub(b[k],R(a[k])) for k in ['h','l','r']];d=mul(add(add(*ds[:2]),ds[2]),1/3)
 print('frames',lo,hi,'translation',sub(b['h'],a['h']),'up',R([0,1,0]),'shapeerr',max(mag(sub(v,d)) for v in ds))
 best=999
 for x in [v/100 for v in range(-40,41,2)]:
  for z in [v/100 for v in range(-40,41,2)]:
   for y in [.6,.8,1,1.2,1.4]:
    p=[x,y,z];err=mag(sub(d,sub(p,R(p))))
    if err<best:best=err;bp=p
 print('best fixed pivot',bp,best)
