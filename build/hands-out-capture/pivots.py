exec(open('build/hands-out-capture/geometry.py').read().split('for lo,hi in')[0])
def wrap(x):return (x+math.pi)%(2*math.pi)-math.pi
def yaw(v,a):return [math.cos(a)*v[0]+math.sin(a)*v[2],v[1],-math.sin(a)*v[0]+math.cos(a)*v[2]]
for frame in range(1650,2200,15):
 a,b=by[frame-12],by[frame]
 pa,pb=sub(a['r'],a['l']),sub(b['r'],b['l']);ang=wrap(math.atan2(pb[0],pb[2])-math.atan2(pa[0],pa[2]))
 ma,mb=mul(add(a['l'],a['r']),.5),mul(add(b['l'],b['r']),.5)
 if abs(ang)<.02:continue
 t=sub(mb,yaw(ma,ang));c=1-math.cos(ang);s=math.sin(ang);den=c*c+s*s;p=[(c*t[0]+s*t[2])/den,0,(-s*t[0]+c*t[2])/den]
 err=mag(sub(b['h'],add(mb,yaw(sub(a['h'],ma),ang))))
 print(frame,'deg',round(math.degrees(ang)), 'pitch',round(b['f'][1],2),'pivot',*[round(v,2) for v in p[::2]],'headradius',round(math.hypot(b['h'][0]-p[0],b['h'][2]-p[2]),2),'anchor-radius',round(math.dist(b['anchor'],p[::2]),2),'err',round(err,3))
