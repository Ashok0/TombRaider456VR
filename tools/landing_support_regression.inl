#include "landing_support_checks.inl"
const tr::motiongun::Frame* landingSupportJoints=nullptr;
void __cdecl LandingSupportJoint(uint8_t*,tr::PHD_VECTOR* point,int joint,int) {
    const auto p=tr::motiongun::Transform(landingSupportJoints[joint],{float(point->x),float(point->y),float(point->z)});
    *point={int32_t(std::lround(p.x)),int32_t(std::lround(p.y)),int32_t(std::lround(p.z))};
}
void TestLandingSupportPalette(uint8_t* objectInfo) {
    using namespace tr;using namespace tr::motiongun;
    const auto* standing=&nativeLandingPoses[0];
    for(const auto& s:nativeLandingPoses) if(s.game==dll.game+4 && s.clip==103) standing=&s;
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&LandingSupportJoint)-g_boundBase);
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
    for(bool measured:{false,true}) for(float factor:{0.f,.7f,1.f,1.2f}) for(float drop:{0.f,.12f,.15f,.3f}) for(float pitch:{0.f,.75f}) for(int status:{0,4}) {
        *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);
        config.firstPersonBodyScale=factor;ResetBodyFollower();g_autoBodyCalibration={};
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-drop;
        VR().m_rawHeadPose.m[1][1]=VR().m_rawHeadPose.m[2][2]=std::cos(pitch);
        VR().m_rawHeadPose.m[1][2]=std::sin(pitch);VR().m_rawHeadPose.m[2][1]=-std::sin(pitch);
        g_groundTorso.Reset();g_torsoSurface.Reset();g_torsoPose.Reset();
        if(measured) g_torsoSurface.Add(objectInfo,1,{-67.2f,-199.6f,-91.7f},{69.2f,-32.9f,64.4f},4185);
        if(factor==0) {g_bodySize.valid=true;g_bodySize.torsoFitted=true;g_bodySize.factor=.845f;g_bodySize.eyeGap=88.4f;g_bodySize.units=LiveWorldUnitsPerMetre();g_bodySizeObject=0;}
        for(size_t n=0;n<=std::size(nativeLandingPoses);++n) {
            const auto& s=n==std::size(nativeLandingPoses)?*standing:nativeLandingPoses[n];
            if(s.game!=dll.game+4) continue;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=s.clip==92?1:2;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=int16_t(s.clip);
            VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-(s.clip==103 ? 0.f : drop);
            landingSupportJoints=s.joints;
            const Vec eye=Transform(s.joints[14],{0,-32,140});
            g_scenePose={int32_t(eye.x),int32_t(eye.y),int32_t(eye.z)};
            for(int j=0;j<15;++j) {bodyJointWorld[j]=s.joints[j].origin;WriteRows(Multiply(s.joints[j],inverseBind[j]),fixture+j*12);}
            std::memcpy(palette,fixture,sizeof(palette));clock.Advance(1./30);++VR().m_poseSerial;
            if(s.clip!=103) Check(Anchor(g_scenePose),"supported body uses the actual corrected landing camera");
            const auto camera=g_scenePose;
            const auto memory=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            Check(ApplyFullBodyIK(itemMemory,palette,15),"native impact body renders without a tracked IK solve");
            Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)) && memory==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),
                "leg support does not alter landing camera, collision or animation");
            Frame out[15]{};
            for(int j=0;j<15;++j) Check(ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,out[j]),"supported landing joint readable");
            if(s.clip!=103) {
                CheckLandingSupport(s.joints,out,factor==0?.845f:factor);
                // The independent head target is assigned from the camera and
                // cannot validate attachment. Transport the native neck with
                // the rendered torso instead, then compare the final HMD eye.
                Frame inverseChest{};Inverse(s.joints[7],inverseChest);
                const Frame connected=Multiply(Multiply(out[7],inverseChest),s.joints[14]);
                const float finalEye=g_scenePose.y_pos+InvertRigid(VR().HeadView()).r[1][3];
                Check(finalEye<=connected.origin.y+2.f,
                    "final rendered eye never dips below the neck attached to the supported torso");
                const float reserve=factor==0 ? g_bodySize.eyeGap : 0.f;
                Check(finalEye<=connected.origin.y-reserve+2.f,
                    "landing retains calibrated eye-to-neck clearance instead of dropping to the neck base");
                if(measured) {
                    float top=INFINITY;
                    for(int corner=0;corner<8;++corner) {
                        const Vec local{corner&1 ? 69.2f : -67.2f,corner&2 ? -32.9f : -199.6f,corner&4 ? 64.4f : -91.7f};
                        top=std::min(top,Transform(out[7],local).y);
                    }
                    Check(finalEye<=top-.25f*LiveWorldUnitsPerMetre()+2.f,
                        "impact eye keeps standing clearance above the visible collar, not only the neck pivot");
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
                // Exercise the renderer too: no arm-only fallback or stale
                // pistol attachment may replace the native impact palette.
                std::memcpy(palette,fixture,sizeof(palette));
                Check(Detour_GetJoints(itemMemory,palette,0)==15 && g_bodySkinReady,
                    "native landing publishes its complete render palette through the joint hook");
                for(int j=0;j<14;++j) {
                    Frame rendered{};
                    Check(ReadIKJoint(objectInfo,g_bodyRenderPalette,15,0,nullptr,nullptr,j,rendered) &&
                        firstperson::Length(Sub(rendered.origin,out[j].origin))<.15f,
                        "render hooks cannot reapply tracked wrists or deform native landing joints");
                }
            } else Check(g_bodyFollower.valid && !g_bodyLanding,"standing solver resumes normally after landing");

        }
    }
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&FakeBodyJoint)-g_boundBase);
    landingSupportJoints=nullptr;
    skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
}
