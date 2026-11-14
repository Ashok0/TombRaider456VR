#include "landing_leg_checks.inl"
void TestNativeLandingFloors() {
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) for(float factor:{0.f,.7f,1.f,1.2f})
    for(float drop:{0.f,.12f}) for(bool armed:{false,true}) {
        BodyRig(game,armed);testConfig.firstPersonFullBodyIK=true;
        testConfig.firstPersonBodyScale=factor;testConfig.firstPersonMovementStabilization=true;
        testConfig.firstPersonAnchorY=-32;testConfig.firstPersonAnchorZ=140;
        testConfig.firstPersonBodyForwardCm=0;
        g_holsters.active=true;g_holsters.weapon=1;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_bodySkinScope=true;g_autoBodyCalibration={};g_heading.base=0;g_haveManualInput=false;
        auto* obj=Ptr<uint8_t>(dll.objects);
        const NativeLandingPose* standing=nullptr;
        for(const auto& s:nativeLandingPoses) if(s.game==game && s.clip==103) standing=&s;
        Check(standing!=nullptr,"installed game has a standing reference for landing regression");
        Frame inverseBind[15]{};
        for(int j=0;j<15;++j) {
            Inverse(standing->joints[j],inverseBind[j]);
            auto* b=reinterpret_cast<float*>(obj+192)+j*16;
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) b[c*4+r]=inverseBind[j].basis.r[r][c];
            b[12]=inverseBind[j].origin.x;b[13]=inverseBind[j].origin.y;b[14]=inverseBind[j].origin.z;b[15]=1;
        }
        trackingtest::Clock clock;
        for(size_t n=0;n<=std::size(nativeLandingPoses);++n) {
            const auto& s=n==std::size(nativeLandingPoses)?*standing:nativeLandingPoses[n];
            if(s.game!=game) continue;
            Field<int16_t>(off::item_anim_state)=s.clip==92?1:2;
            Field<int16_t>(off::item_anim_number)=int16_t(s.clip);
            testViewRise=s.clip==103?0:-drop;testVR.m_rawHeadPose.m[1][3]=1.7f+testViewRise;
            const motiongun::Vec eye=Transform(s.joints[14],{0,-32,140});
            g_scenePose={int32_t(eye.x),int32_t(eye.y),int32_t(eye.z)};
            for(int j=0;j<15;++j) WriteRows(Multiply(s.joints[j],inverseBind[j]),ikNative+j*12);
            clock.Advance(1./30);++testVR.m_poseSerial;
            const auto camera=g_scenePose;
            const auto simulationBefore=std::string(reinterpret_cast<char*>(item),off::item_stride);
            const float size=AnimatedBodyScale(item);
            Detour_GetJoints(item);
            Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)) &&
                simulationBefore==std::string(reinterpret_cast<char*>(item),off::item_stride),
                "landing palette fitting leaves camera, collision, animation and damage state untouched");
            const float* physics=FirstPersonNativeBodySkin();
            Check(physics && !std::memcmp(physics,ikNative,15*12*sizeof(float)),
                "landing physics retains the unmodified native animation palette");
            Frame out[15]{};
            for(int j=0;j<15;++j) Check(ReadIKJoint(obj,g_bodyRenderPalette,15,0,nullptr,nullptr,j,out[j]),"landing joint readable");
            if(s.clip!=103) {
                CheckLandingLegPalette(s.joints,out,size);
                for(int hand=0;hand<2;++hand) {
                    Frame wrist{};Basis basis{};
                    Check(BuildControllerWrist(hand,wrist,basis) && firstperson::Length(Sub(out[hand?10:13].origin,wrist.origin))<.1f,
                        "native landing legs retain exact live controller wrists");
                }
                if(s.clip==24 && s.frame==0) for(int missing:{0,1,2}) {
                    const auto status=*Ptr<int16_t>(dll.lara+2);
                    testMissingHand=missing<2?missing:-1;
                    if(missing==2) *Ptr<int16_t>(dll.lara+2)=1;
                    ++testVR.m_poseSerial;Detour_GetJoints(item);
                    for(int j=0;j<15;++j) ReadIKJoint(obj,g_bodyRenderPalette,15,0,nullptr,nullptr,j,out[j]);
                    CheckLandingLegPalette(s.joints,out,size);
                    testMissingHand=-1;*Ptr<int16_t>(dll.lara+2)=status;
                }
            } else Check(g_bodyFollower.valid && !g_bodyLanding,"standing rendering resumes after native landing recovery");
        }
    }
    testViewRise=0;
}
