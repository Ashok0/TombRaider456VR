import hashlib,json,shutil
from pathlib import Path
p=Path(__file__).resolve().parent;root=p.parent.parent;repo123=Path('C:/dev/TombRaider123VR');stage=p/'123-work'
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest().upper()
files=['src/ControllerBodyAnchor.h','src/PlantedSwivel.h','tests/body_transition_selftest.h','tests/recorded_hands_out_regression.inl','README.md']
manifest=[dict(Relative=f,OriginalHash=sha(repo123/f) if (repo123/f).exists() else None,ModifiedHash=sha(stage/f),Deleted=False) for f in files]
(p/'source123-manifest.json').write_text(json.dumps(manifest,indent=2))
releases=json.loads((root/'build/glance-3d-analysis/releases.json').read_text(encoding='utf-8-sig'))
for r in releases:
 r['Previous']=r['Expected'];r['Expected']=sha(p/r['Id']/'bin/TombRaiderVR.dll');r['RenderChecks']=6630012 if r['Id']=='456' else 3736781
(p/'releases.json').write_text(json.dumps(releases,indent=2))
production=['src/ControllerBodyAnchor.h','src/PlantedSwivel.h','src/ControllerBodyAnchor.inl','src/FirstPerson.cpp','src/BodyFollower.h']
validation=dict(capture=dict(poses=13554,exactReplayMismatches=0,originalDLL=releases[0]['Previous']),
    regression=dict(poses=647,frames=[1554,2200],beforeCumulativeAnchorTravelMetres=3.245,afterCumulativeAnchorTravelMetres=0,afterTurns=0,afterSteps=0,
        walkingAfterwardFirstFrameAt90Hz=24,walkingAfterwardMaxErrorMetres=.012,recordedAndFixedBodyYaw=True),
    tests456=6630012,tests123=3736781,releaseBuilds='Both x64 Release builds passed',
    syntheticMixedWalking=dict(description='0.25 m/s shared travel added to the captured swivel; not an actual walking recording',
        expectedMetres=2.698,beforeFinalDistanceMetres=[2.461,2.455,2.495,2.706],afterFinalDistanceMetres=[2.206,1.935,2.104,2.099],
        limitation='Simultaneous deep lean/swivel/walking can lag more than ordinary steps; conservative inference still under-follows this constructed stress test.'),
    initialCaptureLimit='Before the repeated-glance regression, the first second of transitioning into the inspection pose still has about 95 mm cumulative movement in replay; this change targets the sustained hands-out sliding.',
    unchanged=['INI settings','height calibration','arm solver','camera pose','accepted locomotion transport'],inHeadsetTested=False,
    productionHashes={f:sha(root/f) for f in production})
(p/'validation.json').write_text(json.dumps(validation,indent=2))
deploy=(root/'build/glance-3d-analysis/deploy.ps1').read_text().replace('glance-3d-analysis','hands-out-capture')
check='''$validation=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'validation.json') -Raw | ConvertFrom-Json
foreach($source in $validation.productionHashes.PSObject.Properties) {
    if((Get-FileHash -LiteralPath (Join-Path 'C:\\dev\\TombRaider456VR' $source.Name)).Hash -ne $source.Value) { throw 'TR4-6 tested source changed.' }
}
'''
deploy=deploy.replace("$repo123='C:\\dev\\TombRaider123VR'",check+"$repo123='C:\\dev\\TombRaider123VR'")
(p/'deploy.ps1').write_text(deploy)
# The same input logger remains in production; compile the current replay utility.
s=(root/'build/glance-3d-analysis/test-replay.cmd').read_text().replace('glance-3d-analysis','hands-out-capture')
(p/'test-replay.cmd').write_text(s)
shutil.copy2(root/'build/glance-3d-analysis/generate-replay.cpp',p/'generate-replay.cpp')
print(json.dumps(releases,indent=2))
