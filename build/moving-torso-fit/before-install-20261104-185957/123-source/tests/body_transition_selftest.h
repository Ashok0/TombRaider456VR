namespace {
#include "sidearm_animation_steps.inl"
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

void TestStandingCalibration() {
    TestUprightTorsoFit();
    TestBodyCenterReference();
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) {
        BodyRig(game,false);testConfig.firstPersonFullBodyIK=true;testConfig.firstPersonBodyScale=0;
        testConfig.firstPersonMovementStabilization=true;testConfig.firstPersonBodyForwardCm=11;
        g_holsters.active=true;g_holsters.weapon=1;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_autoBodyCalibration={};g_heading.base=0;g_haveManualInput=false;g_bodySkinScope=true;
        auto* obj=Ptr<uint8_t>(dll.objects);
        auto render=[&] {
            float palette[384];std::memcpy(palette,ikNative,sizeof(palette));++testVR.m_poseSerial;
            Check(ApplyFullBodyIK(item,palette,15),"standing calibration renders full body");
            std::array<Frame,15> pose{};
            for(int j=0;j<15;++j) ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,pose[j]);
            return pose;
        };
        ResetBodyFollower();const auto upright=render();
        for(float drop:{.03f,.06f,-.04f,0.f}) {
            testViewRise=-drop;const auto pose=render();
            Check(std::fabs(pose[0].origin.y-upright[0].origin.y)<.1f,
                  "small headset-height changes cannot bend previously straight legs");
            for(int first:{1,4}) {
                const float reach=firstperson::Length(Sub(pose[first+1].origin,pose[first].origin))+firstperson::Length(Sub(pose[first+2].origin,pose[first+1].origin));
                Check(firstperson::Length(Sub(pose[first+2].origin,pose[first].origin))>reach*.999f,
                      "standing knees remain nearly straight without System recenter");
            }
        }
        testViewRise=-.25f;g_torsoPose.Reset();const auto crouched=render();
        Check(crouched[0].origin.y>upright[0].origin.y+50,"intentional crouch still lowers hips");
        testViewRise=0;g_torsoPose.Reset();
        Check(std::fabs(render()[0].origin.y-upright[0].origin.y)<.1f,"standing restores upright hips");
        ResetBodyFollower();g_autoBodyCalibration.pending=true;render();
        Check(!g_bodySize.valid && g_groundTorso.valid,"uncalibrated render seeds temporary torso reference");
        const float oldChestOffset=g_groundTorso.chestOffset.y;
        g_autoBodyCalibration.pending=false;render();
        Check(g_bodySize.valid && std::fabs(g_groundTorso.chestOffset.y-oldChestOffset*g_bodySize.factor)<.1f,
              "late automatic scaling refreshes torso reference at new size");
        for(float cm:{0.f,11.f,20.f}) {
        testConfig.firstPersonBodyForwardCm=cm;testViewRise=0;g_torsoClearance.Reset();
        const auto neutral=render();float largest=0;
        for(float drop:{.05f,.08f,.09f}) for(int frame=0;frame<40;++frame) {
            testViewRise=-drop;g_torsoClearance.time-=1./90;
            const auto pose=render();largest=std::max(largest,std::fabs(g_torsoClearance.angle));
            for(int j:{0,3,6}) Check(firstperson::Length(Sub(pose[j].origin,neutral[j].origin))<.1f,
                  "live torso clearance preserves hip height, body offset and planted feet");
            for(int hand=0;hand<2;++hand) {
                Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
                Check(firstperson::Length(Sub(pose[hand?10:13].origin,wrist.origin))<.1f,
                      "production clearance keeps controller wrists exact while torso bends");
            }
        }
        if(cm<20) Check(largest>.001f,"production standing solver exercises upper-torso clearance");
        }
        // The real render path must consume measured geometry and keep its
        // calibration fixed after the first upright refinement.
        testViewRise=0;
        g_bodySize.Reset();g_torsoPose.Reset();g_groundTorso.Reset();g_torsoClearance.Reset();g_torsoSurface.Reset();
        const auto unfitted=render();const float oldFactor=g_bodySize.factor;
        const auto scene=g_scenePose;
        g_torsoSurface.Add(obj,998,{-140,-190,-80},{140,100,40},200);
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
        testViewRise=-.05f;render();
        Check(g_bodySize.factor==heldFactor && g_bodySize.eyeGap==heldReserve,
              "head movement cannot change the captured visual fit");
        g_holsters={};g_bodySkinScope=false;
    }
}
void TestBodyTransitions() {
    TestStandingCalibration();
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) for(bool fullBody:{false,true})
    for(float yaw:{0.f,1.5707963f,-1.5707963f}) for(float cm:{0.f,5.f,11.f,20.f}) {
        BodyRig(game,false);g_holsters.active=true;g_holsters.weapon=1;testConfig.firstPersonFullBodyIK=fullBody;testConfig.firstPersonBodyScale=1;
        testConfig.firstPersonBodyForwardCm=cm;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_haveManualInput=false;g_heading.base=yaw;
        Field<PHD_3DPOS>(off::item_pos).y_rot=Field<PHD_3DPOS>(off::item_pos_prev).y_rot=locomotion::Angle(yaw);
        auto* obj=Ptr<uint8_t>(dll.objects);
        int step=0;Frame standingChest{};
        for(int status:{0,2,4,3,0,0}) {
            *Ptr<int16_t>(dll.lara+2)=int16_t(status);
            Field<int16_t>(off::item_anim_state)=step<2 ? 1 : 2;
            Field<uint32_t>(off::item_mesh_bits)=0x3fff;g_bodySkinScope=true;
            for(int j=0;j<15;++j) {
                const auto* bind=Ptr<float>(dll.objects+192)+j*16;
                const motiongun::Vec point{-bind[12],-bind[13],-bind[14]};
                Frame animated{firstperson::IdentityBasis(),Add(point,{step*13.f,0,-step*39.f})};
                if(j>=7) {
                    const Basis pitch=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},step*.055f));
                    const motiongun::Vec pivot{step*13.f,-650,-step*39.f};
                    animated=Multiply({pitch,Sub(Add(pivot,{0,0,-step*23.f}),Transform(pitch,pivot))},animated);
                }
                WriteRows(Multiply(animated,{firstperson::IdentityBasis(),Scale(point,-1)}),ikNative+j*12);
                for(auto offset:{off::item_joints_cur,off::item_joints_prev}) {
                    auto* joint=reinterpret_cast<int32_t*>(item+offset+j*48);
                    float rows[12];WriteRows(animated,rows);
                    for(int k=0;k<12;++k) joint[k]=int32_t(rows[k]*16384);
                }
            }
            ++testVR.m_poseSerial;
            if(fullBody) {
                float palette[384];std::memcpy(palette,ikNative,sizeof(palette));
                Check(ApplyFullBodyIK(item,palette,15),"transition regression exercises complete body IK");
            }
            Detour_GetJoints(item);uint32_t mask=0;const auto* render=FirstPersonBodySkin(mask);
            Frame hip{};Check(render && ReadIKJoint(obj,render,15,0,nullptr,nullptr,0,hip),"transition pelvis is readable");
            Check(std::fabs(hip.origin.x-std::sin(yaw)*cm*10)<.05f && std::fabs(hip.origin.z-std::cos(yaw)*cm*10)<.05f,
                  "run-to-stop animation cannot move body from its configured position");
            Frame chest{};ReadIKJoint(obj,render,15,0,nullptr,nullptr,7,chest);
            if(step==0) standingChest=chest;
            Check(std::fabs(chest.origin.x-standingChest.origin.x)<.06f && std::fabs(chest.origin.z-standingChest.origin.z)<.06f && (fullBody || std::fabs(chest.origin.y-standingChest.origin.y)<.06f),
                  "native running chest and stop blend cannot move the visible torso backwards");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) Check(std::fabs(chest.basis.r[r][c]-standingChest.basis.r[r][c])<.001f,
                  "chest mesh cannot pitch back into the headset on stopping");
            float sphere[4]{};uint64_t clearanceMask=0;
            Check(FirstPersonBodyClearance(sphere,clearanceMask) && std::fabs(sphere[3]-(fullBody ? 30.f : 200.f))<.001f && clearanceMask==0x40ff,
                  "clearance affects body and excludes tracked arms/hands");
            if(fullBody) {
                float inverse[12]{};uint64_t torsoMask=0;bool measured=false;
                Check(FirstPersonTorsoMeasureInfo(inverse,torsoMask,measured) && torsoMask==(1ull<<7),
                      "production torso measurement selects the rendered chest palette slot");
                const float empty[3]={};FirstPersonTorsoMeasure(999,empty,empty,0);
                Check(FirstPersonTorsoBufferKnown(999) && !g_torsoSurface.valid,
                      "arm-only buffers are cached without inventing torso bounds");
            }
            Ptr<int32_t>(dll.w2vMatrix)[3]=32;
            float otherEye[4]{};uint64_t otherMask=0;FirstPersonBodyClearance(otherEye,otherMask);
            Check(std::fabs(otherEye[0]+32-sphere[0])<.01f && otherMask==clearanceMask,"clearance is stable between stereo origins");
            Ptr<int32_t>(dll.w2vMatrix)[3]=0;
            for(int excluded=0;excluded<4;++excluded) {
                const bool activeBefore=g_active,scopeBefore=g_bodySkinScope;const int armBefore=g_renderArm;
                if(excluded==0) g_active=false;
                if(excluded==1) g_bodySkinScope=false;
                if(excluded==2) g_renderArm=1;
                if(excluded==3) *Ptr<int32_t>(dll.renderPass)=4;
                Check(!FirstPersonBodyClearance(sphere,clearanceMask) && sphere[3]==0 && clearanceMask==0,
                      "third-person, non-body, tracked hand and shadow draws cannot inherit clearance");
                g_active=activeBefore;g_bodySkinScope=scopeBefore;g_renderArm=armBefore;*Ptr<int32_t>(dll.renderPass)=0;
            }
            for(int hand=0;hand<2;++hand) {
                Frame wrist{},shown{};Basis controller{};BuildControllerWrist(hand,wrist,controller);
                Check(ReadIKJoint(obj,render,15,0,nullptr,nullptr,hand?10:13,shown) && firstperson::Length(Sub(shown.origin,wrist.origin))<.05f,
                      "draw and holster preserve live controller wrists");
            }
            Check(!std::memcmp(FirstPersonNativeBodySkin(),ikNative,15*12*sizeof(float)),"transition physics stays native");
            ++step;
        }
    }
    for(int game:{1,2,3}) for(bool fullBody:{false,true}) for(int weapon:{1,2,3}) {
        BodyRig(game,false);testConfig.firstPersonFullBodyIK=fullBody;testConfig.firstPersonBodyScale=1;
        testConfig.firstPersonBodyForwardCm=11;testConfig.firstPersonBodyHandYawWeight=0;
        g_groundEye.valid=true;g_groundEye.bodyLocal={};g_haveManualInput=false;
        *Ptr<int16_t>(dll.lara+4)=int16_t(weapon);g_holsters.active=false;
        auto* obj=Ptr<uint8_t>(dll.objects);
        auto render=[&](int status,bool repeat) {
            *Ptr<int16_t>(dll.lara+2)=int16_t(status);Field<uint32_t>(off::item_mesh_bits)=0x3fff;
            g_pistolBodyScope=g_bodySkinScope=true;g_pistolForearmScope=false;
            if(!repeat) ++testVR.m_poseSerial;
            Detour_GetJoints(item);uint32_t mask=0;const auto* palette=FirstPersonBodySkin(mask);
            std::array<Frame,15> result{};
            for(int j=0;j<15;++j) Check(palette && ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,result[j]),"sidearm body joints are readable");
            if(PistolBodyPoseMatches(item,0)) {
                Field<uint32_t>(off::item_mesh_bits)=0x3600;g_pistolBodyScope=false;g_pistolForearmScope=true;
                Detour_GetJoints(item);palette=FirstPersonBodySkin(mask);
                for(int j=8;j<14;++j) {
                    Frame shown{};ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,shown);
                    Check(firstperson::Length(Sub(shown.origin,result[j].origin))<.05f,"attachments share the animated sidearm wrists");
                }
            }
            g_pistolBodyScope=g_pistolForearmScope=false;
            return result;
        };
        ExerciseSidearmAnimation(render,[&] {
            for(int hand=0;hand<2;++hand) testControllerOffset[hand].x+=.05f;
        },[&] {
            for(int j=8;j<14;++j) {
                ikNative[j*12+11]+=30;
                for(auto offset:{off::item_joints_cur,off::item_joints_prev})
                    reinterpret_cast<int32_t*>(item+offset+j*48)[11]+=30*16384;
            }
        });
    }
    for(int game:{1,2,3}) for(bool armed:{false,true}) {
        PhysicalBodySetup(game,armed);
        for(int state:{23,45,74}) {
            Field<int16_t>(off::item_anim_state)=int16_t(state);
            const bool roll=state!=74 || game==3;
            Check(IsRollState(item)==roll,"sprint somersault exists in TR3; ordinary rolls are shared");
            SetMeshVisibility(true,roll);
            Check(g_rollHidden==roll,"tracked controllers do not cancel somersault hiding");
            physicalDrawCalls=0;Detour_DrawCreatureHD(item,0);
            if(roll) Check(physicalDrawCalls==0,"somersault suppresses HD body even with weapon tracking");
        }
        SetMeshVisibility(false,false);
        for(int restIndex:{1,6}) for(int status:{2,3}) {
            IKSetup(game);*Ptr<int16_t>(dll.lara+2)=int16_t(status);Field<uint32_t>(off::item_mesh_bits)=0x3600;
            auto* rest=Ptr<uint8_t>(dll.laraHands)+restIndex*104;auto* open=rest+3*104;
            auto* geom=Ptr<uint8_t>(dll.objects+88);
            std::memset(rest,1,104);std::memset(open,4,104);
            *reinterpret_cast<void**>(rest+16)=reinterpret_cast<void*>(0x1234);
            std::memcpy(geom,open,104);
            { UnarmedRestHandScope scope(item,1);Check(!std::memcmp(geom,rest,104),"draw/holster keeps relaxed fingers instead of open hands"); }
            Check(!std::memcmp(geom,open,104),"native hand descriptor is restored after rendering");
        }
    }
}
}
