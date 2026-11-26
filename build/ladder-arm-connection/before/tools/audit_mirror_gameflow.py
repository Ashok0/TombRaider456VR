import pefile,struct,json
from pathlib import Path
from mirror_asset_audit import ROOT456
out=[]
for game,table,count in [(4,0x12aa00,41),(5,0x127890,15)]:
 p=pefile.PE(fr'C:\dev\PDB\tomb{game}.dll',fast_load=True);d=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 retail=(ROOT456/f'{game}/tomb{game}.dll').read_bytes()
 # Argument widths verified from the native DoGameflow dispatch handlers.
 widths={0x80:1,0x81:3,0x82:3,0x83:0,0x84:1,0x85:4,0x86:4,0x87:1,0x88:1,0x89:9,0x8a:2,0x8b:3,0x8c:1,0x8d:7,0x8e:1}
 if game==5:
  dispatch=struct.unpack_from('<17I',d,0x3a264)
  for op in range(0x80,0xd8):
   target=dispatch[d[0x3a2a8+op-0x80]]
   if target in (0x39f4e,0x39f70):widths[op]=1
 for level,ptr in enumerate(struct.unpack_from(f'<{count}Q',d,table)):
  at=ptr-base;record=dict(game=game,levelIndex=level,sequenceRva=hex(at),mirrors=[],commands=[])
  for _ in range(200):
   op=struct.unpack_from('<I',d,at)[0];at+=4
   assert 0x80<=op<=0xd7,(game,level,hex(at),hex(op))
   n=widths.get(op,7);args=struct.unpack_from(f'<{n}I',d,at);at+=n*4
   record['commands'].append([hex(op),list(args)])
   if op==0x8a:record['mirrors'].append(dict(room=args[0],planeZ=args[1]))
   if op in (0x81,0x82):
    record['flags']=args[0];record['fileId']=args[1];record['nativeMirrorEnabled']=bool(args[0]&0x400);break
   assert op!=0x83
  else:raise RuntimeError('Missing level command')
  assert d[ptr-base:at] in retail, (game,level,'Reference sequence absent from installed game')
  record['matchesInstalledRetail']=True
  out.append(record)
Path('build/mirror/native-gameflow-audit.json').write_text(json.dumps(out,indent=2))
print('Parsed',len(out),'level sequences')
print(json.dumps([{k:v for k,v in r.items() if k!='commands'} for r in out if r['mirrors'] or r['nativeMirrorEnabled']],indent=2))
