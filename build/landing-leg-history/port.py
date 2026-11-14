from pathlib import Path
import shutil,difflib,json,hashlib
r=Path.cwd();w=r/'build/landing-leg-history';repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
for n in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (repo/n).exists():shutil.copytree(repo/n,stage/n,dirs_exist_ok=True)
for n in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:shutil.copy2(repo/n,stage/n)
manifest={}
for n in ['FirstPersonBodyIK.h','BodyFollower.h','FirstPerson.cpp']:
 p=stage/'src'/n;manifest['src/'+n]=hashlib.sha256(p.read_bytes()).hexdigest().upper()
 before=(w/'before/src'/n).read_text().splitlines(True);after=(r/'src'/n).read_text().splitlines(True)
 s=p.read_text()
 for group in difflib.SequenceMatcher(None,before,after,autojunk=False).get_grouped_opcodes(2):
  a,b=group[0],group[-1];old=''.join(before[a[1]:b[2]]);new=''.join(after[a[3]:b[4]])
  if n=='FirstPerson.cpp':old=old.replace('Wrap(', 'locomotion::Wrap(');new=new.replace('Wrap(', 'locomotion::Wrap(')
  assert s.count(old)==1,(n,old)
  s=s.replace(old,new)
 p.write_text(s)
(w/'port-original-hashes.json').write_text(json.dumps(manifest,indent=2))
for n in ['test-arms-456.cmd','test-targeted-camera.cmd','build-456.cmd','build-123.cmd']:(w/n).write_text((r/'build/restore-landing-20261105'/n).read_text().replace('restore-landing-20261105','landing-leg-history'))
