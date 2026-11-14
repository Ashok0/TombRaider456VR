from pathlib import Path
import json,sys
p=Path(__file__).parent
source=p/(sys.argv[1] if len(sys.argv)>1 else 'baseline-probe.log')
text=source.read_text(encoding='utf-16' if source.read_bytes()[:2] in [b'\xff\xfe',b'\xfe\xff'] else 'utf-8')
rows=[list(map(float,l.split(',')[1:])) for l in text.splitlines() if l.startswith('LAND,')]
for game in [4,5]:
 for clip in [103,24,31,82,92,99]:
  r=[r for r in rows if r[0]==game and r[3]==clip]
  if not r:continue
  print(game,clip,'worst foot:',max(r,key=lambda v:v[5]),'worst knee:',max(r,key=lambda v:v[6]))
poses=json.loads((p/'native-poses.json').read_text())
for game in [4,5]:
 r=[v for v in poses if v['game']==game and v['clip']==24]
 for j in [0,2,3,5,6,14]:
  print('native',game,'joint',j,'Y range',min(v['pose'][j][1][1] for v in r),max(v['pose'][j][1][1] for v in r))
