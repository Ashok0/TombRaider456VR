from pathlib import Path
import subprocess,shutil,json,hashlib
r=Path.cwd();w=r/'build/restore-landing-20261105';w.mkdir(exist_ok=True);repo=Path('C:/dev/TombRaider123VR');stage=w/'123-work'
def git(root,rev,name):return subprocess.check_output(['git','-C',str(root),'-c','safe.directory='+root.as_posix(),'show',rev+':'+name])
for n in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (repo/n).exists():shutil.copytree(repo/n,stage/n,dirs_exist_ok=True)
for n in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:shutil.copy2(repo/n,stage/n)
rootNames=['src/FirstPerson.cpp','src/FirstPersonStabilization.h','tools/first_person_tests.cpp','tools/upjump_arms_test.cpp','README.md']
for n in rootNames+['tools/landing_camera_test.cpp']:
 p=w/'before456'/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r/n,p)
# Reviewed differences in these files are exclusively the rejected landing work.
for n in rootNames:(r/n).write_bytes(git(r,'HEAD',n))
otherNames=['src/FirstPerson.cpp','src/FirstPersonStabilization.h','tests/collection_integration_selftest.h','tests/firstperson_ik_selftest.h','tests/remaining_parity_selftest.h','README.md']
for n in otherNames:(stage/n).write_bytes(git(repo,'HEAD',n))
# The November 6 stance compensation postdates the requested baseline. Keep it
# for standing, but restore the original physical posture input during impact.
for base in [r,stage]:
 p=base/'src/FirstPerson.cpp';s=p.read_text();needle='''    if(!Cfg().firstPersonMovementStabilization || !g_groundTorso.valid ||
       !CanFitStandingBody(item) || !HasControllerBodyAnchor()) return drop;''';assert s.count(needle)==1
 s=s.replace(needle,'''    // Native fall landings retain the physical posture input used before the
    // planted-glance compensation; the camera and full-body solver dip together.
    if(UseLandingCamera(item) || !Cfg().firstPersonMovementStabilization || !g_groundTorso.valid ||
       !CanFitStandingBody(item) || !HasControllerBodyAnchor()) return drop;''');p.write_text(s)
# Verify the original camera function exactly matches the November 5 source.
a=git(r,'4607ef8','src/FirstPerson.cpp').decode();b=(r/'src/FirstPerson.cpp').read_text()
extract=lambda s:s[s.index('bool Anchor(PHD_3DPOS& pose) {'):s.index('bool DrawingLaraHead(')]
assert extract(a)==extract(b)
(w/'historical-comparison.json').write_text(json.dumps({'requestedDate':'approximately 2026-11-06','tr456Reference':'4607ef8 (2026-11-05 08:55)','tr123Reference':'9949b67 (2026-11-05 08:55)','tr456AnchorExactlyMatchesNovember5':True,'restored':'Native animated landing camera plus full-body IK, original physical posture input and shared shoulder/hand solve','preserved':'Current standing calibration, roomscale classifiers, menus, controls, jump/visibility and non-landing fixes'},indent=2))
for n in ['build-456.cmd','build-123.cmd','test-arms-456.cmd','test-targeted-camera.cmd']:
 (w/n).write_text((r/'build/landing-final-eye'/n).read_text().replace('landing-final-eye','restore-landing-20261105'))
# Focused regression: use native landing fixtures and the actual rendered HMD
# transform; the earlier dip/recovery must remain free to follow native motion.
p=r/'tools/landing_camera_test.cpp';s=p.read_text();a=s.index('                const float scale=');b=s.index('                Check(physics==',a)
s=s[:a]+'''                const float trackingY=tracking==0 ? duck*(config.flipViewY?-1.f:1.f)*LiveWorldUnitsPerMetre() : 0;
                Check(std::fabs(camera.x_pos-raw.x)<1.1f && std::fabs(camera.y_pos-raw.y)<1.1f && std::fabs(camera.z_pos-raw.z)<1.1f,
                      "restored landing camera follows native animated eye through dip and recovery");
                const float renderedY=camera.y_pos+InvertRigid(VR().HeadView()).r[1][3];
                Check(std::fabs(InvertRigid(VR().HeadView()).r[1][3]-trackingY)<.01f,"test uses actual rendered tracking translation");
                Check(std::fabs(renderedY-(raw.y+trackingY))<1.1f,
                      "restored landing retains live physical head movement without a standing hold or landing height cap");
''' +s[b:];p.write_text(s)
