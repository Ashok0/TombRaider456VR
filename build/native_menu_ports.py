import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from port_addresses import Image,Matcher
g=int(sys.argv[1]); flavor=sys.argv[2]
src=Path('C:/dev/TombRaider123VR')
ref=Image(str(src/f'PDB/tomb{g}.dll'));tgt=Image(str(src/f'build/current-{flavor}-verify/tomb{g}.dll'))
fields={
1:[0x1fea0,0x34230,0x106908,0x11ee08,0x4f87e0,0x41dd94,0x41d6a0,0x119f90],
2:[0x3b890,0x625b0,0x141da8,0x15e334,0x537940,0x4331e0,0x432790,0x159870],
3:[0x4fe70,0x95ed0,0x192738,0x1b4950,0x596a40,0x491f74,0x4907d0,0x1afbb0],
}[g]
m=Matcher(ref,tgt);result=[];report=[]
for a in fields:
 b,why=m.code(a) if ref.section_of(a)=='.text' else m.data(a)
 if b is None: b,why=m.data(a)
 assert b is not None,(a,why)
 result.append(b);report.append([hex(a),hex(b),why])
out={'game':g,'stamp':tgt.stamp,'fields':result,'report':report}
Path(f'build/native-port{g}-{flavor}.json').write_text(json.dumps(out,indent=2))
print(json.dumps(out))
