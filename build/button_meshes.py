from pathlib import Path
import sys,struct
sys.path.insert(0,'tools')
from verify_revolver_laser_mesh import vertices
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
for g in (4,5):
 for p in sorted((root/str(g)/'ITEM').glob('*SWITCH*.TRM')):
  if not ('TYPE' in p.name or 'SEQUENCE' in p.name):continue
  m=vertices(p);groups={}
  for v,(b,w) in m.items():groups.setdefault(b[0],[]).append(v)
  print(g,p.stem,[(j,len(v),[(round(min(p[k] for p in v),1),round(max(p[k] for p in v),1)) for k in range(3)]) for j,v in groups.items()])
