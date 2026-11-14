from pathlib import Path
import hashlib,json,difflib
root=Path('C:/dev/TombRaider456VR');work=root/'build/landing-floor-fit';repo=Path('C:/dev/TombRaider123VR');stage=work/'123-work'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(name,obj):(work/name).write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8')
checks={'456':6905722,'123':3969766}
assert f'PASS: {checks["456"]} straight-up jump arm checks' in (work/'test-arms-456.log').read_text(encoding='utf-16')
assert f'PASS: {checks["123"]} locomotion, gun-control and camera hook checks' in (work/'test-123.log').read_text(encoding='utf-16')
names=['src/FirstPerson.cpp','README.md','tests/collection_integration_selftest.h','tests/landing_floor_selftest.h',
       'tests/landing_leg_checks.inl','tests/native_landing_poses.inl','tools/extract_landing_poses.py']
manifest=[];patch=[]
for name in names:
    old=repo/name;new=stage/name
    manifest.append(dict(Relative=name,OriginalHash=sha(old) if old.exists() else None,ModifiedHash=sha(new),Deleted=False))
    if old.exists():patch.extend(difflib.unified_diff(old.read_text(encoding='utf-8-sig').splitlines(True),new.read_text(encoding='utf-8-sig').splitlines(True),fromfile=name,tofile=name))
save('source123-manifest.json',manifest)
(work/'source123-review.diff').write_text(''.join(patch),encoding='utf-8')
releases=[]
for id,collection,previous in [
    ('456','Tomb Raider IV-VI Remastered','57756B9E2087412FB784F325ABA38F3F18194BA01142E35458AEF79DB18FFFB1'),
    ('123','Tomb Raider I-III Remastered','6FCD4E51EB3DC0C970D2F382146B40D4546A96CA027166014F54398822460EF1')]:
    directory=Path('C:/Program Files (x86)/Steam/steamapps/common')/collection
    assert sha(directory/'TombRaiderVR.dll')==previous,'Installed build changed during investigation'
    releases.append(dict(Id=id,Process='tomb'+id,Directory=str(directory),Previous=previous,
                         Expected=sha(work/id/'bin/TombRaiderVR.dll'),RenderChecks=checks[id]))
save('releases.json',releases)
save('validation.json',{
 'change':'Preserve authored impact-kneel leg poses at calibrated size instead of running standing leg IK while the landing camera drops',
 'reproduction':'Installed TR5 log contains hard-landing animation 24. Original production renderer on extracted native landing frames folds a knee 183.8 game units below ankle support with automatic scale and 12 cm HMD lowering.',
 'fix':'Native landing skeleton scaled about interpolated collision root; controller arm IK retained; standing torso fitting skipped until landing ends; native camera and simulation untouched',
 'nativeFixtures':{'poses':334,'games':[1,2,3,4,5],'levels':['LEVEL1','ASSAULT','JUNGLE','ANGKOR1','ANDREA1'],
   'clips':[103,24,31,82,92,99],'exclusions':'TR1 clip 99 water exit and TR5 clip 99 death excluded by native state',
   'generator':'tools/extract_landing_poses.py','regeneratedSHA256':sha(work/'regenerated-poses.inl')},
 'tr456Checks':checks['456'],'tr123Checks':checks['123'],
 'coverage':['Native hard/soft landing keys','Automatic and 0.7/1.0/1.2 manual body scale','Unarmed and pistols',
    'Standing and 12 cm physical head lowering','Left/right controller loss and hands-busy landing',
    'Exact calibrated pelvis/leg frames, which preserve native mesh floor geometry',
    'Exact controller wrist targets','Native physics palette preserved','Camera and item memory unchanged','Return to standing IK'],
 'build':'Both x64 Release builds passed with automatic game deployment disabled',
 'productionHashes':{name:sha(root/name) for name in ['src/FirstPerson.cpp','src/FirstPersonBodyIK.h','src/BodyFollower.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h']},
 'testHashes':{name:sha(root/name) for name in ['tools/upjump_arms_test.cpp','tools/landing_floor_regression.inl','tools/landing_leg_checks.inl','tools/native_landing_poses.inl','tools/extract_landing_poses.py']},
 'inHeadsetTested':False
})
s=(root/'build/stance-pivot-review/deploy.ps1').read_text(encoding='utf-8-sig').replace('stance-pivot-review','landing-floor-fit').replace('6637210','6905722').replace('3737982','3969766')
(work/'deploy.ps1').write_text(s,encoding='utf-8-sig')
print('Prepared verified release manifests and guarded install; no installation performed.')
