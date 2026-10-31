from pathlib import Path
import sys,json
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from port_addresses import Image,Matcher
root=Path('C:/dev/TombRaider456VR');repo=Path('C:/dev/TombRaider123VR')
g=int(sys.argv[1]);flavor=sys.argv[2]
# DoMainControlPage, controls_page, item_data (int16), MC_texts[20],
# mc_first_visible, T_textStrings[64], T_theStrings[64][512].
fields={1:[0x9a2b0,0x174cb4,0x11ee78,0x134c10,0x134bb8,0x12b500,0x12cb00],
2:[0xc46a0,0x1741cc,0x15e3a0,0x1741d0,0x174178,0x16aae0,0x16c0e0],
3:[0x11ade0,0x213140,0x1b49c0,0x1cac60,0x1cabbc,0x1c1570,0x1c2b70]}[g]
ref=Image(str(repo/f'PDB/tomb{g}.dll'));tgt=Image(str(repo/f'build/current-{flavor}-verify/tomb{g}.dll'))
m=Matcher(ref,tgt);result=[];report=[]
native,_=m.code(fields[0]);assert native is not None
aligned=m.align(ref.disasm(fields[0]),tgt.disasm(native))
for index,a in enumerate(fields):
 b,why=m.code(a) if index==0 else m.data(a)
 if b is None:
  candidates=[]
  size=160 if index==3 else 1
  for x,y in aligned:
   # Match the unique text-construction section, not the repeated action
   # switch cases whose near-identical bodies can align to adjacent rows.
   if index==3 and x[0]>=fields[0]+0xa00:continue
   if x[3] is not None and y[3] is not None and a<=x[3]<a+size:
    candidates.append((y[3]-(x[3]-a),'aligned native Options reference '+hex(x[0])))
  if candidates:
   if index==3:assert len(candidates)>=13
   assert len({v for v,_ in candidates})==1,candidates
   b=candidates[0][0];why='all aligned Options references agree: '+str(candidates[:3])
 print(hex(a),hex(b) if b is not None else None,why,flush=True)
 assert b is not None,(hex(a),why)
 result.append(b);report.append([hex(a),hex(b),why])
out={'game':g,'stamp':tgt.stamp,'fields':result,'reference':fields,'report':report,'path':tgt.path,'refpath':ref.path}
(root/f'build/options123-nested/addresses-{g}-{flavor}.json').write_text(json.dumps(out,indent=2))
print(json.dumps(out))
