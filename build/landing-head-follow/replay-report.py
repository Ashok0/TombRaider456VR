from pathlib import Path
import json,math
w=Path('build/landing-head-follow')
for name,folder,parts in [('new','landing-head-follow',[(0,5),(5,10),(10,15),(15,20),(20,25)]),('old','close-hands-analysis',[(0,10),(12,16),(16.5,19.5),(20.5,29.5)]),('walk','swivel-rollback',[(55,60),(75,80),(80,85)])]:
 r=json.loads(Path('build',folder,'inputs.json').read_text()); c={int(v[1]):dict(anchor=list(map(float,v[2:4])),turn=int(v[5]),move=int(v[6])) for l in (w/f'replay-{name}.log').read_text(encoding='utf-16').splitlines() if l.startswith('POSE,') for v in [l.split(',')]}
 print(name)
 for lo,hi in parts:
  a=[x for x in r if lo<=x['t']-r[0]['t']<hi]; b=[c[x['frame']] for x in a]
  span=lambda rr: [round(max(x['anchor'][i] for x in rr)-min(x['anchor'][i] for x in rr),3) for i in (0,1)]
  print(lo,hi,span(a),span(b),'turn',sum(x['turn'] for x in b),'move',sum(x['move'] for x in b))
