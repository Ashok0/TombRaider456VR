#include "sidearm_animation_steps.inl"
void TestBodyTransitions(uint8_t* objectInfo) {
    using namespace tr;using namespace tr::motiongun;
    const auto savedHolsters=g_holsters;g_holsters.active=true;g_holsters.weapon=1;
    const Vec points[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    std::memset(objectInfo,0,off::object_stride);
    for(int j=0;j<15;++j) {
        auto* bind=reinterpret_cast<float*>(objectInfo+1676)+j*16;
        bind[0]=bind[5]=bind[10]=bind[15]=1;
        bind[12]=-points[j].x;bind[13]=-points[j].y;bind[14]=-points[j].z;
        bodyJointWorld[j]=points[j];
    }
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&FakeBodyJoint)-g_boundBase);
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    float fixture[180]{},palette[180]{};skinFixture=fixture;skinFixtureCount=15;
    g_scenePose={0,-850,0};worldCameraValid=true;
    worldCameraPos[0]=worldCameraPos[1]=worldCameraPos[2]=0;
    g_bodyVisualOffset=g_bodyWallOffset={};g_groundEye.valid=true;g_groundEye.bodyLocal={};
    config.firstPersonBodyScale=1;config.firstPersonJoint=14;
    config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=0;
    g_bodySkinScope=true;g_shadowDll=nullptr;g_haveManualInput=false;
    *reinterpret_cast<int16_t*>(laraMemory+4)=1;
    for(float yaw:{0.f,1.5707963f,-1.5707963f}) for(float cm:{0.f,5.f,11.f,20.f}) {
        ResetBodyFollower();g_headingBase=yaw;
        reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)->y_rot=Angle(yaw);
        reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev)->y_rot=Angle(yaw);
        config.firstPersonBodyForwardCm=cm;
        int step=0;Frame standingChest{};
        for(int status:{0,2,4,3,0,0}) {
            *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=step<2 ? 1 : 2;
            *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x3fff;
            // A run/stop blend carries its whole native skeleton backwards.
            for(int j=0;j<15;++j) {
                bodyJointWorld[j]=Add(points[j],{step*13.f,0,-step*39.f});
                Frame animated{firstperson::IdentityBasis(),bodyJointWorld[j]};
                if(j>=7) {
                    const Basis pitch=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},step*.055f));
                    const Vec pivot=Add(points[7],{step*13.f,0,-step*39.f});
                    animated=Multiply({pitch,Sub(Add(pivot,{0,0,-step*23.f}),Transform(pitch,pivot))},animated);
                    bodyJointWorld[j]=animated.origin;
                }
                WriteRows(Multiply(animated,{firstperson::IdentityBasis(),Scale(points[j],-1)}),fixture+j*12);
            }
            ++VR().m_poseSerial;
            if(config.firstPersonFullBodyIK) {
                std::memcpy(palette,fixture,sizeof(palette));
                Check(ApplyFullBodyIK(itemMemory,palette,15),"transition regression exercises the complete body solver");
            }
            Detour_GetJoints(itemMemory,palette,0);
            Frame pelvis{};ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,0,pelvis);
            const auto hip=pelvis.origin;
            Check(std::fabs(hip.x-std::sin(yaw)*cm*10)<.05f && std::fabs(hip.z-std::cos(yaw)*cm*10)<.05f,
                  "run-to-stop animation cannot slide the configured body position");
            Frame chest{};ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,7,chest);
            if(step==0) standingChest=chest;
            Check(std::fabs(chest.origin.x-standingChest.origin.x)<.06f && std::fabs(chest.origin.z-standingChest.origin.z)<.06f && (config.firstPersonFullBodyIK || std::fabs(chest.origin.y-standingChest.origin.y)<.06f),
                  "animated chest pitch and run-to-stop blend cannot slide the visible torso");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) Check(std::fabs(chest.basis.r[r][c]-standingChest.basis.r[r][c])<.001f,
                  "torso mesh orientation stays stable when native stop animation pitches backwards");
            float sphere[4]{};uint64_t clearanceMask=0;
            Check(FirstPersonBodyClearance(sphere,clearanceMask) && std::fabs(sphere[3]-(config.firstPersonFullBodyIK ? 30.f : 200.f))<.001f && clearanceMask==0x40ff,
                  "headset clearance protects body surfaces and excludes all arm and hand joints");
            if(config.firstPersonFullBodyIK) {
                float inverse[12]{};uint64_t torsoMask=0;bool measured=false;
                Check(FirstPersonTorsoMeasureInfo(inverse,torsoMask,measured) && torsoMask==(1ull<<7),
                      "production torso measurement selects the rendered chest palette slot");
                const float empty[3]={};FirstPersonTorsoMeasure(999,empty,empty,0);
                Check(FirstPersonTorsoBufferKnown(999) && !g_torsoSurface.valid,
                      "arm-only buffers are cached without inventing torso bounds");
            }
            worldCameraPos[0]=32;
            float otherEye[4]{};uint64_t otherMask=0;FirstPersonBodyClearance(otherEye,otherMask);
            Check(std::fabs(otherEye[0]+32-sphere[0])<.01f && otherMask==clearanceMask,"both eyes share one world-space clearance volume");
            worldCameraPos[0]=0;
            for(int excluded=0;excluded<4;++excluded) {
                const bool activeBefore=g_active,scopeBefore=g_bodySkinScope,mirrorBefore=g_nativeMirrorDraw;
                const int armBefore=g_renderArm;
                if(excluded==0) g_active=false;
                if(excluded==1) g_bodySkinScope=false;
                if(excluded==2) g_renderArm=1;
                if(excluded==3) g_nativeMirrorDraw=true;
                Check(!FirstPersonBodyClearance(sphere,clearanceMask) && sphere[3]==0 && clearanceMask==0,
                      "third-person, non-body, tracked hand and mirror draws clear the headset cutout");
                g_active=activeBefore;g_bodySkinScope=scopeBefore;g_nativeMirrorDraw=mirrorBefore;g_renderArm=armBefore;
            }
            for(int hand=0;hand<2;++hand) {
                Frame wrist{};Basis controller{};BuildControllerWrist(hand,wrist,controller);
                Frame shown{};ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,hand?10:13,shown);
                Check(firstperson::Length(Sub(shown.origin,wrist.origin))<.05f,
                      "draw/holster keeps both rendered wrists on the live controllers");
            }
            uint64_t visible=0;int count=0;
            Check(!std::memcmp(FirstPersonBodyPalette(visible,count),fixture,sizeof(fixture)),
                  "body placement and holstering leave the physics skeleton native");
            ++step;
        }
    }
    g_holsters.active=false;config.firstPersonBodyHandYawWeight=0;
    const bool savedLongHooks=g_longGunHooksReady;g_longGunHooksReady=true;
    for(int weapon:{1,2,3}) {
        ResetBodyFollower();g_headingBase=0;
        reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)->y_rot=0;
        reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev)->y_rot=0;
        *reinterpret_cast<int16_t*>(laraMemory+4)=int16_t(weapon);
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
        config.firstPersonBodyForwardCm=11;
        auto render=[&](int status,bool repeat) {
            *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);
            *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x3fff;
            g_pistolBodyScope=g_bodySkinScope=true;g_pistolForearmScope=false;
            if(!repeat) ++VR().m_poseSerial;
            Detour_GetJoints(itemMemory,palette,0);
            std::array<Frame,15> result{};
            for(int j=0;j<15;++j) Check(ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,result[j]),"sidearm body joints are readable");
            if(PistolBodyPoseMatches(itemMemory,0)) {
                *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x3600;
                g_pistolBodyScope=false;g_pistolForearmScope=true;
                float attachment[180];Detour_GetJoints(itemMemory,attachment,0);
                for(int j=8;j<14;++j) {
                    Frame shown{};ReadIKJoint(objectInfo,attachment,15,0,nullptr,nullptr,j,shown);
                    Check(firstperson::Length(Sub(shown.origin,result[j].origin))<.05f,"sidearm attachment follows the body's animated wrists and forearms");
                }
            }
            g_pistolBodyScope=g_pistolForearmScope=false;
            return result;
        };
        ExerciseSidearmAnimation(render,[&] {
            for(int hand=0;hand<2;++hand) VR().m_rawControllerPose[hand].m[0][3]+=.05f;
        },[&] {
            for(int j=8;j<14;++j) { fixture[j*12+11]+=30;bodyJointWorld[j].z+=30; }
        });
    }
    g_longGunHooksReady=savedLongHooks;
    g_holsters=savedHolsters;g_pistolAnimation.Reset();
    skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
}

void TestBodyCenterReference() {
    using namespace tr;
    for(float base:{0.f,.7f,1.5707963f,-2.6f}) for(float capturedSide:{-44.f,44.f})
    for(float anchorX:{0.f,13.f}) {
        stabilization::GroundEye reference;
        const float bodyYaw=base+.6f;
        const auto animatedFlat=locomotion::Rotate({capturedSide,133},bodyYaw);
        const stabilization::Point root{100,500,300};
        const stabilization::Point animated{root.x+animatedFlat.x,root.y-700,root.z+animatedFlat.z};
        const auto eye=reference.Apply(root,base,animated,bodyYaw,false,anchorX);
        Check(std::fabs(reference.bodyLocal.x-anchorX)<.001f && std::fabs(reference.bodyLocal.z-133)<.001f,
              "first-frame neck sway cannot become a permanent sideways body offset");
        Check(std::fabs(eye.x-animated.x)<.001f && std::fabs(eye.y-animated.y)<.001f && std::fabs(eye.z-animated.z)<.001f,
              "body centering does not move the calibrated scene camera");
        for(float turn:{0.f,.8f,3.1415926f,6.2831853f}) {
            const float yaw=bodyYaw+turn;
            const auto fromRoot=locomotion::Rotate({reference.local.x,reference.local.z},base+turn);
            const auto bodyOffset=reference.BodyOffsetAtEye(base+turn,yaw,{},{},423,fromRoot);
            const auto relative=locomotion::Rotate(fromRoot-bodyOffset,-yaw);
            Check(std::fabs(relative.x-anchorX)<.001f && std::fabs(relative.z-133)<.001f,
                  "centered reference survives stick and physical turns without requiring a full revolution");
        }
        const auto previous=reference.local;
        reference.Resume(true,base,base+1.1f,0);
        reference.Apply(root,base+1.1f,{animated.x+80,animated.y+20,animated.z-60},bodyYaw,false,anchorX);
        Check(reference.bodyLocal.x==anchorX && reference.local.y==previous.y,
              "runtime recenter preserves centered lateral fit and calibrated height");
    }
}

void TestUprightTorsoFit() {
    using namespace tr::firstperson;using Vec=tr::motiongun::Vec;
    const Vec points[15]={{0,-380,0},{-45,-380,0},{-45,-200,50},{-45,0,0},
        {45,-380,0},{45,-200,50},{45,0,0},{0,-428,14},
        {90,-580,0},{125,-460,20},{90,-320,50},{-90,-580,0},{-125,-460,20},{-90,-320,50},{0,-650,0}};
    Frame native[15]{};for(int j=0;j<15;++j) native[j]={IdentityBasis(),points[j]};
    const Vec eye{0,-682,140};const float units=423;
    BodySizeCalibration size;
    Check(size.Capture(native,eye,eye.y,units),"upright torso fixture captures eye/floor height");
    const float original=size.factor;
    Check(size.FitTorso(native,{-67,-199,-92},{69,-33,64},eye.y),"visible torso refines the upright body fit");
    Check(size.factor<original && size.factor>=original*.85f && size.eyeGap>0,
          "visual body is proportionally fitted within a fifteen-percent refinement");
    const float fitted=size.factor,reserve=size.eyeGap;
    Check(!size.FitTorso(native,{-67,-500,-92},{69,-33,64},eye.y+100) && size.factor==fitted && size.eyeGap==reserve,
          "looking down cannot repeatedly shrink the calibrated body");
    auto solve=[&](float factor,float gap,Vec target) {
        Frame scaled[15]{},posed[15]{};ScaleBody(native,factor,scaled);
        Frame inverse{};Inverse(native[14],inverse);
        const Vec anchor=Transform(scaled[14],Transform(inverse,eye));
        Check(SolveFollowingBody(scaled,anchor,target,0,IdentityBasis(),{0,0,1},units,{},0,true,0,0,posed,1,gap),
              "fitted upright pose solves with the actual eye target");
        std::array<Frame,15> out{};for(int j=0;j<15;++j) out[j]=posed[j];return out;
    };
    const auto old=solve(original,0,eye),upright=solve(fitted,reserve,eye);
    Check(upright[7].origin.y>old[7].origin.y+units*.05f,
          "the corrected standing chest remains lower instead of lifting toward the headset");
    for(float drop:{0.f,.05f,.2f}) {
        const Vec target=Add(eye,{0,drop*units,0});const auto pose=solve(fitted,reserve,target);
        for(int first:{1,4}) {
            Check(std::fabs(pose[first+2].origin.y)<.1f,"upright fitting leaves both feet on the original floor");
            const float reach=Length(Sub(pose[first+1].origin,pose[first].origin))+Length(Sub(pose[first+2].origin,pose[first+1].origin));
            Check(Length(Sub(pose[first+2].origin,pose[first].origin))>reach*.999f,
                  "lower torso fit retains nearly straight standing legs");
        }
        Check(std::fabs(pose[7].origin.y-upright[7].origin.y)<.1f,
              "the standing chest does not rise as the tracked eyes move down");
        Check(std::fabs(pose[14].origin.y-(target.y+(682-650)*fitted))<.1f,
              "visual fit does not move the tracked head target");
    }
}

void TestStandingCalibration(uint8_t* objectInfo) {
    TestUprightTorsoFit();
    TestBodyCenterReference();
    using namespace tr;using namespace tr::motiongun;
    const auto savedHolsters=g_holsters;g_holsters.active=true;g_holsters.weapon=1;
    const Vec points[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    std::memset(objectInfo,0,off::object_stride);
    for(int j=0;j<15;++j) {
        auto* bind=reinterpret_cast<float*>(objectInfo+1676)+j*16;
        bind[0]=bind[5]=bind[10]=bind[15]=1;
        bind[12]=-points[j].x;bind[13]=-points[j].y;bind[14]=-points[j].z;
        bodyJointWorld[j]=points[j];
    }
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&FakeBodyJoint)-g_boundBase);
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    float fixture[180]{},palette[180]{};skinFixture=fixture;skinFixtureCount=15;
    g_scenePose={0,-850,0};worldCameraValid=true;
    worldCameraPos[0]=worldCameraPos[1]=worldCameraPos[2]=0;
    g_bodyVisualOffset=g_bodyWallOffset={};g_groundEye.valid=true;g_groundEye.bodyLocal={};
    config.firstPersonBodyScale=0;config.firstPersonJoint=14;
    config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=0;
    g_bodySkinScope=true;g_shadowDll=nullptr;g_haveManualInput=false;
    *reinterpret_cast<int16_t*>(laraMemory+4)=1;

    g_autoBodyCalibration={};g_headingBase=0;
    *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
    *reinterpret_cast<int16_t*>(laraMemory+2)=0;
    config.firstPersonBodyForwardCm=11;
    const float initialHeight=VR().m_rawHeadPose.m[1][3];
    auto render=[&] {
        for(int j=0;j<15;++j) WriteRows({firstperson::IdentityBasis(),{}},fixture+j*12);
        std::memcpy(palette,fixture,sizeof(palette));++VR().m_poseSerial;
        const bool solved=ApplyFullBodyIK(itemMemory,palette,15);
        if(!solved) std::printf("height fixture ready=%d blocked=%s status=%d weapon=%d active=%d runtime=%d\n",FullBodyIKReady(),ControllerTrackingBlockedReason(),*reinterpret_cast<int16_t*>(laraMemory+2),CurrentMotionWeapon(),g_active,g_runtimeEnabled);
        Check(solved,"standing calibration renders full body");
        std::array<Frame,15> result{};
        for(int j=0;j<15;++j) ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,result[j]);
        return result;
    };
    ResetBodyFollower();
    const auto upright=render();
    for(float drop:{.03f,.06f,-.04f,0.f}) {
        VR().m_rawHeadPose.m[1][3]=initialHeight-drop;
        const auto pose=render();
        Check(std::fabs(pose[0].origin.y-upright[0].origin.y)<.1f,
              "small standing headset-height changes cannot bend previously straight legs");
        for(int first:{1,4}) {
            const float reach=firstperson::Length(Sub(pose[first+1].origin,pose[first].origin))+firstperson::Length(Sub(pose[first+2].origin,pose[first+1].origin));
            Check(firstperson::Length(Sub(pose[first+2].origin,pose[first].origin))>reach*.999f,
                  "standing knees remain nearly straight without a runtime System recenter");
        }
    }
    VR().m_rawHeadPose.m[1][3]=initialHeight-.25f;g_torsoPose.Reset();
    const auto crouched=render();
    Check(crouched[0].origin.y>upright[0].origin.y+50,"intentional physical crouch still lowers the hips");
    VR().m_rawHeadPose.m[1][3]=initialHeight;g_torsoPose.Reset();
    Check(std::fabs(render()[0].origin.y-upright[0].origin.y)<.1f,"standing up restores the upright hips");
    // The view toggle must clear size/posture caches just like System recenter.
    for(int recenter:{0,1}) {
        VR().m_rawHeadPose.m[1][3]=initialHeight+.08f;
        if(recenter==0) {
            FirstPersonToggle();FirstPersonToggle();
            g_active=g_haveHeading=g_scenePoseValid=true;g_headingBase=0;g_headingItem=itemMemory;
            g_groundEye.valid=true;g_groundEye.bodyLocal={};
        } else FirstPersonTrackingOriginChanged(0);
        Check(!g_bodySize.valid && !g_groundTorso.valid && !g_torsoClearance.valid && Near(VR().FirstPersonVerticalOffset(),0),
              "view toggle and System recenter invalidate height and torso calibration together");
        const auto pose=render();
        Check(g_bodySize.valid,"returning to standing captures automatic body size");
        for(int first:{1,4}) {
            const float reach=firstperson::Length(Sub(pose[first+1].origin,pose[first].origin))+firstperson::Length(Sub(pose[first+2].origin,pose[first+1].origin));
            Check(firstperson::Length(Sub(pose[first+2].origin,pose[first].origin))>reach*.999f,
                  "view toggle and System recenter both produce straight standing knees");
        }
    }
    // A load/run can render before automatic size capture becomes eligible.
    ResetBodyFollower();g_autoBodyCalibration.pending=true;render();
    Check(!g_bodySize.valid && g_groundTorso.valid,"uncalibrated render seeds a temporary torso reference");
    const float oldChestOffset=g_groundTorso.chestOffset.y;
    g_autoBodyCalibration.pending=false;render();
    Check(g_bodySize.valid && std::fabs(g_groundTorso.chestOffset.y-oldChestOffset*g_bodySize.factor)<.1f,
          "late automatic body scaling refreshes the torso reference at the new size");
    for(float cm:{0.f,11.f,20.f}) {
    config.firstPersonBodyForwardCm=cm;g_torsoClearance.Reset();
    VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1];
    const auto neutral=render();float largest=0;
    for(float drop:{.05f,.08f,.09f}) for(int frame=0;frame<40;++frame) {
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-drop;
        g_torsoClearance.time-=1./90;
        const auto pose=render();largest=std::max(largest,std::fabs(g_torsoClearance.angle));
        for(int j:{0,3,6}) Check(firstperson::Length(Sub(pose[j].origin,neutral[j].origin))<.1f,
              "live torso clearance preserves standing hip height, body offset and planted feet");
        for(int hand=0;hand<2;++hand) {
            Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
            Check(firstperson::Length(Sub(pose[hand?10:13].origin,wrist.origin))<.1f,
                  "production clearance keeps controller wrists exact while the torso bends");
        }
    }
    if(cm<20) Check(largest>.001f,"production standing solver exercises nonzero upper-torso clearance");
    }
    // The real render path must consume measured geometry and keep its
    // calibration fixed after the first upright refinement.
    VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1];
    g_bodySize.Reset();g_torsoPose.Reset();g_groundTorso.Reset();g_torsoClearance.Reset();g_torsoSurface.Reset();
    const auto unfitted=render();const float oldFactor=g_bodySize.factor;
    const auto scene=g_scenePose;
    g_torsoSurface.Add(objectInfo,998,{-140,-190,-80},{140,100,40},200);
    const auto fittedPose=render();
    Check(g_bodySize.torsoFitted && g_bodySize.factor<oldFactor && g_bodySize.eyeGap>0,
          "production body render refines automatic size from the measured torso");
    Check(fittedPose[7].origin.y>unfitted[7].origin.y+5,
          "production standing torso is visibly lower after the fit");
    Check(!std::memcmp(&scene,&g_scenePose,sizeof(scene)),"visual torso fitting cannot move the scene camera");
    for(int first:{1,4}) {
        Check(std::fabs(fittedPose[first+2].origin.y-unfitted[first+2].origin.y)<.1f,
              "production refinement leaves feet planted at their original floor height");
        const float reach=firstperson::Length(Sub(fittedPose[first+1].origin,fittedPose[first].origin))+firstperson::Length(Sub(fittedPose[first+2].origin,fittedPose[first+1].origin));
        Check(firstperson::Length(Sub(fittedPose[first+2].origin,fittedPose[first].origin))>reach*.999f,
              "production fit does not restore bent standing knees");
    }
    for(int hand=0;hand<2;++hand) {
        Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
        Check(firstperson::Length(Sub(fittedPose[hand?10:13].origin,wrist.origin))<.1f,
              "smaller visual body retains exact tracked wrists");
    }
    const float heldFactor=g_bodySize.factor,heldReserve=g_bodySize.eyeGap;
    VR().m_rawHeadPose.m[1][3]-=.05f;render();
    Check(g_bodySize.factor==heldFactor && g_bodySize.eyeGap==heldReserve,
          "head movement cannot change the captured visual fit");
    g_holsters=savedHolsters;skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
}
