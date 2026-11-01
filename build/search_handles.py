from pathlib import Path
import sys,collections
sys.path.insert(0,'tools')
from verify_revolver_laser_mesh import vertices
root=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered/5/ITEM')
for f in sorted(root.glob('SEARCH_OBJECT[12]*.TRM')):
 print(f.name)
 mesh=vertices(f)
 for joint in ((0,1) if 'OBJECT2' in f.name else (1,2)):
  p=[p for p,(j,w) in mesh.items() if j[0]==joint];z=max(v[2] for v in p)
  tips=[tuple(round(x,2) for x in v) for v in p if v[2]>z-15]
  print(joint,'tips',tips)
