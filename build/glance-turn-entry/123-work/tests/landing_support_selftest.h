#include "landing_support_checks.inl"
void TestLandingSupportPalettes() {
    using namespace tr;using namespace tr::motiongun;
    for(bool measured:{false,true}) for(int game:{1,2,3}) for(float factor:{0.f,.7f,1.f,1.2f})
    for(float drop:{0.f,.12f,.15f,.3f}) for(bool armed:{false,true}) {
        BodyRig(game,armed);testConfig.firstPersonFullBodyIK=true;
        testConfig.firstPersonBodyScale=factor;testConfig.firstPersonMovementStabilization=true;
        testConfig.firstPersonAnchorY=-32;testConfig.firstPersonAnchorZ=140;
        testConfig.firstPersonBodyForwardCm=0;
        g_holsters.active=true;g_holsters.weapon=1;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_bodySkinScope=true;g_autoBodyCalibration={};g_heading.base=0;g_haveManualInput=false;
        auto* obj=Ptr<uint8_t>(dll.objects);
        g_torsoSurface.Reset();
        if(measured) g_torsoSurface.Add(obj,1,{-67.2f,-199.6f,-91.7f},{69.2f,-32.9f,64.4f},4185);
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
        if(factor==0) {g_bodySize.valid=true;g_bodySize.torsoFitted=true;g_bodySize.factor=.845f;g_bodySize.eyeGap=.209f*LiveWorldUnitsPerMetre();g_bodySize.units=LiveWorldUnitsPerMetre();g_bodySizeObject=0;}
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
            // Feed the real fixed-point head query used by the collection's
            // camera; a camera-only change must also preserve fitted contacts.
            for(int j=0;j<15;++j) for(auto offset:{off::item_joints_prev,off::item_joints_cur}) {
                auto* joint=reinterpret_cast<int32_t*>(item+offset+j*off::joint_stride);
                for(int row=0;row<3;++row) {
                    for(int col=0;col<3;++col) joint[row*4+col]=int32_t(std::lround(s.joints[j].basis.r[row][col]*16384));
                    const float value=row==0?s.joints[j].origin.x:row==1?s.joints[j].origin.y:s.joints[j].origin.z;
                    joint[row*4+3]=int32_t(std::lround(value*16384));
                }
            }
            if(s.clip!=103) {
                Check(Anchor(g_scenePose),"supported landing uses actual collection camera");
                Check(g_scenePose.y_pos<=s.joints[14].origin.y-31.f,
                    "landing eye stays above the animated neck without rotating forward clearance down");
            }
            const auto camera=g_scenePose;
            const auto simulationBefore=std::string(reinterpret_cast<char*>(item),off::item_stride);
            const float size=factor==0?.845f:factor;
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
                CheckLandingSupport(s.joints,out,size);
                Frame inverseChest{};Inverse(s.joints[7],inverseChest);
                const Frame connected=Multiply(Multiply(out[7],inverseChest),s.joints[14]);
                const float finalEye=g_scenePose.y_pos+VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre();
                Check(finalEye<=connected.origin.y+2.f,
                    "final collection camera cannot sink below the neck attached to the rendered torso");
                const float reserve=factor==0 ? g_bodySize.eyeGap : 0.f;
                Check(finalEye<=connected.origin.y-reserve+2.f,
                    "collection landing preserves calibrated eye clearance instead of using the neck base");
                if(measured) {
                    float top=INFINITY;
                    for(int corner=0;corner<8;++corner) {
                        const motiongun::Vec local{corner&1 ? 69.2f : -67.2f,corner&2 ? -32.9f : -199.6f,corner&4 ? 64.4f : -91.7f};
                        top=std::min(top,Transform(out[7],local).y);
                    }
                    Check(finalEye<=top-.25f*LiveWorldUnitsPerMetre()+2.f,
                        "collection impact eye preserves clearance above the actual rendered collar");
                }
                Check(g_bodyLanding && !g_bodyFollower.valid && !g_torsoPose.valid,
                    "native impact suspends cached body and torso inference");
                Check(!MotionReady() && !UnarmedIKReady() && !FullBodyIKReady(),
                    "native impact disables tracked weapons and both arm/body IK paths");
                const float nativeSize=factor==0 ? .845f : factor;
                const motiongun::Vec pivot{s.joints[0].origin.x,std::max(s.joints[3].origin.y,s.joints[6].origin.y),s.joints[0].origin.z};
                for(int j=0;j<14;++j) {
                    const auto expected=Add(pivot,Scale(Sub(s.joints[j].origin,pivot),nativeSize));
                    Check(firstperson::Length(Sub(out[j].origin,expected))<.15f,
                        "landing preserves every native body and arm joint position at calibrated size");
                    for(int row=0;row<3;++row) for(int col=0;col<3;++col)
                        Check(std::fabs(out[j].basis.r[row][col]-s.joints[j].basis.r[row][col]*nativeSize)<.001f,
                            "impact and recovery preserve native limb angles without VRIK contortion");
                }
                if(factor==0) Check(g_bodySize.valid && g_bodySize.torsoFitted && g_bodySize.factor==.845f &&
                    g_bodySizeObject==0 && g_bodySize.units==LiveWorldUnitsPerMetre(),
                    "native impact keeps existing height and body-size calibration");
            } else Check(g_bodyFollower.valid && !g_bodyLanding,"standing rendering resumes after native landing recovery");
        }
    }
    testViewRise=0;
}
