from pathlib import Path
import math
rows=[v.split(',') for v in Path('build/hands-out-capture/candidate.log').read_text(encoding='utf-16').splitlines() if v.startswith('POSE,')]
for lo,hi in [(657,1640),(1640,2200),(657,2200)]:
 r=[v for v in rows if lo<=int(v[1])<=hi]
 print(lo,hi,'travel',sum(math.hypot(float(a[2])-float(b[2]),float(a[3])-float(b[3])) for a,b in zip(r,r[1:])),'turn',sum(v[5]=='1' for v in r))
 print('movement frames',[int(b[1]) for a,b in zip(r,r[1:]) if math.hypot(float(a[2])-float(b[2]),float(a[3])-float(b[3]))>.001][:80])
