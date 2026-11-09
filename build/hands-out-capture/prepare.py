import json,hashlib,shutil
from pathlib import Path
root=Path.cwd();p=root/'build/hands-out-capture'
for f in ['ControllerBodyAnchor.h','PlantedSwivel.h']:shutil.copy2(p/'src'/f,root/'src'/f)
s=(p/'new-recorded.cpp').read_text();data=s.split('const Sample samples[]={\n')[1].split('\n};')[0]
out='''// Exact hands-out capture: arms lowered, raised, then repeated planted side glances.
// The old implementation moves the anchor more than three cumulative metres.
void TestRecordedHandsOutSwivels() {
    using namespace tr::firstperson;
    struct Sample { unsigned long long frame;double time;float body,yaw;Vec eye,left,right,forward; };
    static const Sample samples[]={
'''+data+'''
    };
    for(bool recordedBody:{false,true}) {
        ControllerBodyAnchor anchor;Vec planted{};
        for(const auto& s:samples) {
            anchor.Update(s.frame,s.time,s.eye,s.left,s.right,0,recordedBody?s.body:samples[0].body,true,false,s.yaw,.15f,s.forward);
            if(s.frame==samples[0].frame) planted=anchor.head;
            Check(Length(Flat(Sub(anchor.head,planted)))<.00001f,
                "recorded hands-out torso swivel retains the planted position");
            Check(!anchor.translating && !anchor.rotating && !anchor.heading.active,
                "recorded hands-out torso swivel grants neither travel nor body yaw");
        }
        const auto& last=samples[sizeof(samples)/sizeof(samples[0])-1];
        for(int direction=0;direction<4;++direction) {
            auto walking=anchor;Vec shift{};int firstStep=0;
            for(int i=1;i<=180;++i) {
                shift=YawVector({0,0,i*.0015f},direction*1.5707963f);
                walking.Update(last.frame+i,last.time+i/90.,Add(last.eye,shift),Add(last.left,shift),Add(last.right,shift),
                    0,recordedBody?last.body:samples[0].body,true,false,last.yaw,.15f,last.forward);
                if(!firstStep && walking.translating) firstStep=i;
            }
            Check(firstStep>0 && firstStep<=25,"straight walking releases a hands-out swivel hold within 25 poses");
            Check(Length(Flat(Sub(Sub(walking.head,planted),shift)))<.015f,
                "walking from the recorded swivel posture retains physical travel and the existing lean");
        }
    }
}
'''
(root/'tools/recorded_hands_out_regression.inl').write_text(out)
f=root/'tools/body_transition_regression.inl';s=f.read_text();s='#include "recorded_hands_out_regression.inl"\n'+s;s=s.replace('    TestRecordedPlantedGlances();','    TestRecordedPlantedGlances();\n    TestRecordedHandsOutSwivels();');f.write_text(s)
stage=p/'123-work';src123=Path('C:/dev/TombRaider123VR')
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (src123/name).exists():shutil.copytree(src123/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:
 if (src123/name).exists():shutil.copy2(src123/name,stage/name)
for f in ['ControllerBodyAnchor.h','PlantedSwivel.h']:shutil.copy2(root/'src'/f,stage/'src'/f)
shutil.copy2(root/'tools/recorded_hands_out_regression.inl',stage/'tests/recorded_hands_out_regression.inl')
f=stage/'tests/body_transition_selftest.h';s=f.read_text();s='#include "recorded_hands_out_regression.inl"\n'+s;s=s.replace('    TestRecordedPlantedGlances();','    TestRecordedPlantedGlances();\n    TestRecordedHandsOutSwivels();');f.write_text(s)
for f in ['test-arms-456.cmd','build-456.cmd','build-123.cmd']:
 (p/f).write_text((root/'build/glance-3d-analysis'/f).read_text().replace('glance-3d-analysis','hands-out-capture'))
