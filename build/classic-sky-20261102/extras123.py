from pathlib import Path
import hashlib,json
root=Path(r'C:\dev\TombRaider123VR');out=Path(__file__).parent;records=[]
def stage(rel,text):
    path=root/rel;original=path.read_bytes() if path.exists() else None
    if original is not None:
        p=out/'123-extra-before'/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(original)
    p=out/'123-extra-staged'/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(text.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))
    records.append(dict(path=rel,sha256=hashlib.sha256(original).hexdigest() if original else None))
text=(root/'README.md').read_text()
text=text.replace('- Sky at optical infinity, so the HD dome does not sit a few metres away in\n  stereo or paint over distant geometry.', '- Sky at optical infinity for Remastered TR1–3 and Classic TR2/3 horizons,\n  so backgrounds do not sit a few metres away in stereo or cover distant scenery.\n  TR1 has no separate Classic horizon renderer.')
text=text.replace('The sky fix hooks `DrawSkyHD` in the live game DLL. `SkyAtInfinity=0` returns', 'The sky fix hooks `DrawSkyHD` and the verified Classic TR2/3 horizon draw call.\n`SkyAtInfinity=0` returns')
text=text.replace('The PDBs say this is the only sky draw path. There is no `DrawFlatSky`,\n`DrawSkySegment` or `SkyDrawPhase` in these DLLs.', 'This is the Remastered sky path. Classic TR2/3 instead draw a horizon mesh\nthrough `ins_mesh` inside `DrawRooms`. There is no separate `DrawFlatSky`,\n`DrawSkySegment` or `SkyDrawPhase` entry point in these DLLs.')
text=text.replace('data, not a function. One hook covers every sky triangle the remaster issues.', 'data, not a function. `DrawSkyHD` covers the Remastered dome. The Classic\nhook only recognizes the verified horizon call site, flushes preceding world\ncommands before entering its scope, and submits the queued horizon inside the\nscope. Ordinary object meshes keep native stereo depth. All six supported\nTR2/3 DLL builds have verified addresses in `src/SkyBindings.h`; TR1 needs no\nadditional Classic hook. The existing `SkyAtInfinity=1` default enables both paths.')
text=text.replace('sky: first DrawSkyHD draw','sky: first sky draw')
text=text.replace('Sky.cpp          DrawSkyHD hook: sky draws at optical infinity','Sky.cpp          Classic/Remastered scopes: sky draws at optical infinity')
stage('README.md',text)
stage('src/Config.h',(root/'src/Config.h').read_text().replace('Draw the HD sky at optical infinity. 1 = on and the default.','Draw Classic/Remastered sky backgrounds at optical infinity. Default on.'))
text=(out/'sky_selftest123.cpp').read_text().replace('C:/dev/TombRaider123VR/src/','../src/')
stage('tests/sky_selftest.cpp',text)
(out/'123-extra-manifest.json').write_text(json.dumps(records,indent=2))
print('Staged TR1-3 documentation and focused test source.')
