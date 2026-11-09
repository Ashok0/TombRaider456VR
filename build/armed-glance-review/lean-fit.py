import json,math
from pathlib import Path
rows=json.loads(Path('build/armed-glance-review/samples.json').read_text())
def add(a,b):return [x+y for x,y in zip(a,b)]
def sub(a,b):return [x-y for x,y in zip(a,b)]
def mul(a,n):return [x*n for x in a]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def mag(a):return math.sqrt(dot(a,a))
def norm(a):return mul(a,1/max(1e-9,mag(a)))
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def basis(row):
 x=norm(sub(row['right'],row['left']));y=norm(sub(row['head'],mul(add(row['right'],row['left']),.5)));z=norm(cross(x,y));return [x,cross(z,x),z]
def rot(v,a,b):return [sum(dot(v,a[j])*b[j][i] for j in range(3)) for i in range(3)]
for begin,end in [(1200,1500),(2450,2670),(880,1150)]:
 q=[r for r in rows if begin<=r['pose']<end];a=q[0];ba=basis(a);pivot=sub(a['head'],[0,.75,0])
 print('reference',a['pose'],a['head'])
 for b in q[::5]:
  bb=basis(b);pred=add(pivot,rot(sub(a['head'],pivot),ba,bb));err=sub(b['head'],pred)
  rigid=mag(sub(rot(sub(a['head'],a['left']),ba,bb),sub(b['head'],b['left'])))
  print(int(b['pose']),'observed',round(mag(sub(a['head'],b['head']))*1000),'residual',round(mag(err)*1000),'vertical',round(err[1]*1000),'rigid error',round(rigid*1000))
