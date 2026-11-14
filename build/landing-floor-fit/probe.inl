#include "native-poses.inl"
void ProbeLanding(uint8_t* objectInfo) {
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
    for(float factor:{0.f,.7f,1.f,1.2f}) for(float drop:{0.f,.12f}) {
        config.firstPersonBodyScale=factor;ResetBodyFollower();g_autoBodyCalibration={};
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-drop;
        for(const auto& s:nativeLandingPoses) {
            if(s.game!=dll.game+4) continue;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=s.clip==92?1:2;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=int16_t(s.clip);
            const Vec eye=Transform(s.joints[14],{0,-32,140});
            g_scenePose={int32_t(eye.x),int32_t(eye.y),int32_t(eye.z)};
            for(int j=0;j<15;++j) {bodyJointWorld[j]=s.joints[j].origin;WriteRows(Multiply(s.joints[j],inverseBind[j]),fixture+j*12);}
            std::memcpy(palette,fixture,sizeof(palette));clock.Advance(1./30);++VR().m_poseSerial;
            const bool full=ApplyFullBodyIK(itemMemory,palette,15);
            Check(full || ApplyArmIK(itemMemory,palette,15),"native landing probe solves");
            float maxFoot=-INFINITY,maxKnee=-INFINITY;Frame out[15]{};
            for(int j=0;j<15;++j) ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,out[j]);
            for(int j:{3,6}) maxFoot=std::max(maxFoot,out[j].origin.y-s.joints[j].origin.y);
            for(int j:{2,5}) maxKnee=std::max(maxKnee,out[j].origin.y-std::max(s.joints[3].origin.y,s.joints[6].origin.y));
            std::printf("LAND,%d,%.1f,%.2f,%d,%d,%.4f,%.4f,%.4f,%.4f\n",s.game,factor,drop,s.clip,s.frame,maxFoot,maxKnee,out[0].origin.y,eye.y);
        }
    }
    VR().m_system=nullptr;
}
