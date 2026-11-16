from pathlib import Path
import math
W=lambda a:math.atan2(math.sin(a),math.cos(a))
def scan(path):
 lines=Path(path).read_text(errors='replace').splitlines();bases={}
 for l in lines:
  if l.startswith('body-basis: '):
   z=l.split();bases[(int(z[1]),z[2])]=list(map(float,z[3:]))
 rr=[list(map(float,l[12:].split(' -> ')[0].split())) for l in lines if l.startswith('body-input: ')];t0=rr[0][1];history=[];hits=[]
 for i,r in enumerate(rr):
  if i%3:continue
  if history and (r[1]-history[-1][1]>.15 or r[2]!=history[-1][2]):history=[]
  history=[a for a in history if r[1]-a[1]<2.]
  span=math.hypot(r[14]-r[11],r[16]-r[13]);f=math.atan2(r[14]-r[11],r[16]-r[13]);mx=(r[11]+r[14])/2;mz=(r[13]+r[16])/2
  if span>=.12 and not r[5]:
   for a in history:
    dt=r[1]-a[1];sp=math.hypot(a[14]-a[11],a[16]-a[13]);turn=W(f-math.atan2(a[14]-a[11],a[16]-a[13]));head=W(r[6]-a[6])
    if dt<.35 or sp<.12 or abs(turn)<1.2 or abs(turn)>2.8 or head*turn<=0 or abs(W(turn-head))>.15 or abs(r[9]-a[9])>.05 or abs(r[18]-a[18])>.12:continue
    ax=(a[11]+a[14])/2;az=(a[13]+a[16])/2
    ex=mx+math.cos(turn)*(a[8]-ax)+math.sin(turn)*(a[10]-az);ez=mz-math.sin(turn)*(a[8]-ax)+math.cos(turn)*(a[10]-az)
    if math.hypot(ex-r[8],ez-r[10])>.04:continue
    c=1-math.cos(turn);sn=math.sin(turn);den=c*c+sn*sn;tx=mx-(math.cos(turn)*ax+math.sin(turn)*az);tz=mz-(-math.sin(turn)*ax+math.cos(turn)*az);px=(c*tx+sn*tz)/den;pz=(-sn*tx+c*tz)/den
    radius=math.hypot(px-r[8],pz-r[10])
    if radius>.18:continue
    A=bases[(int(a[0]),'head')];B=bases[(int(r[0]),'head')]
    dot=sum((math.cos(turn)*A[k]-math.sin(turn)*A[k+6])*B[k]+A[k+3]*B[k+3]+(math.sin(turn)*A[k]+math.cos(turn)*A[k+6])*B[k+6] for k in range(3))
    residual=math.acos(max(-1,min(1,(dot-1)/2)))
    hits.append((r[1]-t0,math.degrees(turn),radius,dt,math.degrees(residual)));break
  history.append(r)
 return hits
for name,path in [('turn','build/turn-reacquire/session.log'),('planted','build/stance-transitions/session.log')]:
 hits=scan(path);print(name,len(hits));print([(round(t,2),round(a,1),round(rad,3),round(dt,2),round(e,1)) for t,a,rad,dt,e in hits[:15]])
