import json,math
from pathlib import Path
p=Path(__file__).parent
def load(path):
 return {int(v[1]):[float(v[2]),float(v[3])] for l in path.read_text(encoding='utf-16').splitlines() if l.startswith('POSE,') for v in [l.split(',')]}
r=json.loads(Path('build/swivel-rollback/inputs.json').read_text());t=r[0]['t']
base=load(Path('build/swivel-rollback/restored-replay.log'));candidate=load(p/'walking-replay.log')
for lo,hi in [(0,5),(55,60),(75,80),(80,85)]:
 rr=[x for x in r if lo<=x['t']-t<hi]
 def span(f):return round(max(f[x['frame']][1] for x in rr)-min(f[x['frame']][1] for x in rr),3)
 print(lo,hi,'head',round(max(x['h'][2] for x in rr)-min(x['h'][2] for x in rr),3),'baseline',span(base),'candidate',span(candidate))
v=load(p/'glance-replay.log');rows=[x for x in json.loads(Path('build/hands-out-capture/inputs.json').read_text()) if 1554<=x['frame']<=2200]
for lo,hi in [(1554,1640),(1641,1800),(1801,2000),(2001,2200)]:
 r=[x for x in rows if lo<=x['frame']<=hi]
 changes=[(b['frame'],round(math.dist(v[a['frame']],v[b['frame']])*1000)) for a,b in zip(r,r[1:]) if math.dist(v[a['frame']],v[b['frame']])>.001]
 print(lo,hi,'changes',changes)
