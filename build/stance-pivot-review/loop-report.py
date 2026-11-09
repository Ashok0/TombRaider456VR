from pathlib import Path
import json,math
p=Path(__file__).parent
def load(name):return {int(v[1]):dict(anchor=[float(v[2]),float(v[3])],held=int(v[4]),turn=int(v[5]),move=int(v[6])) for l in (p/name).read_text(encoding='utf-16').splitlines() if l.startswith('POSE,') for v in [l.split(',')]}
for folder,output,parts in [('close-hands-analysis','loop-candidate-close.log',[(0,10),(12,16),(16.5,19.5),(20.5,29.5)]),('orientation-analysis','loop-candidate-lowered.log',[(0,10),(10.5,15),(16,19.5),(20,25)]),('swivel-rollback','loop-candidate-walking.log',[(55,60),(75,80),(80,85)])]:
 rows=json.loads(Path('build',folder,'inputs.json').read_text());candidate=load(output);start=rows[0]['t'];print(folder)
 for lo,hi in parts:
  r=[v for v in rows if lo<=v['t']-start<hi];c=[candidate[v['frame']] for v in r]
  span=lambda data:[round(max(v['anchor'][i] for v in data)-min(v['anchor'][i] for v in data),3) for i in [0,1]]
  path=sum(math.dist(a['anchor'],b['anchor']) for a,b in zip(c,c[1:]))
  print(lo,hi,'span old/new',span(r),span(c),'path',round(path,3),'turn old/new',sum(v['turn'] for v in r),sum(v['turn'] for v in c),'move',sum(v['move'] for v in c))
