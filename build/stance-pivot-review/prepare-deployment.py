from pathlib import Path
import hashlib,json,shutil,difflib

root=Path('C:/dev/TombRaider456VR');work=root/'build/stance-pivot-review'
repo=Path('C:/dev/TombRaider123VR');stage=work/'123-work'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def save(name,data):(work/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')

files=['src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','tests/body_transition_selftest.h',
       'tests/close_hands_motion_regression.inl','tests/recorded_hands_out_regression.inl','README.md']
manifest=[];patch=[]
for name in files:
    old=repo/name;new=stage/name
    manifest.append(dict(Relative=name,OriginalHash=sha(old) if old.exists() else None,ModifiedHash=sha(new),Deleted=False))
    if old.exists():patch.extend(difflib.unified_diff(old.read_text(encoding='utf-8-sig').splitlines(True),new.read_text(encoding='utf-8-sig').splitlines(True),fromfile=name,tofile=name))
save('source123-manifest.json',manifest)
(work/'source123-review.diff').write_text(''.join(patch),encoding='utf-8')
for name in ['ControllerBodyAnchor.h','PlantedPivotEvidence.h']:
    assert sha(root/'src'/name)==sha(stage/'src'/name)
previous=json.loads((root/'build/tracking-orientation-capture/releases.json').read_text(encoding='utf-8-sig'))
for release in previous:
    release['Previous']=release['Expected']
    release['Expected']=sha(work/release['Id']/'bin/TombRaiderVR.dll')
    release['RenderChecks']=6637210 if release['Id']=='456' else 3737982
save('releases.json',previous)
validation={
 'change':'Recognize raised-hand downward inspection arcs behind the eye without revoking established steps or turns',
 'build':'Both x64 Release builds passed; DeployToGame=false',
 'tr456Checks':6637210,'tr123Checks':3737982,
 'additionalChecks':{'pitchedTracking':38424,'extendedArmStance':576024,'curvedWalking':'passed','pitchedPhysicalTurns':'passed'},
 'recordedCloseHands':{'poses':2122,'baselineExactReplayMismatches':0,'closedLoopFollower':True,
     'plantedFirst10Seconds':{'baselineAnchorSpanMetres':[.489,.108],'candidateAnchorSpanMetres':[0,0],
          'finalPaletteHipAndChestHorizontalTravelGameUnits':0,'renderCases':'TR4 and TR5, unarmed and dual pistols'},
     'forwardBackwardSpanMetres':.474,'sidestepSpanMetres':.349,'establishedTurnSamples':539},
 'recordedLoweredHands':{'poses':1851,'baselineExactReplayMismatches':0,'plantedAnchorTravelMetres':0,
     'forwardBackwardSpanMetres':.510,'sidestepSpanMetres':.286,'turnSamplesBaseline':299,'turnSamplesCandidate':280,
     'limitation':'About 0.32 seconds later initial turn admission in this recording; established turning continues'},
 'earlierRaisedHands':{'plantedPoses':647,'anchorTravelMetres':0,'walkingAfterHoldFirstPermitFrames90Hz':[17,24,24,17],
     'maximumFinalWalkingErrorMetres':.0105},
 'previousWalkingFreeze':{'isolated55to60SecondsSpanMetres':[.290,.209],'isolated75to85SecondsSpanMetres':[.322,.697],
     'comparison':'Isolated actual walking inputs match the restored walking algorithm',
     'limitation':'Full legacy capture contains analog movement/reset history that cannot be reconstructed from this log. Full-history 75-80s Z span differs (.250 candidate vs .467 restored); no candidate hold active there. Do not treat isolated agreement as full-session equivalence.'},
 'syntheticWalkingDuringRecordedGlances':{'baselineFinalDistancesMetres':[2.461,2.455,2.495,2.706],
     'candidateFinalDistancesMetres':[2.461,2.455,2.495,2.702],'translationAddedMetres':2.698,
     'limitation':'Constructed mixed motion, not an independent real walking capture'},
 'productionHashes':{name:sha(root/name) for name in ['src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','src/BodyFollower.h','src/ControllerBodyAnchor.inl','src/FirstPerson.cpp']},
 'testHashes':{name:sha(root/name) for name in ['tools/body_transition_regression.inl','tools/close_hands_motion_regression.inl','tools/recorded_hands_out_regression.inl']},
 'inHeadsetTested':False
}
save('validation.json',validation)
deploy=(root/'build/tracking-orientation-capture/deploy.ps1').read_text(encoding='utf-8-sig').replace('tracking-orientation-capture','stance-pivot-review')
(work/'deploy.ps1').write_text(deploy,encoding='utf-8-sig')
print('Prepared source review, hashes, release manifest, validation and guarded deployment. No installation performed.')
