
import math,statistics,re
from pathlib import Path
rows=[]
for l in Path('build/stance-transitions/session.log').read_text(errors='replace').splitlines():
 if l.startswith('body-input: '):
  a=l.split();rows.append(dict(frame=int(a[1]),t=float(a[2]),eye=list(map(float,a[9:12])),left=list(map(float,a[12:15])),right=list(map(float,a[15:18])),yaw=float(a[7]),f=list(map(float,a[18:21]))))
start=rows[0]['t']
def flat(v):return (v[0],v[2])
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def rot(a,t):return (a[0]*math.cos(t)+a[1]*math.sin(t),-a[0]*math.sin(t)+a[1]*math.cos(t))
def norm(v):return math.hypot(*v)
def angle(a):return math.remainder(a,2*math.pi)
def measure(rows):
 result=[];j=0
 for i,s in enumerate(rows):
  while j<i and s['t']-rows[j]['t']>.20:j+=1
  o=rows[j]
  if s['t']-o['t']<.15:continue
  b=sub(flat(s['right']),flat(s['left']));a=sub(flat(o['right']),flat(o['left']))
  t=angle(math.atan2(b[0],b[1])-math.atan2(a[0],a[1]));ht=angle(s['yaw']-o['yaw'])
  if abs(t)<.06 or abs(t)>.7:continue
  m=tuple((x+y)/2 for x,y in zip(flat(s['right']),flat(s['left'])));om=tuple((x+y)/2 for x,y in zip(flat(o['right']),flat(o['left'])))
  error=norm(sub(add(m,rot(sub(flat(o['eye']),om),t)),flat(s['eye'])))
  tr=sub(m,rot(om,t));c=1-math.cos(t);sn=math.sin(t);den=c*c+sn*sn
  p=((c*tr[0]+sn*tr[1])/den,(c*tr[1]-sn*tr[0])/den)
  radius=norm(sub(p,flat(s['eye'])))
  result.append((s['t']-rows[0]['t'],radius,error,t,ht))
 return result
v=measure(rows)
for lo,hi in [(0,3.8),(8.3,10.5),(10.5,14.9),(35,41),(43,48),(57.2,79),(80,99),(100,110)]:
 a=[r for r in v if lo<r[0]<hi];good=[r for r in a if r[2]<.03 and r[3]*r[4]>0 and abs(r[3]-r[4])<max(.04,abs(r[3])*.5)]
 print(lo,hi,'N',len(a),'rigid',len(good),'radius quantiles',([round(statistics.quantiles([r[1] for r in good],n=4)[k],3) for k in range(3)] if len(good)>1 else []),'near',sum(r[1]<.24 for r in good))
# Export prior labelled capture from C++ initialization.
old=[]
for l in Path('tools/close_hands_motion_regression.inl').read_text().splitlines():
 if not re.match(r'^\{\d+,',l):continue
 a=[float(s.rstrip('f')) for s in re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?f?',l,re.I)]
 old.append(dict(t=a[1],yaw=a[4],eye=a[6:9],left=a[9:12],right=a[12:15],f=a[15:18]))
v=measure(old)
for lo,hi in [(0,10),(12,19.5),(20.5,29.5)]:
 good=[r for r in v if lo<r[0]<hi and r[2]<.03 and r[3]*r[4]>0 and abs(r[3]-r[4])<max(.04,abs(r[3])*.5)]
 print('OLD',lo,hi,'N',len(good),'radius',([round(statistics.quantiles([r[1] for r in good],n=4)[k],3) for k in range(3)] if len(good)>1 else []),'near',sum(r[1]<.24 for r in good))
