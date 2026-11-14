from pathlib import Path
import shutil
root=Path('C:/dev/TombRaider456VR');work=root/'build/landing-camera-height'
source=(root/'tools/upjump_arms_test.cpp').read_text()
setup=source[source.index('int main() {'):source.index('    for(int which:{0,1}) for(bool fullBody:{false,true}) for(bool recovery:')]
setup=setup.replace('    TestTorsoClearance();\n','').replace('&JumpJoint','&FakeGaitJoint')
test=r'''
    for(int which:{0,1}) for(bool full:{false,true}) for(bool stable:{false,true}) for(bool calibrated:{false,true})
    for(int clip:{24,31,82,92,99}) for(float heading:{0.f,.7f,-1.9f}) for(float duck:{0.f,-.2f}) {
        reset(which,full);config.firstPersonMovementStabilization=stable;
        config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=0;
        config.firstPersonPhysicalCrouch=false;
        auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
        auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
        auto& animation=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
        pos=prev={};pos.y_rot=prev.y_rot=Angle(heading);g_headingBase=heading;
        g_previousBody={};gaitSway={};animation=103;fraction=256;collisionMode=0;
        PHD_3DPOS camera{};Check(Anchor(camera),"standing camera available before fall");
        if(!calibrated) g_groundEye.Reset();const auto saved=g_groundEye;
        VR().m_rawHeadPose.m[1][3]+=duck;const auto head=VR().m_rawHeadPose;
        const float physical=VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre();
        state=clip==92?1:2;animation=int16_t(clip);
        for(int height:{-700,-600,-400,-200,-80,50,-80,-200,-400,-600,-700})
        for(int f:{0,64,128,192,256}) {
            prev.y_pos=400;pos.y_pos=512;fraction=f;gaitSway={30,height+700,-20};
            const auto physics=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            Check(Anchor(camera),"landing camera available through impact and recovery");
            const float root=400+112*(f/256.f);
            const bool held=stable && calibrated;
            Check(std::fabs(camera.y_pos-(root+(held?saved.local.y:height)))<1.1f,
                  "calibrated landing ignores neck compression while following interpolated collision height");
            if(held) {
                const auto flat=locomotion::Rotate({saved.local.x,saved.local.z},g_headingBase);
                Check(std::fabs(camera.x_pos-flat.x)<1.1f && std::fabs(camera.z_pos-flat.z)<1.1f,
                      "landing camera preserves the calibrated horizontal flight anchor");
                Check(std::fabs((camera.y_pos+physical)-(root+saved.local.y+duck*LiveWorldUnitsPerMetre()))<1.1f,
                      "physical head lowering remains live and is applied once");
            }
            Check(physics==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                  !std::memcmp(&head,&VR().m_rawHeadPose,sizeof(head)),"landing camera cannot alter physics or raw HMD tracking");
            Check(!std::memcmp(&saved,&g_groundEye,sizeof(saved)),"landing cannot recalibrate standing height");
        }
        state=2;animation=103;gaitSway={};pos.y_pos=prev.y_pos=512;fraction=256;
        Check(Anchor(camera) && camera.y_pos==512-700,"standing recovery has no extra camera rise after stabilized landing");
    }
    VR().m_system=nullptr;std::printf("PASS: %d landing camera checks\n",checks);
}
'''
prefix='#include <array>\n#define main UnusedFullFirstPersonSuite\n#include "first_person_tests.cpp"\n#undef main\nnamespace tr { void VRMenuShutdown() {} }\n'
(root/'tools/landing_camera_test.cpp').write_text(prefix+setup+test)
cmd=(work/'test-camera.cmd').read_text().replace('tools\\first_person_tests.cpp','tools\\landing_camera_test.cpp').replace(' build\\landing-camera-height\\camera-stubs.cpp','')
(work/'test-targeted-camera.cmd').write_text(cmd)
# Isolate the prior source to establish whether the older broad-suite failure
# predates this camera change; never swap the live checkout during testing.
baseline=work/'baseline'
for name in ['src','tools']:shutil.copytree(root/name,baseline/name,dirs_exist_ok=True)
shutil.copy2(work/'before456/FirstPerson.cpp',baseline/'src/FirstPerson.cpp')
shutil.copy2(work/'before456/first_person_tests.cpp',baseline/'tools/first_person_tests.cpp')
cmd=(work/'test-camera.cmd').read_text().replace('tools\\first_person_tests.cpp','build\\landing-camera-height\\baseline\\tools\\first_person_tests.cpp').replace('camera-tests.exe','baseline-camera-tests.exe')
(work/'test-baseline-camera.cmd').write_text(cmd)
