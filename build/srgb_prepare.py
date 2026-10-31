from pathlib import Path
import hashlib,json,shutil,re
root=Path('C:/dev/TombRaider456VR');src=Path('C:/dev/TombRaider123VR');stage=root/'build/vr-options123';work=root/'build/srgb-option'
work.mkdir(exist_ok=True)
names=['README.md','TombRaiderVR.ini','src/DefaultIni.h','src/Config.h','src/VRSystem.cpp','src/StereoRenderer.cpp','src/StereoRenderer.h','src/Engine.h']
baseline={}
for name in names:
 baseline[name]=hashlib.sha256((src/name).read_bytes()).hexdigest()
 for repo,label in [(root,'456'),(src,'123')]:
  dest=work/'before'/label/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,dest)
 shutil.copy2(src/name,stage/name)
(work/'baseline123.json').write_text(json.dumps(baseline,indent=2))
sources={}
for p in (src/'src').rglob('*'):
 if p.is_file():
  rel=p.relative_to(src);sources[str(rel)]=hashlib.sha256(p.read_bytes()).hexdigest();shutil.copy2(p,stage/rel)
(work/'sources123.json').write_text(json.dumps(sources,indent=2))
for repo in [root,stage]:
 p=repo/'src/VRSystem.cpp';s=p.read_text();start=s.index('    // Preserve the existing darker presentation');end=s.index('\n',s.index('    t.eColorSpace =',start))
 s=s[:start]+'''    // Both schemes submit display-ready pixels. Brighter restores the native
    // sRGB eye/scratch texture format; a compositor flag alone did not produce
    // the earlier brighter result on the user's headset.
    t.eColorSpace = vr::ColorSpace_Gamma;'''+s[end:];p.write_text(s)
 p=repo/'src/Config.h';s=p.read_text().replace('Brighter applies display encoding.','Brighter enables native sRGB render targets.');p.write_text(s)
 p=repo/'src/Engine.h';s=p.read_text().replace('toggles GL_FRAMEBUFFER_SRGB (0x8F9D)','toggles GL_PRIMITIVE_RESTART (0x8F9D)');p.write_text(s)
 p=repo/'TombRaiderVR.ini';s=p.read_text().replace('; Takes effect immediately when changed in VR Options; only affects the headset.','; Brighter restores native sRGB rendering, as in the October 26 color fix.\n; Takes effect on the next frame when changed in VR Options.');p.write_text(s)
 p=repo/'README.md';s=p.read_text(encoding='utf-8').replace('Brighter lifts midtones with display\n  encoding to give a look closer to the brighter flat game; an exact monitor match\n  depends on the headset and display. It does not change game lighting or the desktop mirror.', 'Brighter restores the native sRGB eye and scratch render targets from the\n  October 26 color correction. This lets remastered rendering apply its normal\n  color conversion before submission. Classic rendering retains its native\n  conversion state. This changes neither game lighting nor saturation; final\n  headset appearance still depends on the display.');p.write_text(s,encoding='utf-8')
print('Saved source baselines and prepared optional native sRGB correction.')
