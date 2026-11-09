from pathlib import Path
import json,shutil
p=Path(__file__).parent;root=Path.cwd()
def f(x):
 s=format(x,'.9g');return s+('.0' if '.' not in s and 'e' not in s else '')+'f'
def vec(v):return '{'+','.join(map(f,v))+'}'
rows=json.loads(Path('build/close-hands-analysis/poses.json').read_text())
rows=[r for r in rows if r['t']-rows[0]['t']<30]
code='''// User-labelled capture: planted close-hand glances, forward/back steps,
// side steps, then physical turns. Positions and raw bases are recorded samples.
struct CloseHandsPose {
    unsigned long long frame;double time;float base,body,yaw,neutralY;
    tr::motiongun::Vec eye,left,right,forward;float basis[3][9];
};
const auto& CloseHandsCapture() {
    static const CloseHandsPose samples[]={
'''
for r in rows:
 code+='{'+str(r['frame'])+','+repr(r['t'])+','+','.join(map(f,[r['base'],r['body'],r['yaw'],r['height'][0]]))+','+','.join(vec(r[k]) for k in ['h','l','r','f'])+',{'+','.join(vec(r['basis'][k]) for k in ['head','left','right'])+'}},\n'
code+='''    };return samples;
}
void TestCloseHandsRecordedMotion() {
    using namespace tr::firstperson;
    const auto& samples=CloseHandsCapture();ControllerBodyAnchor anchor;BodyFollower follower;
    tr::motiongun::Vec planted{};float minZ=INFINITY,maxZ=-INFINITY,minX=INFINITY,maxX=-INFINITY;int turns=0;
    for(const auto& s:samples) {
        const float body=follower.valid ? BodyAngle(follower.yaw-s.base) : s.body;
        anchor.Update(s.frame,s.time,s.eye,s.left,s.right,s.base,body,true,false,s.yaw,.15f,s.forward);
        follower.Update(s.frame,s.time,s.base,s.base+anchor.yaw,s.base+s.yaw,{},{},false,BodyFollowSettings(),
            {},false,true,0,anchor.held,true,anchor.heading.active?s.base+anchor.heading.target:NAN,anchor.rotating);
        if(s.frame==samples[0].frame) planted=anchor.head;
        const double elapsed=s.time-samples[0].time;
        if(elapsed<10) {
            Check(Length(Flat(Sub(anchor.head,planted)))<.00001f,"recorded close-hand glances keep the planted anchor fixed");
            Check(!anchor.translating && !anchor.rotating,"recorded close-hand glances grant neither steps nor body turns");
        }
        if(elapsed>=12 && elapsed<16) {minZ=std::min(minZ,anchor.head.z);maxZ=std::max(maxZ,anchor.head.z);}
        if(elapsed>=16.5 && elapsed<19.5) {minX=std::min(minX,anchor.head.x);maxX=std::max(maxX,anchor.head.x);}
        if(elapsed>=20.5 && elapsed<29.5 && anchor.rotating) ++turns;
    }
    Check(maxZ-minZ>.46f,"recorded forward/back steps retain their 47 cm travel after close-hand glances");
    Check(maxX-minX>.33f,"recorded side steps retain their 35 cm travel after close-hand glances");
    Check(turns>=530,"recorded established physical turns remain continuously responsive");
}
'''
(root/'tools/close_hands_motion_regression.inl').write_text(code)
# Retain the earlier independently recorded arms-out regression and walking exit.
shutil.copy2(root/'build/swivel-rollback/failed-source/tools/recorded_hands_out_regression.inl',root/'tools/recorded_hands_out_regression.inl')
file=root/'tools/body_transition_regression.inl';s=file.read_text();s='#include "close_hands_motion_regression.inl"\n#include "recorded_hands_out_regression.inl"\n'+s
s=s.replace('    TestRecordedPlantedGlances();','    TestRecordedPlantedGlances();\n    TestRecordedHandsOutSwivels();\n    TestCloseHandsRecordedMotion();')
file.write_text(s)
# Capture source into a build workspace before touching the TR1-3 checkout.
stage=p/'123-work';repo123=Path('C:/dev/TombRaider123VR')
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (repo123/name).exists():shutil.copytree(repo123/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:
 if (repo123/name).exists():shutil.copy2(repo123/name,stage/name)
for name in ['ControllerBodyAnchor.h','PlantedPivotEvidence.h']:shutil.copy2(root/'src'/name,stage/'src'/name)
for name in ['close_hands_motion_regression.inl','recorded_hands_out_regression.inl']:shutil.copy2(root/'tools'/name,stage/'tests'/name)
file=stage/'tests/body_transition_selftest.h';s=file.read_text();s='#include "close_hands_motion_regression.inl"\n#include "recorded_hands_out_regression.inl"\n'+s
s=s.replace('    TestRecordedPlantedGlances();','    TestRecordedPlantedGlances();\n    TestRecordedHandsOutSwivels();\n    TestCloseHandsRecordedMotion();');file.write_text(s)
for name in ['test-arms-456.cmd','build-456.cmd','build-123.cmd']:
 (p/name).write_text((root/'build/glance-3d-analysis'/name).read_text().replace('glance-3d-analysis','stance-pivot-review'))
