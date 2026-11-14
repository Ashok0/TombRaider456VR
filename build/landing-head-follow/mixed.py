from pathlib import Path
import json
w=Path('build/landing-head-follow');r=json.loads((w/'inputs.json').read_text());t0=r[0]['t']
for name,axis,sign in [('right',0,1),('left',0,-1),('forward',2,1),('back',2,-1)]:
 lines=['body-input-reset']
 for a in r:
  t=a['t']-t0
  if t>=25:break
  travel=max(0,min(t-5,7))*.25*sign
  vectors=[]
  for key in ('h','l','r'):
   v=a[key].copy();v[axis]+=travel;vectors+=v
  values=[a['frame'],a['t'],a['base'],a['body'],int(a['tracked']),int(a['walking']),a['yaw'],a['neck']]+vectors+a['f']
  lines.append('body-input: '+' '.join(map(str,values))+' -> 0 0 1 0 0')
 (w/f'walking-{name}.log').write_text('\n'.join(lines))
