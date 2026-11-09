import json,math
from pathlib import Path
rows=json.loads(Path("build/glance-exact-capture/inputs.json").read_text())
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

for lo,hi in [(1268,1674),(1675,2093),(2094,2846),(2847,3013)]:
 total=hit=0;examples=[]
 for i,b in enumerate(rows):
  if not(lo<=b['frame']<=hi) or not b['move'] or i<10:continue
  a=rows[i-10];ba,bb=basis(a),basis(b)
  R=lambda v:rot(v,ba,bb)
  up=R([0,1,0]);tilt=math.acos(max(-1,min(1,up[1])))
  ds=[sub(b[k],R(a[k])) for k in ['h','l','r']];mean=mul(add(add(*ds[:2]),ds[2]),1/3)
  shape=max(mag(sub(v,mean)) for v in ds)
  best=999;pbest=[]
  for x in [-.15,0,.15]:
   for z in [-.25,-.1,.05,.2]:
    for y in [.5,.7,.9,1.1]:
     p=[.025+x,1.609-y,-.142+z];res=mag(sub(mean,sub(p,R(p))))
     if res<best:best=res;pbest=p
  total+=1
  if tilt>.025 and best<.025 and shape<.025:hit+=1
  if total%15==1:examples.append((b['frame'],round(tilt*57.3,1),round(best*1000),round(shape*1000),[round(v,2) for v in pbest]))
 print(lo,hi,'fit',hit,'of',total,'examples',examples)
