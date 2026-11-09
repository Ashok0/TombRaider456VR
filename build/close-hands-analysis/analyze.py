import re,json,math
from pathlib import Path
p=Path(__file__).parent
rows=[];placements=[];reset=True
for ln,line in enumerate((p/'session.log').read_text(errors='replace').splitlines()):
 if line.startswith('body-input-reset'):reset=True
 if line.startswith('body-input: '):
  a,b=line[12:].split(' -> ');v=list(map(float,a.split()));o=list(map(float,b.split()))
  rows.append(dict(line=ln,frame=int(v[0]),t=v[1],base=v[2],body=v[3],tracked=v[4],walking=v[5],yaw=v[6],neck=v[7],h=v[8:11],l=v[11:14],r=v[14:17],f=v[17:20],anchor=o[:2],held=o[2],turn=o[3],move=o[4],reset=reset));reset=False
 if line.startswith('body-placement:'):
  d={k:float(v) for k,v in re.findall(r'(pose|held|move|turn|state|yaw|posture)=([-\d.]+)',line)}
  for k,v in re.findall(r'(head|left|right|anchor|scene|hip|chest)=\(([^)]+)\)',line):d[k]=list(map(float,v.split(',')))
  placements.append(d)
(p/'inputs.json').write_text(json.dumps(rows))
(p/'placements.json').write_text(json.dumps(placements))
dist=lambda a,b:math.dist(a,b)
start=rows[0]['t'];prev=None
for sec in range(0,int(rows[-1]['t']-start)+1,5):
 rr=[r for r in rows if sec<=r['t']-start<sec+5]
 if not rr:continue
 pp=[r for r in placements if rr[0]['frame']<=r['pose']<=rr[-1]['frame']]
 path=sum(dist(a['anchor'],b['anchor']) for a,b in zip(rr,rr[1:]) if not b['reset'])
 span=lambda key,ix:round(1000*(max(r[key][ix] for r in rr)-min(r[key][ix] for r in rr)))
 hips=[r['hip'] for r in pp];hipspan=[round((max(h[i] for h in hips)-min(h[i] for h in hips))/423*1000) for i in [0,2]] if hips else []
 print(sec,rr[0]['frame'],rr[-1]['frame'],'path',round(path*1000),'move',sum(r['move']>0 for r in rr),'turn',sum(r['turn']>0 for r in rr),'stick',sum(r['walking']>0 for r in rr),'headXZ',span('h',0),span('h',2),'handY',round(rr[len(rr)//2]['l'][1],2),'headY',round(rr[len(rr)//2]['h'][1],2),'hipXZspan',hipspan)
