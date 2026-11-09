exec(open('build/hands-out-capture/geometry.py').read().split('for lo,hi in')[0])
def solve(A,b):
 A=[a[:]+[v] for a,v in zip(A,b)];n=len(b)
 for j in range(n):
  k=max(range(j,n),key=lambda k:abs(A[k][j]));A[j],A[k]=A[k],A[j]
  if abs(A[j][j])<1e-10:return
  d=A[j][j];A[j]=[v/d for v in A[j]]
  for k in range(n):
   if k==j:continue
   d=A[k][j];A[k]=[v-d*w for v,w in zip(A[k],A[j])]
 return [a[-1] for a in A]
def fit(rr):
 A=[];b=[];angles=[]
 for r in rr:
  pair=sub(r['r'],r['l']);th=math.atan2(pair[0],pair[2]);t=r['t']-rr[-1]['t'];c,s=math.cos(th),math.sin(th);angles.append(th)
  A.extend([[1,0,t,0,c,s],[0,1,0,t,-s,c]]);b.extend(r['h'][::2])
 N=[[sum(a[i]*a[j] for a in A) for j in range(6)] for i in range(6)];rhs=[sum(a[i]*v for a,v in zip(A,b)) for i in range(6)];x=solve(N,rhs)
 if not x:return
 rms=math.sqrt(sum((dot(a,x)-v)**2 for a,v in zip(A,b))/(len(b)-6));sig=[]
 for col in [2,3]:
  e=[0]*6;e[col]=1;iv=solve(N,e);sig.append(math.sqrt(max(0,iv[col]))*rms)
 return x[2:4],sig,rms,math.hypot(*x[4:6]),max(angles)-min(angles)
for frame in range(1680,2200,15):
 r=by[frame];rr=[s for s in rows if r['t']-.7<=s['t']<=r['t']][::3];out=fit(rr)
 if out:print(frame,*(tuple(round(v,3) for v in x) if isinstance(x,list) else round(x,3) for x in out))
