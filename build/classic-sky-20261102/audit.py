from pathlib import Path
import sys,json,struct
sys.path.insert(0,str(Path('tools').resolve()))
from port_addresses import Image
out=Path(__file__).parent
game=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered')
records=[]
for g,old,new,flush in [(4,0x2f2e0,0x2f150,0xb1b50),(5,0x30120,0x305a0,0xa5460)]:
    ref=Image(str(Path(r'C:\dev\PDB')/f'tomb{g}.dll'));tgt=Image(str(game/str(g)/f'tomb{g}.dll'))
    length=725 if g==4 else 651
    a=list(ref.md.disasm(ref.mem[old:old+length],old))
    b=list(tgt.md.disasm(tgt.mem[new:new+length],new))
    assert [(x.mnemonic,x.size) for x in a]==[(x.mnemonic,x.size) for x in b]
    matches={b[i].operands[0].imm for i,x in enumerate(a) if x.mnemonic=='call' and x.operands[0].type==2 and x.operands[0].imm==flush}
    assert len(matches)==1
    for img,rva,f in [(ref,old,flush),(tgt,new,matches.pop())]:
        stolen=11 if g==4 else 7
        records.append(dict(game=g,stamp=hex(img.stamp),kind='phase',entry=hex(rva),flush=hex(f),
            stolen=stolen,prologue=img.mem[rva:rva+stolen].hex(' '),rip=7 if g==4 else -1,path=img.path))
for path,entry,sysdraw in [(Path(r'C:\dev\PDB\tomb6.dll'),0x1af5e0,0x3ae520),(game/'6/tomb6.dll',0x1afb00,0x3ad480)]:
    img=Image(str(path));records.append(dict(game=6,stamp=hex(img.stamp),kind='room',entry=hex(entry),skyRoom=hex(sysdraw+0x420),
        stolen=6,prologue=img.mem[entry:entry+6].hex(' '),rip=-1,path=img.path))
(out/'456-address-audit.json').write_text(json.dumps(records,indent=2))
print(json.dumps(records,indent=2))
