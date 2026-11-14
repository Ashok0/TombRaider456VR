#include "landing_leg_checks.inl"
void TestLandingFloorPalette(uint8_t* objectInfo) {
    using namespace tr;using namespace tr::motiongun;
    const auto* standing=&nativeLandingPoses[0];
    for(const auto& s:nativeLandingPoses) if(s.game==dll.game+4 && s.clip==103) standing=&s;
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&FakeBodyJoint)-g_boundBase);
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    std::memset(objectInfo,0,off::object_stride);
    Frame inverseBind[15]{};
    for(int j=0;j<15;++j) {
        Inverse(standing->joints[j],inverseBind[j]);
        auto* b=reinterpret_cast<float*>(objectInfo+1676)+j*16;
        for(int r=0;r<3;++r) for(int c=0;c<3;++c) b[c*4+r]=inverseBind[j].basis.r[r][c];
        b[12]=inverseBind[j].origin.x;b[13]=inverseBind[j].origin.y;b[14]=inverseBind[j].origin.z;b[15]=1;
    }
    float fixture[180]{},palette[180]{};skinFixture=fixture;skinFixtureCount=15;
    worldCameraValid=true;worldCameraPos[0]=worldCameraPos[1]=worldCameraPos[2]=0;
    g_bodyVisualOffset=g_bodyWallOffset={};g_groundEye.valid=true;g_groundEye.bodyLocal={};
    config.firstPersonJoint=14;config.firstPersonAnchorX=0;config.firstPersonAnchorY=-32;config.firstPersonAnchorZ=140;
    config.firstPersonBodyForwardCm=0;g_bodySkinScope=true;g_haveManualInput=false;
    g_holsters.active=true;g_holsters.weapon=1;
    *reinterpret_cast<int16_t*>(laraMemory+4)=1;*reinterpret_cast<int16_t*>(laraMemory+2)=0;
    trackingtest::Clock clock;
    for(float factor:{0.f,.7f,1.f,1.2f}) for(float drop:{0.f,.12f}) for(int status:{0,4}) {
        *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);
        config.firstPersonBodyScale=factor;ResetBodyFollower();g_autoBodyCalibration={};
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1];
        for(size_t n=0;n<=std::size(nativeLandingPoses);++n) {
            const auto& s=n==std::size(nativeLandingPoses)?*standing:nativeLandingPoses[n];
            if(s.game!=dll.game+4) continue;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=s.clip==92?1:2;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=int16_t(s.clip);
            VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-(s.clip==103?0:drop);
            const Vec eye=Transform(s.joints[14],{0,-32,140});
            g_scenePose={int32_t(eye.x),int32_t(eye.y),int32_t(eye.z)};
            for(int j=0;j<15;++j) {bodyJointWorld[j]=s.joints[j].origin;WriteRows(Multiply(s.joints[j],inverseBind[j]),fixture+j*12);}
            std::memcpy(palette,fixture,sizeof(palette));clock.Advance(1./30);++VR().m_poseSerial;
            const auto camera=g_scenePose;
            const auto physicsBefore=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            const float size=AnimatedBodyScale(itemMemory);
            Detour_GetJoints(itemMemory,palette,0);
            Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)) &&
                physicsBefore==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),
                "landing fitting leaves the camera, collision, animation and damage state untouched");
            uint64_t mask=0;int count=0;const float* physics=FirstPersonBodyPalette(mask,count);
            Check(physics && count==15 && !std::memcmp(physics,fixture,sizeof(fixture)),
                "landing physics and dynamic bones retain the unmodified native palette");
            Frame out[15]{};
            for(int j=0;j<15;++j) Check(ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,out[j]),"landing joint readable");
            if(s.clip!=103) {
                CheckLandingLegPalette(s.joints,out,size);
                for(int hand=0;hand<2;++hand) {
                    Frame wrist{};Basis basis{};
                    Check(BuildControllerWrist(hand,wrist,basis) && firstperson::Length(Sub(out[hand?10:13].origin,wrist.origin))<.1f,
                        "native landing legs retain exact live controller wrists");
                }
                if(s.clip==24 && s.frame==0) for(int missing:{0,1,2}) {
                    VR().m_controllerPoseValid[0]=missing!=0;VR().m_controllerPoseValid[1]=missing!=1;
                    *reinterpret_cast<int16_t*>(laraMemory+2)=missing==2?1:int16_t(status);
                    ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                    for(int j=0;j<15;++j) ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,out[j]);
                    CheckLandingLegPalette(s.joints,out,size);
                    VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
                    *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);
                }
            } else Check(g_bodyFollower.valid && !g_bodyLanding,"standing rendering owns the body outside native landings");

        }
    }
    skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
}
