from pathlib import Path
r=Path.cwd();w=r/'build/landing-head-follow';stage=w/'123-work'
p=r/'tools/landing_camera_test.cpp';s=p.read_text();s=s.replace('namespace tr { void VRMenuShutdown() {} }','''namespace tr { void VRMenuShutdown() {} }
#include "native_landing_poses.inl"
const NativeLandingPose* landingFixture=nullptr;
void __cdecl LandingJoint(uint8_t* item,tr::PHD_VECTOR* v,int joint,int frac) {
    using namespace tr;
    const auto& p=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    const auto& old=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos_prev);
    const float t=frac/256.f,yaw=Radians(old.y_rot)+Wrap(Radians(p.y_rot)-Radians(old.y_rot))*t;
    const auto point=motiongun::Transform(landingFixture->joints[joint],{float(v->x),float(v->y),float(v->z)});
    const auto flat=locomotion::Rotate({point.x,point.z},yaw);
    *v={int32_t(std::lround(old.x_pos+(p.x_pos-old.x_pos)*t+flat.x)),
        int32_t(std::lround(old.y_pos+(p.y_pos-old.y_pos)*t+point.y)),
        int32_t(std::lround(old.z_pos+(p.z_pos-old.z_pos)*t+flat.z))};
}''')
a=s.index('    for(int which:{0,1})');b=s.index('    VR().m_system=nullptr',a)
s=s[:a]+'''    for(const auto& fixture:nativeLandingPoses) {
        if(fixture.game<4 || fixture.clip==103) continue;
        for(bool full:{false,true}) for(bool stable:{false,true}) for(bool calibrated:{false,true})
        for(float size:{.7f,1.f,1.2f}) for(float heading:{0.f,.7f,-1.9f}) for(float duck:{0.f,-.2f}) {
            reset(fixture.game-4,full);config.firstPersonMovementStabilization=stable;
            config.firstPersonBodyScale=size;config.firstPersonAnchorX=0;config.firstPersonAnchorY=-32;config.firstPersonAnchorZ=140;
            config.firstPersonPhysicalCrouch=false;
            auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
            auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
            pos=prev={};pos.y_rot=prev.y_rot=Angle(heading);g_headingBase=heading;g_previousBody={};
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=int16_t(fixture.clip);
            state=fixture.clip==92?1:2;
            if(calibrated) g_groundEye.Apply({0,0,0},heading,{0,-700,140},heading);
            else g_groundEye.Reset();const auto saved=g_groundEye;
            landingFixture=&fixture;dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&LandingJoint));
            VR().m_rawHeadPose.m[1][3]+=duck;const auto rawHead=VR().m_rawHeadPose;
            for(int f:{0,64,128,192,256}) {
                prev.y_pos=400;pos.y_pos=512;fraction=f;
                const auto physics=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
                PHD_3DPOS camera{};Check(Anchor(camera),"native landing camera is available");
                PHD_VECTOR head{},hip{},raw{0,-32,140};LandingJoint(itemMemory,&head,14,f);LandingJoint(itemMemory,&hip,0,f);LandingJoint(itemMemory,&raw,14,f);
                const float scale=full?size:1,root=400+112*(f/256.f);
                const float yaw=Radians(pos.y_rot);const auto offset=locomotion::Rotate({0,140*scale},yaw);
                const float x=stable?hip.x+(head.x-hip.x)*scale+offset.x:raw.x;
                const float y=stable?root+(head.y-root-32)*scale:raw.y;
                const float z=stable?hip.z+(head.z-hip.z)*scale+offset.z:raw.z;
                Check(std::fabs(camera.x_pos-x)<1.1f && std::fabs(camera.y_pos-y)<1.1f && std::fabs(camera.z_pos-z)<1.1f,
                      "landing eye follows the scaled animated head with a yaw-only eye offset");
                Check(!stable || std::fabs(camera.y_pos-(root+(head.y-root)*scale)+32*scale)<1.1f,
                      "impact head pitch cannot rotate the forward eye offset into the legs");
                Check(physics==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                    !std::memcmp(&rawHead,&VR().m_rawHeadPose,sizeof(rawHead)),"head following leaves collision and physical tracking untouched");
                Check(saved.valid==g_groundEye.valid && saved.local.y==g_groundEye.local.y,"landing cannot recalibrate standing height");
            }
        }
    }
''' +s[b:];p.write_text(s)
for p in [r/'tools/landing_floor_regression.inl',stage/'tests/landing_floor_selftest.h']:
 s=p.read_text();old='Transform(standing->joints[14],{0,-32,140})';s=s.replace(old,'''s.clip==103 ? Transform(standing->joints[14],{0,-32,140}) :
                motiongun::Vec{s.joints[0].origin.x+(s.joints[14].origin.x-s.joints[0].origin.x)*AnimatedBodyScale('''+('itemMemory' if p.parent.name=='tools' else 'item')+'''),
                    (s.joints[14].origin.y-32)*AnimatedBodyScale('''+('itemMemory' if p.parent.name=='tools' else 'item')+'''),
                    s.joints[0].origin.z+(s.joints[14].origin.z-s.joints[0].origin.z+140)*AnimatedBodyScale('''+('itemMemory' if p.parent.name=='tools' else 'item')+''')}''');p.write_text(s)
p=stage/'tests/firstperson_ik_selftest.h';s=p.read_text();s=s.replace('''        PHD_3DPOS eye{30,-480,150}; const auto native=eye;
''','''        testConfig.firstPersonAnchorX=testConfig.firstPersonAnchorY=testConfig.firstPersonAnchorZ=0;
        for(auto offset:{off::item_joints_prev,off::item_joints_cur}) {
            auto* j=reinterpret_cast<int32_t*>(item+offset+14*off::joint_stride);
            j[3]=30*16384;j[7]=-480*16384;j[11]=150*16384;
        }
        PHD_3DPOS eye{30,-480,150}; const auto native=eye;
''');s=s.replace('eye.x_pos==0 && eye.y_pos==-700 && eye.z_pos==70','eye.x_pos==30 && eye.y_pos==-480 && eye.z_pos==150').replace('calibrated landing eye remains above the kneeling legs at the flight anchor','landing eye follows the head instead of staying at standing height');p.write_text(s)
p=stage/'tests/remaining_parity_selftest.h';s=p.read_text();s=s.replace('''            PHD_3DPOS eye{30,landingY-440,20};UpdateLocomotion(eye);
            Check(eye.y_pos==landingY-700,"calibrated landing camera rejects ordinary and hard-fall neck dip");''','''            testConfig.firstPersonAnchorY=0;
            for(auto offset:{off::item_joints_prev,off::item_joints_cur})
                reinterpret_cast<int32_t*>(item+offset+14*off::joint_stride)[7]=-440*16384;
            PHD_3DPOS eye{30,landingY-440,20};UpdateLocomotion(eye);
            Check(eye.y_pos==landingY-(ordinary?700:440),"ordinary recovery stays stable and hard falls follow the head");''');p.write_text(s)
p=stage/'README.md';s=p.read_text(encoding='utf-8');s=s.replace("active. The calibrated camera follows Lara's collision position through impact\nwithout the animated neck dip, keeping the view above her legs. Physical head\nmovement stays live; collision, damage and recovery timing remain native.","active. The landing camera follows the animated head at the visible body's\nscale, with the eye offset in Lara's facing frame. Physical head movement stays\nlive; collision, damage and recovery timing remain native.");p.write_text(s,encoding='utf-8')
