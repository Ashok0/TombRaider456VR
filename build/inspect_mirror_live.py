"""Read-only diagnostic of the installed v1 mirror prototype."""
from pathlib import Path
import sys,struct,json,time
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
mod=int(sys.argv[3]);exe=int(sys.argv[4])
try:
    app=value(base+0x66d0e0,'Q');lara=value(base+0x4ee840,'Q')
    item=read(lara,112);room=struct.unpack_from('<h',item,28)[0]
    rooms=value(base+0x65eb68,'Q');r=read(rooms+room*304,304)
    chunk=struct.unpack_from('<Q',r,264)[0]
    layout=value(mod+0x1175c8,'Q');layoutData=read(layout,100)
    state=exe+struct.unpack_from('<I',layoutData,0x2c)[0]
    rt=exe+struct.unpack_from('<I',layoutData,0x4c)[0]
    result={'level':value(app+1264,'i'),'flags':value(app+2532,'I'),'ui':[value(app+x,'i') for x in (1308,2148,2152,2156)],
      'lara':hex(lara),'position':struct.unpack_from('<iii',item,96),'room':room,'rooms':value(base+0x65b230,'h'),
      'roomOrigin':struct.unpack_from('<iii',r,40),'chunks':struct.unpack_from('<i',r,256)[0],
      'chunk':list(struct.unpack('<BBH4f6I',read(chunk,44))) if chunk else None,
      'active':value(mod+0x1181ef,'B'),'scenePose':value(mod+0x117afc,'B'),
      'mirrorFbo':value(mod+0x117a20,'I'),'width':value(mod+0x117a30,'i'),'height':value(mod+0x117af4,'i'),
      'failed':value(mod+0x1177ae,'B'),'reflection':struct.unpack('<12f',read(mod+0x1179f0,48)),
      'surfaceMvp':struct.unpack('<32f',read(mod+0x117a70,128)),
      'poseValid':list(read(mod+0x1191a0+33*48,33)),
      'config':{hex(o):value(mod+0xeab20+o,f) for o,f in ((0x192,'B'),(0xc,'B'),(0x8e,'B'),(0x240,'i'),(0x238,'B'),(0x23c,'i'))}}
    samples=[]
    for i in range(30):
        projection=value(state+104,'Q')
        samples.append({'flags':list(read(mod+0x1177ab,4)),'rt':struct.unpack('<6i',read(rt,24)),
          'projection':struct.unpack('<16f',read(projection,64)) if projection else None})
        time.sleep(.04)
    result['samples']=samples
    Path('build/mirror/live-v1.json').write_text(json.dumps(result,indent=2))
    result['samples']=samples[:3]
    print(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
