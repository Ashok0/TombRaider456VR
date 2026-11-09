from pathlib import Path
import math,json
p=Path(__file__).parent;rows=json.loads((p/'inputs.json').read_text());by={r['frame']:r for r in rows}
for line in (p/'session.log').read_text().splitlines():
 if line.startswith('body-basis: '):
  v=line.split();frame=int(v[1]);by[frame].setdefault('basis',{})[v[2]]=list(map(float,v[3:]))
 if line.startswith('body-height: '):
  v=line.split();by[int(v[1])]['height']=list(map(float,v[2:]))
(p/'poses.json').write_text(json.dumps(rows))
def yaw(m):return math.degrees(math.atan2(-m[2],m[8]))
def pair(r):return math.degrees(math.atan2(-(r['r'][2]-r['l'][2]),r['r'][0]-r['l'][0]))
def diff(a,b):return (a-b+180)%360-180
for r in rows[::30]:
 if 'basis' not in r:continue
 print(round(r['t']-rows[0]['t'],1),r['frame'],'xyz',tuple(round(x,2) for x in r['h']),'head/pair/L/R',tuple(round(x) for x in [r['yaw']*180/math.pi,pair(r),yaw(r['basis']['left']),yaw(r['basis']['right'])]),'pitch',round(math.degrees(math.asin(r['f'][1]))),'arms',tuple(round(x,2) for x in r['l']),'hold/turn/move',r['held'],r['turn'],r['move'])
