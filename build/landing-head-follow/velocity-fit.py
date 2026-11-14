import json,math,statistics
from pathlib import Path
r=json.loads(Path('build/landing-head-follow/inputs.json').read_text())
def solve(A,b):
 m=[sum(([sum(row[i]*row[j] for row in A) for j in range(4)],[sum(row[i]*v for row,v in zip(A,b))]),[]) for i in range(4)]
 for k in range(4):
  q=max(range(k,4),key=lambda i:abs(m[i][k]));m[k],m[q]=m[q],m[k]
  if abs(m[k][k])<1e-9:return None
  v=m[k][k];m[k]=[x/v for x in m[k]]
  for i in range(4):
   if i!=k:
    v=m[i][k];m[i]=[x-v*y for x,y in zip(m[i],m[k])]
 return [m[i][4] for i in range(4)]
def fit(rows,key):
 p=lambda a: ([a['h'][0],a['h'][2]] if key=='h' else [(a['l'][i]+a['r'][i])*.5 for i in (0,2)])
 yaw=lambda a:math.atan2(a['r'][0]-a['l'][0],a['r'][2]-a['l'][2])
 a=rows[0];x,z=p(a);base=yaw(a);A=[];b=[]
 for v in rows[1:]:
  ang=yaw(v)-base;c,s=math.cos(ang),math.sin(ang);t=v['t']-a['t'];px,pz=p(v)
  A.extend([[1-c,-s,t,0],[s,1-c,0,t]]);b.extend([px-c*x-s*z,pz+s*x-c*z])
 q=solve(A,b)
 if q is None:return None
 err=math.sqrt(sum((sum(x*y for x,y in zip(row,q))-v)**2 for row,v in zip(A,b))/len(A))
 return q,err
out=[]
for i,a in enumerate(r):
 t=a['t']-r[0]['t']
 if t>25:break
 w=[v for v in r[max(0,i-50):i+1] if a['t']-v['t']<.65][::3]
 if len(w)<8:continue
 h=fit(w,'h');m=fit(w,'m')
 if h and m:
  out.append([t,*h[0][2:],h[1],*m[0][2:],m[1]])
for lo in range(0,25,5):
 a=[v for v in out if lo<=v[0]<lo+5]
 print(lo,'midvelocity median',*[round(statistics.median(abs(v[k]) for v in a),3) for k in [4,5]],'noise max',max(round(math.hypot(v[4],v[5]),2) for v in a),'examples',[[round(x,2) for x in v] for v in a[::60]])
Path('build/landing-head-follow/velocity-fit.json').write_text(json.dumps(out))
