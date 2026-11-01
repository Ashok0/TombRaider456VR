from pathlib import Path
import sys, re, json, struct
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from port_addresses import Image
root=Path(__file__).resolve().parents[1]
game=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')
rows=[]
for g,collision,control in [(4,0x8d7e0,0x8d6a0),(5,0x8c6f0,0x8c5a0)]:
 ref=Image(str(root/f'PDB/tomb{g}.dll'));target=Image(str(game/f'{g}/tomb{g}.dll'))
 found=[]
 for start in [collision,control]:
  key=ref.key(start)
  matches=[s for s,e in zip(target.starts,target.ends) if abs((e-s)-(ref.ends[ref.starts.index(start)]-start))<32 and target.key(s)==key]
  assert len(matches)==1,(g,hex(start),matches)
  print(g,hex(start),hex(matches[0]));found.append(matches[0])
 rows.append([g,target.stamp,*found])
 for im,coll in [(ref,collision),(target,found[0])]:
  # Pairs of data references in matched collision recover cog bounds.
  ins=im.disasm(coll)
  print('data',hex(im.stamp),[hex(i[3]) for i in ins if i[3] is not None and i[2].startswith('lea')])
(root/'build/cog-addresses.json').write_text(json.dumps(rows,indent=2))
