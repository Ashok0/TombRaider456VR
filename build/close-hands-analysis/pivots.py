import json,math
from pathlib import Path
def wrap(x):return (x+math.pi)%(2*math.pi)-math.pi
def measure(rows,lo,hi):
 hits=[]
 for i,b in enumerate(rows):
  if not lo<=b['t']-rows[0]['t']<hi:continue
  a=next((a for a in reversed(rows[:i]) if b['t']-a['t']>=.2),None)
  if not a:continue
  ap=[a['r'][k]-a['l'][k] for k in [0,2]];bp=[b['r'][k]-b['l'][k] for k in [0,2]]
  ang=wrap(math.atan2(bp[0],bp[1])-math.atan2(ap[0],ap[1]));span=math.hypot(*bp)
  if abs(ang)<.04 or span<.12:continue
  ma=[(a['r'][k]+a['l'][k])*.5 for k in [0,2]];mb=[(b['r'][k]+b['l'][k])*.5 for k in [0,2]]
  c,s=math.cos(ang),math.sin(ang);tx=mb[0]-(c*ma[0]+s*ma[1]);tz=mb[1]-(-s*ma[0]+c*ma[1]);q=1-c;den=q*q+s*s
  px,pz=(q*tx+s*tz)/den,(-s*tx+q*tz)/den
  x,z=px-b['h'][0],pz-b['h'][2];f=[-bp[1]/span,bp[0]/span]
  back=-(x*f[0]+z*f[1]);side=x*f[1]-z*f[0]
  pred=[mb[0]+c*(a['h'][0]-ma[0])+s*(a['h'][2]-ma[1]),mb[1]-s*(a['h'][0]-ma[0])+c*(a['h'][2]-ma[1])]
  err=math.hypot(pred[0]-b['h'][0],pred[1]-b['h'][2])
  hits.append([b['frame'],round(math.degrees(ang),1),round(back,3),round(side,3),round(err,3)])
 print(lo,hi,'samples',len(hits),'old',sum(.24<math.hypot(h[2],h[3])<.85 and h[4]<.06 for h in hits),'behind',sum(.24<h[2]<.85 and abs(h[3])<h[2]*.5 and h[4]<.06 for h in hits))
 print(hits[::max(1,len(hits)//12)])
for name,parts in [('close-hands-analysis',[(2.5,10),(12,16),(16.5,19.5),(20.5,29.5)]),('swivel-rollback',[(55,60),(75,80),(80,85)]),('hands-out-capture',[(16.5,26)])]:
 print(name);rows=json.loads(Path('build',name,'inputs.json').read_text())
 for lo,hi in parts:measure(rows,lo,hi)
