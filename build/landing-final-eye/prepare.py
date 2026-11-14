from pathlib import Path
import shutil
r=Path.cwd();w=r/'build/landing-final-eye';other=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (other/name).exists():shutil.copytree(other/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:shutil.copy2(other/name,stage/name)
for n in ['src/FirstPerson.cpp','src/FirstPersonStabilization.h','tools/landing_camera_test.cpp','tools/landing_floor_regression.inl','tools/first_person_tests.cpp','README.md']:
 p=w/'before'/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r/n,p)
for n in ['test-arms-456.cmd','test-targeted-camera.cmd','build-456.cmd','build-123.cmd']:(w/n).write_text((r/'build/landing-head-follow'/n).read_text().replace('landing-head-follow','landing-final-eye'))
p=r/'tools/landing_camera_test.cpp';s=p.read_text();s=s.replace('''                Check(physics==std::string''','''                const float renderedY=camera.y_pos+InvertRigid(VR().HeadView()).r[1][3];
                Check(!stable || renderedY<=root+(head.y-root-32)*scale+1.1f,
                      "rendered eye must not descend below the landing head eye after tracking is added");
                Check(physics==std::string''');p.write_text(s)
