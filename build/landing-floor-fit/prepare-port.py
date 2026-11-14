from pathlib import Path
import shutil,json,hashlib
root=Path('C:/dev/TombRaider456VR');work=root/'build/landing-floor-fit';repo=Path('C:/dev/TombRaider123VR');stage=work/'123-work'
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
    if (repo/name).exists():shutil.copytree(repo/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:shutil.copy2(repo/name,stage/name)
f=stage/'src/FirstPerson.cpp';s=f.read_text()
def replace(a,b):
    global s
    assert s.count(a)==1,(a,s.count(a));s=s.replace(a,b)
s=s.replace('AirborneBodyScale','AnimatedBodyScale').replace('FitAirborneVisualBody','FitAnimatedVisualBody')
replace('Keep the calibrated visual size through native jump animation.','Keep the calibrated visual size through native jump/landing animation.')
replace('if(!Cfg().firstPersonFullBodyIK || !CanFitAirborneBody(item)) return 1.f;','if(!Cfg().firstPersonFullBodyIK || (!CanFitAirborneBody(item) && !UseLandingCamera(item))) return 1.f;')
replace('if(!CanFitAirborneBody(item) || DrawingNativeShadow()) return false;','const bool landing=UseLandingCamera(item);\n    if((!CanFitAirborneBody(item) && !landing) || DrawingNativeShadow()) return false;')
replace('const bool measured=g_torsoSurface.valid && g_torsoSurface.owner==obj;','const bool measured=!landing && g_torsoSurface.valid && g_torsoSurface.owner==obj;')
current=(root/'src/FirstPerson.cpp').read_text();begin=current.index('    if(landing) {\n        // Impact kneels');end=current.index('    if(g_bodyLanding && !landing)',begin)
replace('    const bool landing=UseLandingCamera(item);\n    if(g_bodyLanding && !landing)', '    const bool landing=UseLandingCamera(item);\n'+current[begin:end]+'    if(g_bodyLanding && !landing)')
replace('if(!shadow && Cfg().firstPersonMovementStabilization && g_groundEye.valid && CanFitStandingBody(item)) {','if(!shadow && !UseLandingCamera(item) && Cfg().firstPersonMovementStabilization && g_groundEye.valid && CanFitStandingBody(item)) {')
f.write_text(s)
for name in ['test-arms-456.cmd','build-456.cmd','build-123.cmd']:
    (work/name).write_text((root/'build/stance-pivot-review'/name).read_text().replace('stance-pivot-review','landing-floor-fit'))
print('Staged TR1-3 source port and builds; live checkout unchanged.')
