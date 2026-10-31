from pathlib import Path
import hashlib,json,shutil
root=Path('C:/dev/TombRaider456VR');source=Path('C:/dev/TombRaider123VR');stage=root/'build/vr-options123';work=root/'build/controller-type-only'
work.mkdir(exist_ok=True)
names=['README.md','TombRaiderVR.vcxproj','src/VRMenu.cpp','src/VRMenu.h','src/VRMenuModel.h','src/VRMenuSettings.h','src/VRMenuSettings.inl','src/VRNativeMenu.cpp','src/VRNativeMenuModel.h','src/Gamepad.cpp','tools/vr_menu_tests.cpp','tools/vr_native_menu_tests.cpp','build/test_vr_options.cmd']
baseline={}
for name in names:
 p=source/name;baseline[name]=hashlib.sha256(p.read_bytes()).hexdigest()
 for repo,label in [(root,'456'),(source,'123')]:
  out=work/'before'/label/name;out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,out)
 (stage/name).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,stage/name)
(work/'baseline123.json').write_text(json.dumps(baseline,indent=2))
manifest={}
for p in (source/'src').rglob('*'):
 if p.is_file():
  rel=p.relative_to(source);manifest[str(rel)]=hashlib.sha256(p.read_bytes()).hexdigest();shutil.copy2(p,stage/rel)
(work/'sources123.json').write_text(json.dumps(manifest,indent=2))
