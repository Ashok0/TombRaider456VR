from pathlib import Path
import hashlib,json,shutil
root=Path('C:/dev/TombRaider456VR');src=Path('C:/dev/TombRaider123VR');stage=root/'build/vr-options123';work=root/'build/color-scheme'
work.mkdir(exist_ok=True)
names=['README.md','TombRaiderVR.ini','src/Config.h','src/Config.cpp','src/DefaultIni.h','src/VRSystem.cpp','src/VRMenuModel.h','src/VRMenuSettings.h','src/VRMenuSettings.inl','src/VRNativeMenu.cpp','tools/vr_menu_tests.cpp','tools/vr_native_menu_tests.cpp']
baseline={}
for name in names:
 baseline[name]=hashlib.sha256((src/name).read_bytes()).hexdigest()
 for repo,label in [(root,'456'),(src,'123')]:
  target=work/'before'/label/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,target)
 (stage/name).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src/name,stage/name)
(work/'baseline123.json').write_text(json.dumps(baseline,indent=2))
sources={}
for p in (src/'src').rglob('*'):
 if p.is_file():
  rel=p.relative_to(src);sources[str(rel)]=hashlib.sha256(p.read_bytes()).hexdigest();shutil.copy2(p,stage/rel)
(work/'sources123.json').write_text(json.dumps(sources,indent=2))
for d in [root,stage]:
 p=d/'src/Config.h';s=p.read_text();s=s.replace('    bool valveIndexControllers = false;', '    // Dark preserves the original VR presentation; Brighter applies display encoding.\n    bool vrBrighterColors = false;\n    bool valveIndexControllers = false;');p.write_text(s)
 p=d/'src/Config.cpp';s=p.read_text();needle='    g_cfg.valveIndexControllers = GetBool';assert needle in s;s=s.replace(needle,'    g_cfg.vrBrighterColors = GetBool(L"VRBrighterColors",false,ini);\n'+needle);p.write_text(s)
 p=d/'src/VRSystem.cpp';s=p.read_text();needle='    t.eColorSpace = vr::ColorSpace_Gamma;';assert s.count(needle)==1
 s=s.replace(needle,'''    // Preserve the existing darker presentation by default. The optional
    // brighter scheme asks the compositor to apply linear-to-display encoding
    // to our RGBA8 eye image. This lifts midtones without multiplying/clipping
    // highlights or changing game lighting, and takes effect on the next submit.
    t.eColorSpace = Cfg().vrBrighterColors ? vr::ColorSpace_Linear : vr::ColorSpace_Gamma;''');p.write_text(s)
 p=d/'TombRaiderVR.ini';s=p.read_text();needle='FlipSubmitV=0';assert needle in s;s=s.replace(needle,needle+'\n\n; VR Options > Color Scheme: 0=Dark (original VR look), 1=Brighter.\n; Takes effect immediately when changed in VR Options; only affects the headset.\nVRBrighterColors=0');p.write_text(s)
print('Backed up source and prepared dark-default color configuration in both builds.')
