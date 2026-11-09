from pathlib import Path
import shutil,json,hashlib,difflib
root=Path.cwd();p=root/'build/tracking-orientation-capture';stage=p/'123-work';repo123=Path('C:/dev/TombRaider123VR')
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (repo123/name).exists():shutil.copytree(repo123/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:
 if (repo123/name).exists():shutil.copy2(repo123/name,stage/name)
new=(root/'src/ControllerBodyAnchor.inl').read_text()
addition=new[new.index('    if(trace && tracked) {'):new.index('    if(Cfg().firstPersonDriftLog) {',new.index('    if(trace && tracked) {'))]
old=(p/'before456.inl').read_text();assert new.replace(addition,'')==old
target=stage/'src/ControllerBodyAnchor.inl';s=target.read_text();needle='    if(Cfg().firstPersonDriftLog) {';assert s.count(needle)==1
target.write_text(s.replace(needle,addition+needle))
assert (root/'src/ControllerBodyAnchor.h').read_bytes()==(stage/'src/ControllerBodyAnchor.h').read_bytes()
for name in ['build-456.cmd','build-123.cmd']:
 (p/name).write_text((root/'build/glance-3d-analysis'/name).read_text().replace('glance-3d-analysis','tracking-orientation-capture'))
(p/'changes.patch').write_text(''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='before/ControllerBodyAnchor.inl',tofile='after/ControllerBodyAnchor.inl')))
print('Only supplementary logging added. Both movement headers still match the restored version.')
