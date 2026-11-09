from pathlib import Path
import json,hashlib
root=Path.cwd();p=root/'build/tracking-orientation-capture';stage=p/'123-work';repo123=Path('C:/dev/TombRaider123VR')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
name='src/ControllerBodyAnchor.inl'
manifest=[dict(Relative=name,OriginalHash=sha(repo123/name),ModifiedHash=sha(stage/name),Deleted=False)]
(p/'source123-manifest.json').write_text(json.dumps(manifest,indent=2))
releases=json.loads((root/'build/glance-3d-analysis/releases.json').read_text(encoding='utf-8-sig'))
for r in releases:
 r['Previous']=r['Expected'];r['Expected']=sha(p/r['Id']/'bin/TombRaiderVR.dll');r['RenderChecks']=0
(p/'releases.json').write_text(json.dumps(releases,indent=2))
files=['src/ControllerBodyAnchor.h','src/BodyFollower.h','src/ControllerBodyAnchor.inl','src/FirstPerson.cpp']
validation=dict(change='Supplementary diagnostic logging only; restored movement and rendering algorithms unchanged',
    build='Both x64 Release builds passed',behavioralTestsRerun=False,
    baselineCaptureReplay=dict(poses=2375,mismatches=0),
    newFields='Raw 3x3 orientation bases for head/left/right, calibrated standing head height, compensated posture drop; keyed by pose serial',
    productionHashes={f:sha(root/f) for f in files},inHeadsetTested=False)
(p/'validation.json').write_text(json.dumps(validation,indent=2))
deploy=(root/'build/hands-out-capture/deploy.ps1').read_text().replace('hands-out-capture','tracking-orientation-capture')
(p/'deploy.ps1').write_text(deploy)
notes=dict(installedAlgorithm='Unmodified restored release; no experiment installed',
    rejected=[dict(name='3D tracked triangle decomposition',reason='Still under-follows recorded forward/back walking; captured glances retain measurable drift'),
              dict(name='Yaw/torso-lever decomposition',reason='Reduces repeated glance path but still loses recorded walking travel; fixed lever and missing controller orientation are not sufficient')],
    next='Capture synchronized raw controller orientations and calibrated posture, then compare hand aim rotation to controller-pair rotation on separately labelled glance/step/turn sequences')
(root/'build/motion-decomposition/findings.json').write_text(json.dumps(notes,indent=2))
print(json.dumps(releases,indent=2))
