from pathlib import Path
import hashlib,json,shutil
root=Path('C:/dev/TombRaider456VR');repo=Path('C:/dev/TombRaider123VR');stage=root/'build/vr-options123';work=root/'build/options123-nested'
work.mkdir(exist_ok=True)
names=['README.md','src/VRNativeMenu.cpp','src/VRNativeMenuExtra.inl','src/VRNativeMenuModel.h','tools/vr_native_menu_tests.cpp']
baseline={}
for name in names:
 baseline[name]=hashlib.sha256((repo/name).read_bytes()).hexdigest()
 for source,label in [(root,'456'),(repo,'123')]:
  target=work/'before'/label/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,target)
 shutil.copy2(repo/name,stage/name)
(work/'baseline123.json').write_text(json.dumps(baseline,indent=2))
sources={}
for p in (repo/'src').rglob('*'):
 if p.is_file():
  rel=p.relative_to(repo);sources[str(rel)]=hashlib.sha256(p.read_bytes()).hexdigest();shutil.copy2(p,stage/rel)
(work/'sources123.json').write_text(json.dumps(sources,indent=2))
print('Saved TR1-3 source baseline and refreshed isolated staging tree.')
