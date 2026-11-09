exec(open('build/orientation-analysis/basis.py').read().split('for r in rows[::30]')[0])
def twist(old,new):
 d=[[sum(new[i*3+k]*old[j*3+k] for k in range(3)) for j in range(3)] for i in range(3)]
 return -math.degrees(2*math.atan2(d[0][2]-d[2][0],1+d[0][0]+d[1][1]+d[2][2]))
def axis(old,new):
 d=[[sum(new[i*3+k]*old[j*3+k] for k in range(3)) for j in range(3)] for i in range(3)]
 q=[d[2][1]-d[1][2],d[0][2]-d[2][0],d[1][0]-d[0][1]]
 return abs(q[1])/max(1e-9,math.sqrt(sum(v*v for v in q)))
for i in range(30,len(rows)-20,30):
 a,b=rows[i-12],rows[i];yawChanges=[twist(a['basis'][k],b['basis'][k]) for k in ['head','left','right']]
 print(round(b['t']-rows[0]['t'],1),b['frame'],'projected head/pair',round(diff(yaw(b['basis']['head']),yaw(a['basis']['head'])),1),round(diff(pair(b),pair(a)),1),'world twist H/L/R',*[round(v,1) for v in yawChanges],'head vertical axis',round(axis(a['basis']['head'],b['basis']['head']),2))
