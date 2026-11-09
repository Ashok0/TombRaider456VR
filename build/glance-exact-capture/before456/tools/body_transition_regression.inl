#include "pitched_glance_regression.inl"
#include "extended_arm_glance_regression.inl"
#include "body_anchor_capture.inl"

void TestHiddenNeckSkin() {
    using namespace tr::motiongun;
    for(int count:{15,33}) for(float yaw:{0.f,.8f,3.14f}) for(float size:{.85f,1.f}) {
        int32_t mapping[33]{};for(int i=0;i<count;++i) mapping[i]=i<15?i:14;
        std::swap(mapping[7],mapping[3]); // Palette order is not joint order.
        const auto visible=tr::firstperson::SkinJointMask(0x3fff,mapping,count);
        float palette[33*12]{},before[33*12]{};
        for(int frame=0;frame<=100;++frame) {
            auto basis=tr::firstperson::RotationMatrix(tr::firstperson::AxisRotation({0,1,0},yaw));
            for(auto& row:basis.r) for(auto& value:row) value*=size;
            const Frame chest{basis,{10,100.f-frame,30}};
            const Frame head{tr::firstperson::IdentityBasis(),{-30,-200-float(frame*3),-100}};
            for(int i=0;i<count;++i) WriteRows(mapping[i]==14?head:chest,palette+i*12);
            std::memcpy(before,palette,sizeof(before));
            Check(tr::firstperson::StabilizeHiddenNeck(palette,count,mapping,visible),"hidden collar correction handles mapped body palettes");
            const tr::motiongun::Vec vertex{12,-20,17};
            for(int i=0;i<count;++i) {
                if(mapping[i]!=14) Check(!std::memcmp(palette+i*12,before+i*12,12*sizeof(float)),"neck correction preserves every visible bone");
                else for(float weight:{.01f,.25f,.49f}) {
                    const auto mixed=Add(Scale(Transform(ReadRows(palette+i*12),vertex),weight),Scale(Transform(chest,vertex),1-weight));
                    Check(tr::firstperson::Length(Sub(mixed,Transform(chest,vertex)))<.001f,"mixed collar vertices cannot spike toward the independently moving hidden head");
                }
            }
            float repeat[33*12];std::memcpy(repeat,palette,sizeof(palette));
            tr::firstperson::StabilizeHiddenNeck(palette,count,mapping,visible);
            Check(!std::memcmp(repeat,palette,sizeof(palette)),"hidden collar correction is idempotent across stereo draws");
            for(auto mask:{tr::firstperson::SkinJointMask(0x7fff,mapping,count),tr::firstperson::SkinJointMask(0x3f00,mapping,count)}) {
                std::memcpy(palette,before,sizeof(palette));
                Check(!tr::firstperson::StabilizeHiddenNeck(palette,count,mapping,mask) && !std::memcmp(palette,before,sizeof(palette)),
                      "visible heads and arms-only attachments keep their original palettes");
            }
        }
    }
}

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

void TestControllerBodyAnchor() {
    TestPitchedGlanceTracking();
    TestExtendedArmGlances();
    TestCapturedBodyAnchorTransitions();
    for(float drop:{0.f,.1f,.3f,.6f}) for(float radius:{.45f,.7f,.95f}) {
        Check(tr::firstperson::PlantedPostureDrop(drop,0,radius)==drop,
            "an actual vertical crouch retains its complete posture drop");
        const float lean=radius*.6f; // A 3-4-5 triangle lowers the neck by one fifth of its radius.
        Check(std::fabs(tr::firstperson::PlantedPostureDrop(drop+radius*.2f,lean,radius)-drop)<.000001f,
            "a real crouch combined with a planted side bend retains only the true crouch component");
    }

    using namespace tr::firstperson;
    // The facing blend reduces the false yaw caused by changing one arm's
    // reach, and stays well defined across angular wrap and close hand spacing.
    for(float base:{0.f,1.5f,3.1f,-3.1f}) for(float side:{-1.f,1.f}) {
        const Vec eye{0,1.7f,0};
        const Vec left=YawVector(Vec{-.2f,1,.3f},base),right=YawVector(Vec{.2f,1,.3f},base);
        BodyRelativeFacing estimate;estimate.Capture(eye,base,left,right);
        const Vec movedLeft=side<0 ? Add(left,YawVector(Vec{0,0,.15f},base)) : left;
        const Vec movedRight=side>0 ? Add(right,YawVector(Vec{0,0,.15f},base)) : right;
        const Vec pair=Sub(movedRight,movedLeft),original=Sub(right,left);
        const float line=BodyAngle(base+std::atan2(pair.x,pair.z)-std::atan2(original.x,original.z));
        const float facing=estimate.Estimate(eye,base,line,movedLeft,movedRight);
        Check(std::fabs(BodyAngle(facing-base))<std::fabs(BodyAngle(line-base))*.6f,
            "body-relative hand votes reduce the facing error caused by asymmetric arm reach");
        const float close=estimate.Estimate(eye,base,base,YawVector(Vec{-.01f,1,.3f},base),YawVector(Vec{.01f,1,.3f},base));
        Check(std::isfinite(close) && std::fabs(BodyAngle(close-base))<.00001f,
            "body-relative facing is stable when the hands approach one another at any heading");
    }
    {
        BodyRelativeFacing facing;const Vec eye{0,1.7f,0};
        facing.Capture(eye,0,Vec{-.2f,1,.3f},Vec{.2f,1,.3f});
        const Vec pivot{0,1.7f,-.15f};
        const auto left=Add(pivot,YawVector(Vec{-.2f,-.7f,.45f},3.12f));
        const auto right=Add(pivot,YawVector(Vec{.2f,-.7f,.45f},-3.12f));
        Check(std::fabs(BodyAngle(facing.Estimate(eye,0,3.14159265f,left,right)-3.14159265f))<.03f,
            "opposite-facing hand votes straddling angle wrap cannot cancel into forward facing");
    }
    // Rendering follows elapsed time rather than controller-checkpoint cadence.
    for(int hz:{72,90,120}) {
        BodyFollower body;body.Update(1,0,0,0,0,{},{},false,BodyFollowSettings());
        for(int i=1;i<=hz;++i) {
            body.Update(i+1,double(i)/hz,0,0,0,{},{},false,BodyFollowSettings(),{},false,true,0,true,true,.08f,true);
            Check(body.yaw==0,"small accepted facing errors keep the lower-body geometric lock planted");
        }
        for(int i=1;i<=hz*3;++i) {
            const float goal=.2f+i*.6f/hz;
            body.Update(hz+i+1,1.+double(i)/hz,0,0,0,{},{},false,BodyFollowSettings(),{},false,true,0,true,true,goal,true);
            if(i>hz/2) Check(std::fabs(BodyAngle(body.yaw-goal))<.03f,
                "continuous physical facing remains responsive at different headset refresh rates");
        }
    }
    ControllerBodyAnchor a;
    const Vec left{-.2f,1,.3f},right{.2f,1,.3f},head{0,1.7f,0};
    a.Update(1,1,head,left,right,0,0,true,false);
    a.Update(2,1.15,head,left,right,0,0,true,false);
    Check(a.held,"stationary controller positions arm body hold after settling");
    for(int frame=3;frame<100;++frame) {
        const Vec eye{.12f*std::sin(frame*.1f),1.7f,.10f*std::cos(frame*.1f)};
        a.Update(frame,1.15+(frame-2)/90.,eye,left,right,0,0,true,false);
        Check(a.held && Length(Flat(Sub(Sub(eye,a.offset),head)))<.00001f,
              "stationary controllers hold body anchor while HMD moves in every horizontal direction");
    }
    {
        ControllerBodyAnchor release;
        release.Update(1,0,head,left,right,0,0,true,false);
        release.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec lean{.30f,0,0},step{.03f,0,0};
        release.Update(3,.21,Add(head,lean),left,right,0,0,true,false);
        for(int i=1;i<=20;++i) {
            const auto delta=Scale(step,i/20.f);
            release.Update(i+3,.21+i/90.,Add(head,delta),Add(left,delta),Add(right,delta),0,0,true,false);
        }
        BodyFollower facing;
        facing.Update(1,0,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        facing.Update(2,.02,0,1.5f,0,step,{},false,tr::firstperson::BodyFollowSettings(),{},false,true,0,false,true);
        Check(std::fabs(facing.yaw)<.00001f,"translation release cannot snap the held body toward a head-only glance");
        facing.Update(3,.03,0,1.6f,0,step,{},false,tr::firstperson::BodyFollowSettings(),{},false,true,.1f,false,true);
        Check(std::fabs(facing.yaw-.1f)<.00001f,"controller-owned facing still accepts corroborated physical turns");
        const auto footprint=Sub(Add(head,step),release.offset);
        Check(Length(Flat(Sub(footprint,Add(head,step))))<.004f,
              "physical release carries only controller movement, never accumulated head lean");
        for(int frame=24;frame<40;++frame) {
            const auto eye=Add(head,{.15f+(frame-23)*.01f,0,0});
            release.Update(frame,.44+(frame-23)/90.,eye,Add(left,step),Add(right,step),0,0,true,false);
            Check(Length(Flat(Sub(Sub(eye,release.offset),footprint)))<.00001f,
                  "head-only motion stays excluded during physical-release settling");
        }
    }
    {
        // A large one-hand gesture must not drag the torso when the other hand
        // drifts only a few centimetres in the same direction (session repro).
        ControllerBodyAnchor gesture;
        gesture.Update(1,0,head,left,right,0,0,true,false);
        gesture.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec lstep{.40f,0,0},rstep{.03f,0,0},eyeStep{.215f,0,0};
        gesture.Update(3,.21,Add(head,eyeStep),Add(left,lstep),Add(right,rstep),0,0,true,false);
        Check(gesture.held && Length(Flat(Sub(Sub(Add(head,eyeStep),gesture.offset),head)))<.00001f,
              "asymmetric controller drift cannot be averaged into roomscale body travel");
        ControllerBodyAnchor glance;
        glance.Update(1,0,head,left,right,0,0,true,false);
        glance.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec drift{.03f,0,0};
        glance.Update(3,.21,Add(head,drift),Add(left,drift),Add(right,drift),0,0,true,false,1.f);
        Check(glance.held,"small common hand drift during a sideways head glance cannot release the body");
        ControllerBodyAnchor parallax;
        parallax.Update(1,0,head,left,right,0,0,true,false);
        parallax.Update(2,.2,head,left,right,0,0,true,false);
        parallax.Update(3,.21,head,Add(left,drift),Add(right,drift),0,0,true,false,.07f);
        Check(parallax.held,"parallel controller drift is not a physical turn around the head");
        ControllerBodyAnchor stale;
        stale.Update(1,0,head,left,right,0,0,true,false);
        stale.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec gestureShift{.1f,0,0};
        stale.Update(3,.21,head,Add(left,gestureShift),Add(right,gestureShift),0,0,true,false);
        stale.Update(4,.5,head,Add(left,gestureShift),Add(right,gestureShift),0,0,true,false);
        stale.Update(5,.51,Add(head,gestureShift),Add(left,gestureShift),Add(right,gestureShift),0,0,true,false);
        Check(stale.held && Length(Flat(Sub(Sub(Add(head,gestureShift),stale.offset),head)))<.00001f,
              "head movement cannot revive an old controller gesture as a physical step");
        PhysicalBodyTurn signal;
        signal.Observe(1,0,0,Sub(left,head),Sub(right,head),true);
        Check(signal.Observe(2,.01,.07f,Sub(Add(left,drift),head),Sub(Add(right,drift),head),true)==0,
              "head-relative hand parallax cannot accumulate a latent body turn");
    }
    {
        ControllerBodyAnchor resumed;
        resumed.Update(1,0,head,left,right,0,0,true,false);
        resumed.Update(2,.2,head,left,right,0,0,true,false);
        resumed.Update(3,.5,head,left,right,0,0,true,false,1.2f);
        for(int i=1;i<=90;++i) {
            const tr::motiongun::Vec shift{.001f*i,0,0};
            resumed.Update(i+3,.5+i/90.,Add(head,shift),Add(left,shift),Add(right,shift),0,0,true,false,1.2f);
        }
        Check(!resumed.held && resumed.head.x>.06f,
              "slow real walking remains available after a sideways head glance settles");
    }
    ControllerBodyAnchor noisy;noisy.Update(1,0,head,left,right,0,0,true,false);
    noisy.Update(2,.3,head,left,right,0,0,true,false);
    for(int i=3;i<100;++i) {
        const tr::motiongun::Vec eye{.12f*std::sin(i*.1f),1.7f,.1f*std::cos(i*.1f)};
        const tr::motiongun::Vec jitter{.0065f*(i%2 ? 1.f : -1.f),.002f,0}; // 13 mm between samples, seen in the session log.
        noisy.Update(i,.3+(i-2)/90.,eye,Add(left,jitter),Sub(right,jitter),0,0,true,false);
        Check(noisy.held && Length(Flat(Sub(Sub(eye,noisy.offset),head)))<.00001f,
              "recorded controller-scale jitter cannot release the body to head translation");
    }
    a.Update(100,2.4,head,left,right,0,0,true,false); // Settle the glance before a real step.
    for(int i=1;i<=20;++i) {
        const Vec step{.002f*i,0,0};
        a.Update(100+i,2.4+i/90.,Add(head,step),Add(left,step),Add(right,step),0,0,true,false);
    }
    Check(!a.held,"coordinated physical head and controller translation releases the hold");
    a.Update(121,2.85,Add(head,{.04f,0,0}),Add(left,{.04f,0,0}),Add(right,{.04f,0,0}),0,0,true,false);
    Check(a.held,"controllers can settle again after motion");
    a.Update(122,2.86,head,left,right,.4f,0,true,false);
    Check(Length(a.offset)==0 && a.reason==4,"stick turning discards the physical hold without stale correction");
    a.Update(123,3.1,head,left,right,.4f,0,true,false);
    a.Update(124,3.11,head,left,right,.4f,0,true,true);
    Check(!a.held,"stick travel remains available with physically stationary controllers");
    ControllerBodyAnchor turning;BodyFollower follower;PhysicalBodyTurn signal;
    turning.Update(1,0,head,left,right,0,0,true,false);
    turning.Update(2,.3,head,left,right,0,0,true,false);
    signal.Observe(2,.3,0,Sub(left,head),Sub(right,head),true);
    follower.Update(2,.3,0,0,0,{}, {},false,BodyFollowSettings());
    bool released=false;
    for(int i=1;i<=100;++i) {
        const float angle=i*.005f;const double now=.3+i/90.;
        const auto l=Add(head,YawVector(Sub(left,head),angle)),r=Add(head,YawVector(Sub(right,head),angle));
        turning.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
        const auto turn=signal.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
        follower.Update(i+2,now,0,angle,0,{}, {},false,BodyFollowSettings(),{},false,true,turning.turnOutput,turning.held,true,turning.heading.active ? turning.heading.target : NAN,turning.rotating);
        released=released || turning.rotating;
        Check(std::fabs(BodyAngle(follower.yaw-angle))<.3f,
              "whole-body turns clear the conservative planted-stance threshold and follow controller heading");
    }
    Check(released,"deliberate physical turning releases only the angular hold");
    {
        ControllerBodyAnchor jitter;PhysicalBodyTurn signal;BodyFollower follower;
        jitter.Update(1,0,head,left,right,0,0,true,false);
        jitter.Update(2,.2,head,left,right,0,0,true,false);
        signal.Observe(2,.2,0,Sub(left,head),Sub(right,head),true);
        follower.Update(2,.2,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        for(int i=1;i<=900;++i) {
            const double now=.2+i/90.;const float glance=1.5f*std::sin(i*.03f);
            const auto eye=Add(head,{.12f*std::sin(i*.027f),0,.08f*std::sin(i*.019f)});
            const auto l=Add(left,{.0065f*std::sin(i*1.7f),0,.0065f*std::cos(i*1.3f)});
            const auto r=Add(right,{.0065f*std::cos(i*1.1f),0,.0065f*std::sin(i*1.9f)});
            jitter.Update(i+2,now,eye,l,r,0,follower.yaw,true,false,glance);
            const float turn=signal.Observe(i+2,now,glance,Sub(l,eye),Sub(r,eye),true);
            follower.Update(i+2,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,jitter.turnOutput,jitter.held,true,jitter.heading.active ? jitter.heading.target : NAN,jitter.rotating);
            Check(jitter.held && std::fabs(follower.yaw)<.00001f,
                  "controller jitter in both horizontal axes cannot release head-only glance rotation");
        }
    }
    // Evaluate the complete angular detector + body hold, including realistic
    // millimetre hand variation. Missed samples must not erase part of a turn.
    for(float speed:{.002f,.008f,.02f}) for(float sign:{-1.f,1.f}) for(int lag:{0,3}) {
        ControllerBodyAnchor hold;PhysicalBodyTurn signal;BodyFollower follower;
        hold.Update(1,0,head,left,right,0,0,true,false);
        hold.Update(2,.2,head,left,right,0,0,true,false);
        signal.Observe(2,.2,0,Sub(left,head),Sub(right,head),true);
        follower.Update(2,.2,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        tr::motiongun::Vec l{},r{};
        for(int i=1;i<=360;++i) {
            const float angle=sign*speed*i;const double now=.2+i/90.;
            l=Add(head,YawVector(Add(Sub(left,head),{.003f*std::sin(i*1.7f),0,.003f*std::cos(i*1.3f)}),sign*speed*std::max(0,i-lag)));
            r=Add(head,YawVector(Add(Sub(right,head),{.003f*std::cos(i*1.1f),0,.003f*std::sin(i*1.9f)}),sign*speed*std::max(0,i-lag-(lag ? 2 : 0))));
            hold.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
            const float turn=signal.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
            follower.Update(i+2,now,0,angle,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,hold.turnOutput,hold.held,true,hold.heading.active ? hold.heading.target : NAN,hold.rotating);
            Check(std::fabs(BodyAngle(follower.yaw-angle))<(std::fabs(angle)<.4f ? .4f : lag ? .35f : .12f),
                  "noisy real roomscale turns retain their rotation through the stationary-controller hold");
        }
        const float final=sign*speed*360,bodyBefore=follower.yaw;
        const Vec pair=Sub(r,l),initialPair=Sub(right,left);
        const float controllerYaw=BodyAngle(std::atan2(pair.x,pair.z)-std::atan2(initialPair.x,initialPair.z));
        Check(std::fabs(BodyAngle(bodyBefore-controllerYaw))<.09f,"roomscale turn ends within the controller noise deadband without cumulative yaw loss");
        auto fixedHold=hold;auto fixedFollower=follower;
        for(int i=1;i<=90;++i) {
            const double now=4.2+i/90.;const float glance=final+i*.015f;
            hold.Update(362+i,now,head,l,r,0,follower.yaw,true,false,glance);
            const float turn=signal.Observe(362+i,now,glance,Sub(l,head),Sub(r,head),true);
            follower.Update(362+i,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,hold.turnOutput,hold.held,true,hold.heading.active ? hold.heading.target : NAN,hold.rotating);
            fixedHold.Update(362+i,now,head,l,r,0,fixedFollower.yaw,true,false,final);
            fixedFollower.Update(362+i,now,0,final,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,
                fixedHold.turnOutput,fixedHold.held,true,fixedHold.heading.active ? fixedHold.heading.target : NAN,fixedHold.rotating);
            Check(std::fabs(BodyAngle(follower.yaw-fixedFollower.yaw))<.00001f,
                  "a head glance cannot add rotation beyond completing the already accepted controller turn");
        }
    }
    // Real arms need not remain a rigid triangle while the player turns.
    // Small elbow movements can defeat the strict incremental detector for
    // an entire half turn; the mesh must still recover its overall facing.
    for(float sign:{-1.f,1.f}) {
        ControllerBodyAnchor anchor;BodyFollower follower;PhysicalBodyTurn fine;
        anchor.Update(1,0,head,left,right,0,0,true,false);
        anchor.Update(2,.2,head,left,right,0,0,true,false);
        fine.Observe(2,.2,0,Sub(left,head),Sub(right,head),true);
        follower.Update(2,.2,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        for(int i=1;i<=400;++i) {
            const double now=.2+i/90.;const float angle=sign*std::min(i,360)*.01f;
            const float flex=i<=360 ? 1.f : std::max(0.f,1-(i-360)/15.f);
            const auto l=Add(head,YawVector(Add(Sub(left,head),{0,0,flex*.07f*std::sin(i*.31f)}),angle));
            const auto r=Add(head,YawVector(Add(Sub(right,head),{0,0,flex*.07f*std::cos(i*.27f)}),angle));
            anchor.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
            const float turn=fine.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
            follower.Update(i+2,now,0,angle,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,anchor.turnOutput,anchor.held,true,anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
        }
        Check(std::fabs(BodyAngle(follower.yaw-sign*3.6f))<.15f,
              "overall roomscale heading cannot remain facing backward when the arms move naturally during a turn");
        const float stopped=follower.yaw;
        const auto stoppedLeft=Add(head,YawVector(Sub(left,head),sign*3.6f));
        const auto stoppedRight=Add(head,YawVector(Sub(right,head),sign*3.6f));
        for(int i=1;i<=360;++i) {
            const double now=4.7+i/90.;const float glance=sign*3.6f+1.5f*std::sin(i*.03f);
            const auto eye=Add(head,{.12f*std::sin(i*.027f),0,.08f*std::sin(i*.019f)});
            const auto l=Add(stoppedLeft,{.0065f*std::sin(i*1.7f),0,.0065f*std::cos(i*1.3f)});
            const auto r=Add(stoppedRight,{.0065f*std::cos(i*1.1f),0,.0065f*std::sin(i*1.9f)});
            anchor.Update(i+403,now,eye,l,r,0,follower.yaw,true,false,glance);
            const float turn=fine.Observe(i+403,now,glance,Sub(l,eye),Sub(r,eye),true);
            follower.Update(i+403,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,anchor.turnOutput,anchor.held,true,
                anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
            Check(std::fabs(BodyAngle(follower.yaw-stopped))<.02f,
                  "absolute heading recovery cannot chase a head glance after the body turn ends");
        }
    }
    {
        ControllerBodyAnchor sway;
        const tr::motiongun::Vec neck{0,0,.12f};
        sway.Update(1,0,head,left,right,0,0,true,false);
        sway.Update(2,.2,head,left,right,0,0,true,false);
        for(int i=1;i<=720;++i) {
            const float angle=.8f*std::sin(i*.02f);const double now=.2+i/90.;
            const auto eye=Add(head,Sub(YawVector(neck,angle),neck));
            const auto l=Add(Sub(head,neck),YawVector(Add(Sub(left,head),neck),angle));
            const auto r=Add(Sub(head,neck),YawVector(Add(Sub(right,head),neck),angle));
            sway.Update(i+2,now,eye,l,r,0,angle,true,false,angle);
            Check(sway.held && Length(Flat(Sub(sway.head,eye)))<.08f,
                  "whole-body rotation carries the held anchor around the neck without cumulative orbit error");
            if(sway.heading.sampleAccepted) Check(Length(Flat(Sub(sway.head,eye)))<.015f,
                  "filtered rigid controller motion carries the body anchor along the head arc within 15 mm");
        }
    }
    // Compare identical controller streams with and without head movement,
    // immediately after turns stop. A settled-idle-only test missed both stale
    // movement checkpoints and the incremental/absolute double application.
    struct TurnRig {
        ControllerBodyAnchor anchor;BodyFollower body;double rate=90;
        void Step(int frame,Vec eye,Vec l,Vec r,float yaw) {
            const double now=frame/rate;
            anchor.Update(frame,now,eye,l,r,0,body.yaw,true,false,yaw);
            body.Update(frame,now,0,anchor.yaw,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,
                anchor.turnOutput,anchor.held,true,anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
        }
    };
    // Arm reach is not root travel. Reproduce the reported foot-sized jump
    // when a turn resumes with a different hand pose, including curved walking
    // where one controller nearly coincides with the instantaneous pivot.
    for(int mode=0;mode<4;++mode) for(float reach:{-.22f,.30f})
        for(float sign:{-1.f,1.f}) for(int direction=0;direction<4;++direction) {
        TurnRig rig;Vec previousEye{},previousBody{},eye{};
        for(int i=0;i<600;++i) {
            float angle=0,extension=0,walk=0;
            if(mode==0) {extension=reach*std::clamp((i-90)/60.f,0.f,1.f);angle=std::max(0,i-150)*.008f;}
            if(mode==1) {
                angle=std::min(std::max(0,i-40),130)*.008f+std::max(0,i-260)*.008f;
                extension=reach*std::clamp((i-180)/60.f,0.f,1.f);
            }
            if(mode==2) {
                angle=std::max(0,i-40)*.008f;extension=reach*std::clamp((i-150)/80.f,0.f,1.f);
                walk=std::clamp((i-250)/100.f,0.f,1.f)*.35f;
            }
            if(mode==3) {
                walk=std::clamp((i-40)/100.f,0.f,1.f)*.35f;
                extension=reach*std::clamp((i-150)/60.f,0.f,1.f);angle=std::max(0,i-220)*.008f;
            }
            angle*=sign;
            const Vec pivot{0,1.7f,-.12f},shift=YawVector(Vec{0,0,walk},direction*1.5707963f);
            auto pose=[&](Vec v) {return Add(Add(pivot,YawVector(Sub(v,pivot),angle)),shift);};
            eye=pose({0,1.7f,0});
            rig.Step(i+1,eye,pose({-.2f,1,.35f+extension}),pose({.2f,1,.35f+extension}),angle);
            if(i>0) {
                Check(Length(Flat(Sub(rig.anchor.head,previousBody)))-Length(Flat(Sub(eye,previousEye)))<.04f,
                    "changed arm reach cannot replay a stale hand offset as a mesh jump");
                Check(Length(Flat(Sub(rig.anchor.head,eye)))<.05f,
                    "changing hand pose during turns and steps cannot accumulate body-anchor drift");
            }
            previousEye=eye;previousBody=rig.anchor.head;
        }
        Check(Length(Flat(Sub(rig.anchor.head,eye)))<.02f,
            "body stays centered after changing arm reach and resuming a physical turn");
    }
    // Translational evidence during an articulated turn must not consume
    // angular progress that the placement gate has not applied yet.
    for(float radius:{0.f,.06f,.12f,.15f}) for(float sign:{-1.f,1.f}) {
        TurnRig rig;Vec eye{};
        for(int i=0;i<=400;++i) {
            const float angle=sign*std::min(i,360)*.01f;
            const float fade=i<=360 ? 1.f : std::max(0.f,1-(i-360)/15.f);
            eye={radius*std::sin(angle),1.7f,radius*(std::cos(angle)-1)};
            const Vec l=Add(eye,YawVector(Vec{-.2f,-.3f,.35f+fade*.07f*std::sin(i*.31f)},angle));
            const Vec r=Add(eye,YawVector(Vec{ .2f,-.3f,.35f+fade*.07f*std::sin(i*.27f)},angle));
            rig.Step(i+1,eye,l,r,angle);
            Check(Length(Flat(Sub(rig.anchor.head,eye)))<.09f,
                "articulated turns around different physical pivots cannot accumulate anchor drift");
        }
        // The 25 mm position checkpoint can leave a small unconsumed arc.
        Check(Length(Flat(Sub(rig.anchor.head,eye)))<.035f,
            "translation fallback preserves pending rotation through arm articulation");
    }
    // A smooth millimetre hand drift can cross an OLD placement checkpoint
    // just after a turn stops. It must not spend the newer headset glance.
    for(double hz:{60.,90.}) for(float tempo:{.1f,1.f}) for(float noise:{.002f,.0065f}) for(float glance:{.03f,.15f,.30f})
        for(float speed:{.002f,.008f,.02f}) for(int stop=30;stop<500;stop+=7) {
        TurnRig rig;rig.rate=hz;Vec eye{},l{},r{};float angle=0;
        for(int i=1;i<=stop;++i) {
            angle=i*speed;const Vec neck{0,1.7f,-.15f};
            eye=Add(neck,YawVector(Vec{0,0,.15f},angle));
            l=Add(neck,YawVector(Vec{-.2f,-.7f,.5f},angle));
            r=Add(neck,YawVector(Vec{ .2f,-.7f,.5f},angle));
            rig.Step(i,eye,l,r,angle);
        }
        const Vec stopped=rig.anchor.head;
        for(int i=1;i<=180;++i) {
            rig.Step(stop+i,Add(eye,Vec{glance*std::sin(i*.08f*tempo),0,glance*.3f*std::sin(i*.04f*tempo)}),
                Add(l,Vec{noise*std::sin(i*.1f),0,noise*(tempo<1 ? std::sin(i*.07f) : std::cos(i*.07f))}),
                Add(r,Vec{noise*(tempo<1 ? std::sin(i*.06f) : std::cos(i*.06f)),0,noise*std::sin(i*.09f)}),angle+.7f*std::sin(i*.1f*tempo));
            Check(Length(Flat(Sub(rig.anchor.head,stopped)))<.00001f,
                "smooth tracked hand drift after a turn cannot spend a newer holster-side head glance");
        }
    }
    // A glance immediately after stopping must not spend the last turn/step's
    // unconsumed controller movement. Include small positional tracking noise.
    for(float speed:{.002f,.008f,.02f}) for(float step:{0.f,.002f})
        for(float noise:{0.f,.002f}) for(int stop=30;stop<500;stop+=7) {
        TurnRig rig;Vec eye{},l{},r{};float angle=0;
        for(int i=1;i<=stop;++i) {
            angle=i*speed;
            const Vec neck{step*i,1.7f,-.15f};
            eye=Add(neck,YawVector(Vec{0,0,.15f},angle));
            l=Add(neck,YawVector(Vec{-.2f,-.7f,.5f},angle));
            r=Add(neck,YawVector(Vec{ .2f,-.7f,.5f},angle));
            rig.Step(i,eye,l,r,angle);
        }
        const Vec stopped=rig.anchor.head;Vec settled{};
        for(int i=1;i<=90;++i) {
            const Vec glance=Add(eye,Vec{.03f*std::sin(i*.12f),0,.03f*std::sin(i*.05f)});
            rig.Step(stop+i,glance,
                Add(l,Vec{noise*std::sin(i*.8f),0,noise*std::cos(i*.7f)}),
                Add(r,Vec{noise*std::cos(i*.6f),0,noise*std::sin(i*.9f)}),angle+.3f*std::sin(i*.1f));
            Check(Length(Flat(Sub(rig.anchor.head,stopped)))<(noise==0 ? .00001f : .009f),
                "head-only glance cannot reuse stale turn or walking evidence after hands stop");
            if(i==12) settled=rig.anchor.head;
            if(i>12) Check(Length(Flat(Sub(rig.anchor.head,settled)))<.00001f,
                "settled controller noise cannot revive pending motion during sideways glances");
        }
    }
    // Straight and curved walking must survive natural opposing arm swing.
    // Rotation samples must not consume the separate movement observation.
    for(float speed:{0.f,.002f,.008f}) for(float swing:{0.f,.01f,.025f}) for(int direction=0;direction<4;++direction) {
        TurnRig rig;int permitted=0;float previousStep=0;
        for(int i=0;i<480;++i) {
            const float angle=i*speed;
            const Vec shift=YawVector(Vec{0,0,i*.002f},direction*1.5707963f),neck{0,1.7f,-.15f};
            const Vec eye=Add(Add(neck,YawVector(Vec{0,0,.15f},angle)),shift);
            const Vec l=Add(Add(neck,YawVector(Vec{-.2f,-.7f,.45f+swing*std::sin(i*.13f)},angle)),shift);
            const Vec r=Add(Add(neck,YawVector(Vec{ .2f,-.7f,.45f-swing*std::sin(i*.13f)},angle)),shift);
            const float before=rig.body.yaw;
            rig.Step(i+1,eye,l,r,angle);
            const float step=BodyAngle(rig.body.yaw-before);
            if(i>120 && rig.anchor.translating) ++permitted;
            if(i>220) {
                Check(Length(Flat(Sub(rig.anchor.head,eye)))<.055f,
                    "combined turn and translation cannot double-count travel or leave the mesh far from the player");
                if(speed==.008f && swing==.01f) Check(std::fabs(step-previousStep)<.011f,
                    "natural small arm swings cannot turn sustained body rotation into stop-start angular bursts");
            }
            previousStep=step;
        }
        Check(permitted>340,"forward backward left and right walking remains responsive during turns and modest arm swing");
    }
    // Reproduce the in-session stall after hand spacing/reach changes, plus
    // the stop/start cadence of an otherwise perfectly steady physical turn.
    for(float speed:{.002f,.008f,.02f}) for(float sign:{-1.f,1.f}) for(int scenario=0;scenario<4;++scenario) {
        TurnRig rig;
        for(int i=0;i<=720;++i) {
            const float angle=sign*i*speed;
            const float span=scenario==1 || (scenario==2 && i>100) ? .075f : .4f;
            const float reach=.3f+(scenario==3 && i>100 ? .16f : 0.f);
            const auto l=YawVector(Vec{-span*.5f,1,reach},angle);
            const auto r=YawVector(Vec{ span*.5f,1,reach},angle);
            const float before=rig.body.yaw;
            rig.Step(i+1,head,l,r,angle);
            if(i>400) {
                Check(std::fabs(BodyAngle(rig.body.yaw-angle))<.12f,
                    "physical turn reacquires changed arm geometry and close hands without becoming stuck facing backward");
                const float step=sign*BodyAngle(rig.body.yaw-before);
                Check(step>speed*.65f && step<speed*1.35f,
                    "steady physical turns render continuously instead of stopping between controller checkpoints");
            }
        }
    }
    for(float span:{.075f,.12f}) {
        TurnRig rig;const Vec l{-span*.5f,1,.3f},r{span*.5f,1,.3f};
        rig.Step(1,head,l,r,0);
        for(int i=1;i<=720;++i) {
            const float glance=1.5f*std::sin(i*.03f);
            const auto eye=Add(head,{.12f*std::sin(i*.027f),0,.08f*std::sin(i*.019f)});
            rig.Step(i+1,eye,Add(l,{.0065f*std::sin(i*1.7f),0,.0065f*std::cos(i*1.3f)}),
                Add(r,{.0065f*std::cos(i*1.1f),0,.0065f*std::sin(i*1.9f)}),glance);
            Check(std::fabs(rig.body.yaw)<.00001f && Length(Flat(Sub(rig.anchor.head,head)))<.00001f,
                "closely spaced controller noise cannot authorize head-only mesh movement");
        }
    }
    for(float sign:{-1.f,1.f}) for(int stop=20;stop<=400;stop+=10) for(float noise:{0.f,.002f,.0065f}) {
        TurnRig glance;
        Vec l=left,r=right;
        for(int frame=1;frame<=stop;++frame) {
            const float yaw=sign*std::max(0,frame-20)*.01f;
            l=Add(head,YawVector(Add(Sub(left,head),{0,0,.07f*std::sin(frame*.31f)}),yaw));
            r=Add(head,YawVector(Add(Sub(right,head),{0,0,.07f*std::cos(frame*.27f)}),yaw));
            glance.Step(frame,head,l,r,yaw);
        }
        auto still=glance;const float start=sign*(stop-20)*.01f;
        float settled=0,movedGlance=0,movedStill=0,previousGlance=glance.body.yaw,previousStill=still.body.yaw;
        for(int i=1;i<=360;++i) {
            const auto eye=Add(head,{.03f*std::sin(i*.025f),0,.03f*std::sin(i*.019f)});
            const auto nl=Add(l,{noise*std::sin(i*1.7f),0,noise*std::cos(i*1.3f)});
            const auto nr=Add(r,{noise*std::cos(i*1.1f),0,noise*std::sin(i*1.9f)});
            glance.Step(stop+i,eye,nl,nr,start+.2f*std::sin(i*.035f));
            still.Step(stop+i,head,nl,nr,start);
            movedGlance+=std::fabs(BodyAngle(glance.body.yaw-previousGlance));
            movedStill+=std::fabs(BodyAngle(still.body.yaw-previousStill));
            previousGlance=glance.body.yaw;previousStill=still.body.yaw;
            Check(movedGlance<=movedStill+.00001f,
                "a head glance may replant sooner but cannot add mesh rotation to the same controller stream");
            if(i==90) settled=glance.body.yaw;
            if(i>90) Check(std::fabs(BodyAngle(glance.body.yaw-settled))<.00001f,
                "stationary controller noise cannot keep rotating the body after the accepted turn settles");
        }
    }
    // A glance plus small deliberate hand adjustments is still a planted
    // stance. Validate that separately from zero-motion and tracking-noise cases.
    // Real turns also carry the held anchor around nonzero physical pivots.
    for(float sign:{-1.f,1.f}) for(float radius:{.08f,.12f,.22f}) for(float adjustment:{.06f,.12f,.18f}) {
        TurnRig rig;int frame=0;
        const Vec pivot{-.05f,0,-radius};
        auto rotate=[&](Vec v,float angle) {return Add(pivot,YawVector(Sub(v,pivot),angle));};
        for(int i=0;i<=720;++i) {
            const float angle=sign*i*6.28318530718f/720;
            const auto eye=rotate(head,angle);
            rig.Step(++frame,eye,rotate(left,angle),rotate(right,angle),angle);
            if(i>=90) {
                Check(Length(Flat(Sub(rig.anchor.head,eye)))<.03f,
                    "roomscale quarter and half turns keep the body centered without requiring a full revolution");
                Check(std::fabs(BodyAngle(rig.body.yaw-angle))<.12f,
                    "sustained physical turns retain facing after the planted-stance decision");
            }
        }
        for(int i=0;i<90;++i) rig.Step(++frame,head,left,right,0);
        const float facing=rig.body.yaw;const Vec planted=rig.anchor.head;
        for(int i=1;i<=720;++i) {
            const float glance=.7f*std::sin(i*.025f),hands=adjustment*std::sin(i*.025f);
            rig.Step(++frame,rotate(head,glance),rotate(left,hands),rotate(right,hands),glance);
            Check(std::fabs(BodyAngle(rig.body.yaw-facing))<.00001f && Length(Flat(Sub(rig.anchor.head,planted)))<.00001f,
                "head glances with centimetres of hand adjustment retain the planted mesh position and facing");
        }
    }
    // The native/IK footprint must follow the accepted moving pivot, not
    // accumulate an orbit around a sequence of different pivot positions.
    for(float sign:{-1.f,1.f}) {
        ControllerBodyAnchor anchor;BodyFollower body;
        const Vec pivot{0,0,-.12f},local{.035f,0,.14f};
        for(int i=0;i<=1080;++i) {
            const float angle=sign*i*6.2831853f/720;const double now=i/90.;
            auto rotate=[&](Vec v) {return Add(pivot,YawVector(Sub(v,pivot),angle));};
            anchor.Update(i+1,now,rotate(head),rotate(left),rotate(right),0,body.yaw,true,false,angle);
            const auto goal=Sub(anchor.head,YawVector(local,body.yaw));
            body.Update(i+1,now,0,anchor.yaw,0,goal,{},false,tr::firstperson::BodyFollowSettings(),anchor.head,false,true,0,anchor.held,true,
                anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
            if(anchor.held) Check(Length(Sub(body.root,Sub(Flat(anchor.head),YawVector(local,body.yaw))))<.00001f,
                "leg footprint and pelvis share the accepted pivot through repeated physical turns");
        }
    }
    for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor walk;walk.Update(1,0,head,left,right,0,0,true,false);
        walk.Update(2,.2,head,left,right,0,0,true,false);
        Check(!walk.translating,"settled tracking does not grant permission for gameplay movement");
        for(int i=1;i<=180;++i) {
            const float glance=.45f*std::sin(i*.04f);
            const auto step=YawVector({0,0,i*.002f},direction*1.5707963f);
            const auto eye=Add(Add(head,step),Sub(YawVector({0,0,.15f},glance),{0,0,.15f}));
            walk.Update(i+2,.2+i/90.,eye,Add(left,step),Add(right,step),0,0,true,false,glance);
            if(i>=15) Check(Length(Sub(Flat(walk.head),step))<.025f && !walk.held,
                "forward backward and sideways steps remain responsive while the head glances independently");
        }
    }
    // Recorded failure pattern: a settled sideways gaze with unequal hand
    // adjustments must not masquerade as a physical step. The short-window
    // head yaw can be zero even though the player is clearly looking aside.
    for(float gaze:{-1.2f,1.2f}) for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor planted;
        planted.Update(1,0,head,left,right,0,0,true,false,gaze);
        planted.Update(2,.2,head,left,right,0,0,true,false,gaze);
        for(int i=1;i<=20;++i) {
            const Vec axis=YawVector(Vec{i/20.f,0,0},direction*1.5707963f);
            planted.Update(i+2,.2+i/90.,Add(head,Scale(axis,.03f)),
                Add(left,Scale(axis,.041f)),Add(right,Scale(axis,.010f)),0,0,true,false,gaze);
            Check(planted.held && Length(Flat(Sub(planted.head,head)))<.00001f,
                "a settled sideways gaze and unequal small hand adjustments leave the inferred feet planted");
        }
    }
    ControllerBodyAnchor slow;slow.Update(1,0,head,left,right,0,0,true,false);
    for(int i=1;i<=90;++i) {
        const tr::motiongun::Vec shift{.002f*i,0,0};
        slow.Update(i+1,i/90.,Add(head,shift),Add(left,shift),Add(right,shift),0,0,true,false);
        if(i>=15) Check(!slow.held,"slow cumulative controller translation is not mistaken for stationary tracking noise");
    }
    a.Update(106,2.82,head,left,right,.4f,0,false,false);
    Check(!a.valid && !a.held && Length(a.offset)==0,"tracking loss cannot retain a stale body anchor");
}

void TestPhysicalBodyTurn() {
    TestControllerBodyAnchor();
    using namespace tr::firstperson;
    const tr::firstperson::Vec left{-.2f,0,.35f},right{.2f,0,.35f},foot{0,0,-.15f};
    tr::firstperson::BodyFollowSettings settings;
    for(float direction:{-1.f,1.f}) for(float glance:{0.f,.4f}) {
        PhysicalBodyTurn signal;BodyFollower physical,analog,mixed;
        physical.Update(1,1,0,0,0,foot,{},false,settings,{},false,true,0,false);
        analog=physical;mixed=physical;
        signal.Observe(1,1,glance,left,right,true);
        float angle=0;
        for(int frame=2;frame<1400;++frame) {
            const float step=(frame<700 ? direction : -direction)*.012f;
            angle+=step;const double now=1+double(frame-1)/90;
            const float observed=signal.Observe(frame,now,BodyAngle(angle+glance),YawVector(left,angle),YawVector(right,angle),true);
            Check(std::fabs(observed-step)<.00001f,"coherent physical turn crosses wrap and reverses without losing heading");
            physical.Update(frame,now,0,BodyAngle(angle+glance),0,YawVector(foot,physical.yaw),{},false,settings,{},false,true,observed,false);
            analog.Update(frame,now,angle,BodyAngle(angle+glance),0,YawVector(foot,analog.yaw),{},false,settings,{},false,true,0,false);
            mixed.Update(frame,now,angle*.5f,BodyAngle(angle*1.5f+glance),0,YawVector(foot,mixed.yaw),{},false,settings,{},false,true,observed,false);
            Check(std::fabs(BodyAngle(physical.yaw-analog.yaw))<.00005f && Length(Sub(physical.root,analog.root))<.00005f,
                  "roomscale and analog turns keep identical body heading and centered footprint");
            Check(std::fabs(BodyAngle(physical.yaw-angle))<.00005f,"physical turn retains the existing head-only glance");
            Check(std::fabs(BodyAngle(mixed.yaw-angle*1.5f))<.0001f,"simultaneous physical and analog turns each apply once");
            const auto root=physical.root;const float yaw=physical.yaw;
            physical.Update(frame,now,0,angle+glance,0,root,{},false,settings,{},false,true,observed,false);
            Check(physical.yaw==yaw && Length(Sub(root,physical.root))==0,"stereo and accessory passes cannot repeat a physical turn");
        }
    }
    for(int mode=0;mode<8;++mode) {
        PhysicalBodyTurn signal;signal.Observe(1,1,0,left,right,true);
        float head=.1f;tr::firstperson::Vec a=YawVector(left,.1f),b=YawVector(right,.1f);double now=1.01;bool tracked=true;
        if(mode==0) {a=left;b=right;} // Head-only glance.
        if(mode==1) head=0; // Hands-only gesture.
        if(mode==2) b=right; // One hand follows the head.
        if(mode==3) tracked=false;
        if(mode==4) now=1.5;
        if(mode==5) {head=1;a=YawVector(left,1);b=YawVector(right,1);}
        if(mode==6) a={0,0,.01f};
        if(mode==7) a=Scale(a,2);
        Check(signal.Observe(2,now,head,a,b,tracked)==0,"glances, gestures and tracking discontinuities do not drag the body");
    }
    PhysicalBodyTurn lost;lost.Observe(1,1,0,left,right,true);
    lost.Observe(2,1.01,0,left,right,false);
    Check(lost.Observe(3,1.02,1,YawVector(left,1),YawVector(right,1),true)==0,"tracking recovery seeds without replaying a turn");
    Check(std::fabs(lost.Observe(4,1.03,1.1f,YawVector(left,1.1f),YawVector(right,1.1f),true)-.1f)<.00001f,"physical following resumes after tracking recovery");
    BodyFollower moving;moving.Update(1,1,0,0,0,{}, {},false,settings);
    moving.Update(2,1.01,0,.1f,.2f,{}, {},true,settings,{},false,true,.1f);
    Check(std::fabs(moving.yaw-.2f)<.00001f,"travel heading remains authoritative while moving");
}

void TestForwardJumpTorso() {
    using namespace tr::firstperson;using Vec=tr::motiongun::Vec;
    const Vec points[15]={{0,-380,0},{-45,-380,0},{-45,-200,50},{-45,0,0},
        {45,-380,0},{45,-200,50},{45,0,0},{0,-428,14},
        {90,-580,0},{125,-460,20},{90,-320,50},{-90,-580,0},{-125,-460,20},{-90,-320,50},{0,-650,0}};
    Frame standing[15]{};for(int j=0;j<15;++j) standing[j]={IdentityBasis(),points[j]};
    int model=0;GroundTorso reference;
    Check(reference.Apply(&model,standing,IdentityBasis(),{0,-682,140},423,0,true),"capture grounded torso for forward-jump collar protection");
    for(float yaw:{0.f,.7f,-2.4f}) for(float pitch:{-.9f,-.3f,-.1f,0.f,.1f,.3f,.7f,.9f}) {
        const Basis turn=RotationMatrix(AxisRotation({0,1,0},yaw));
        Frame pose[15]{},before[15]{};
        for(int j=0;j<15;++j) pose[j]=Multiply({turn,{100,0,200}},standing[j]);
        const auto bow=TorsoClearance::Bend(pose[0].origin,{std::cos(yaw),0,-std::sin(yaw)},pitch);
        for(int j=7;j<15;++j) pose[j]=Multiply(bow,pose[j]);
        std::memcpy(before,pose,sizeof(pose));
        Check(reference.HoldForwardJump(&model,pose,turn,423),"forward-jump upright reference accepts live animation");
        Frame inverse{};Inverse({turn,{}},inverse);
        const auto q=RotationOf(Multiply(inverse.basis,pose[7].basis));
        const float angle=2*std::atan2(Length(q.v),std::fabs(q.w));
        Check(angle<=.0001f,"forward jump cannot bow the exposed collar far away from the upright reference");
        for(int j:{0,1,2,3,4,5,6,14}) Check(!std::memcmp(&pose[j],&before[j],sizeof(Frame)),"collar protection leaves native legs and hidden head unchanged");
        Check(std::fabs(Length(Sub(pose[7].origin,pose[0].origin))-Length(Sub(before[7].origin,before[0].origin)))<.001f,
              "jump torso protection preserves spine length");
        for(int shoulder:{8,11}) Check(std::fabs(Length(Sub(pose[shoulder].origin,pose[7].origin))-Length(Sub(before[shoulder].origin,before[7].origin)))<.001f,
              "forward-jump shoulder sockets follow the same rigid torso correction");
        if(pitch==0) for(int j=7;j<14;++j) Check(Length(Sub(pose[j].origin,before[j].origin))<.001f,
              "an already upright jump torso remains unmodified");
    }
}

void TestUprightTorsoFit() {
    TestHiddenNeckSkin();
    TestPhysicalBodyTurn();
    TestForwardJumpTorso();
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

void TestConfirmedRoomscaleSteps() {
    trackingtest::Clock trackingClock;
    using namespace tr;
    const auto settings=config;const auto savedVR=VR();const auto layout=dll;
    const auto savedAnchor=g_controllerBodyAnchor;const auto savedFollow=g_roomscaleFollower;
    const auto savedTime=g_roomscaleFollowTime;
    const auto savedDrag=g_dragCurrent,savedShown=g_dragShown;
    const auto savedCollision=collisionMode;
    auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);const auto savedPos=pos;
    const auto rva=[](const void* p){return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.getCollisionInfo=rva(reinterpret_cast<void*>(&FakeCollision));dll.getFloor=rva(reinterpret_cast<void*>(&FakeFloor));
    dll.getHeight=rva(reinterpret_cast<void*>(&FakeHeight));dll.itemNewRoom=rva(reinterpret_cast<void*>(&FakeNewRoom));
    config.firstPersonFullBodyIK=config.firstPersonMovementStabilization=true;
    config.firstPersonRoomscaleMove=config.positionalTracking=config.firstPersonHeadTranslation=true;
    config.firstPersonRoomscaleDeadzoneMetres=.02f;config.firstPersonRoomscaleNeckMetres=.15f;
    auto tick=[]() {LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&g_roomscaleFollowTime);
        g_roomscaleFollowTime.QuadPart-=frequency.QuadPart/30;};
    for(int direction=0;direction<4;++direction) {
        Head(0);VR().RecenterFirstPersonHead();ResetControllerBodyAnchor();
        pos={};g_dragCurrent=g_dragShown={};g_roomscaleFollower={};collisionMode=0;collisionCalls=0;
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        for(int hand=0;hand<2;++hand) {
            VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[hand].m[0][3]+=(hand ? .2f : -.2f);
            VR().m_rawControllerPose[hand].m[1][3]-=.3f;
            VR().m_rawControllerPose[hand].m[2][3]-=.3f;
        }
        const auto left=VR().m_rawControllerPose[0],right=VR().m_rawControllerPose[1],eye=VR().m_rawHeadPose;
        ++VR().m_poseSerial;UpdateControllerBodyAnchor();
        g_controllerBodyAnchor.moved=TurnTime()-.2;++VR().m_poseSerial;UpdateControllerBodyAnchor();
        Check(BodyControllersStationary(),"roomscale fixture begins with planted tracked controllers");
        const auto travel=locomotion::Rotate({.03f,0},direction*1.5707963f);
        for(int i=1;i<=20;++i) {
            const float t=i/20.f,angle=.3f*t;Head(angle);
            VR().m_rawHeadPose.m[0][3]=eye.m[0][3]+.15f*std::sin(angle)+travel.x*t;
            VR().m_rawHeadPose.m[2][3]=eye.m[2][3]-.15f*(std::cos(angle)-1)-travel.z*t;
            VR().m_rawControllerPose[0]=left;VR().m_rawControllerPose[1]=right;
            for(auto& controller:VR().m_rawControllerPose) {controller.m[0][3]+=travel.x*t;controller.m[2][3]-=travel.z*t;}
            trackingClock.Advance(1./90);++VR().m_poseSerial;UpdateControllerBodyAnchor();
        }
        ++VR().m_poseSerial;tick();const auto movement=DragBody(itemMemory);
        Check(g_controllerBodyAnchor.translating && locomotion::Length(movement)>.005f && collisionCalls>0,
            "confirmed three-centimetre steps reach native collision movement without a second fifteen-centimetre deadzone");
        Check(movement.x*travel.x+movement.z*travel.z>0,
            "confirmed roomscale movement preserves forward backward left and right direction during a head glance");
        const auto prior=pos;collisionMode=1;
        VR().m_rawHeadPose.m[0][3]+=travel.x;VR().m_rawHeadPose.m[2][3]-=travel.z;
        for(auto& controller:VR().m_rawControllerPose) {controller.m[0][3]+=travel.x;controller.m[2][3]-=travel.z;}
        ++VR().m_poseSerial;tick();const auto blocked=DragBody(itemMemory);
        Check(locomotion::Length(blocked)==0 && pos.x_pos==prior.x_pos && pos.z_pos==prior.z_pos,
            "responsive controller-confirmed steps still respect native wall collision");
    }
    config=settings;VR()=savedVR;dll=layout;pos=savedPos;collisionMode=savedCollision;
    g_controllerBodyAnchor=savedAnchor;g_roomscaleFollower=savedFollow;g_roomscaleFollowTime=savedTime;
    g_dragCurrent=savedDrag;g_dragShown=savedShown;
}

void TestStandingCalibration(uint8_t* objectInfo) {
    TestConfirmedRoomscaleSteps();
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
    {
        const auto savedVR=VR();
        auto setTracking=[&](float yaw) {
            Head(yaw);
            for(int hand=0;hand<2;++hand) {
                const auto offset=firstperson::YawVector({hand ? .2f : -.2f,0,.35f},yaw);
                VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
                VR().m_rawControllerPose[hand].m[0][3]+=offset.x;
                VR().m_rawControllerPose[hand].m[1][3]-=.3f;
                VR().m_rawControllerPose[hand].m[2][3]-=offset.z;
            }
        };
        trackingtest::Clock trackingClock;
        auto advanceTrackingClock=[&]() {trackingClock.Advance(1./90);};
        const float savedHandWeight=config.firstPersonBodyHandYawWeight;
        for(float weight:{0.f,.15f}) {
            config.firstPersonBodyHandYawWeight=weight;
            setTracking(0);ResetBodyFollower();render();
            for(int frame=1;frame<=60;++frame) {
                const float yaw=frame*.005f;setTracking(yaw);
                advanceTrackingClock();
                const auto camera=g_scenePose;
                render();
                Check(std::fabs(firstperson::BodyAngle(g_bodyFollower.yaw-(weight>0 ? yaw : 0.f)))<(weight>0 ? .30f : .0001f),
                      "production mesh follows physical turns and respects disabled controller support");
                Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)),"physical mesh heading never rotates or translates the camera");
            }
        }


        config.firstPersonBodyHandYawWeight=.15f;
        setTracking(0);ResetBodyFollower();render();
        g_controllerBodyAnchor.moved=TurnTime()-.2;render();
        for(int frame=1;frame<=400;++frame) {
            const float angle=std::min(frame,360)*.01f;setTracking(angle);
            const float arcX=.12f*std::sin(angle),arcZ=.12f*(std::cos(angle)-1);
            VR().m_rawHeadPose.m[0][3]+=arcX;VR().m_rawHeadPose.m[2][3]-=arcZ;
            for(auto& controller:VR().m_rawControllerPose) {controller.m[0][3]+=arcX;controller.m[2][3]-=arcZ;}
            const float fade=frame<=360 ? 1.f : std::max(0.f,1-(frame-360)/15.f);
            for(int hand=0;hand<2;++hand) {
                const float flex=fade*.07f*std::sin(frame*(hand ? .27f : .31f));
                VR().m_rawControllerPose[hand].m[0][3]+=std::sin(angle)*flex;
                VR().m_rawControllerPose[hand].m[2][3]-=std::cos(angle)*flex;
            }
            advanceTrackingClock();
            const auto camera=g_scenePose;const auto turnPose=render();
            for(int first:{1,4}) {
                const float reach=firstperson::Length(Sub(turnPose[first+1].origin,turnPose[first].origin))+
                    firstperson::Length(Sub(turnPose[first+2].origin,turnPose[first+1].origin));
                Check(firstperson::Length(Sub(turnPose[first+2].origin,turnPose[first].origin))>reach*.97f,
                    "production standing legs remain extended during roomscale turns instead of folding around the old footprint");
                const auto foot=firstperson::YawVector(Sub(turnPose[first+2].origin,turnPose[0].origin),-g_bodyFollower.yaw);
                Check((first==1 ? -foot.x : foot.x)>LiveWorldUnitsPerMetre()*.03f,
                    "roomscale turning keeps each planted foot on its own side of the pelvis");
            }
            Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)),"overall heading correction never turns the camera");
        }
        Check(std::fabs(firstperson::BodyAngle(g_bodyFollower.yaw-3.6f))<.15f,
              "production solver consumes the absolute roomscale heading correction");
        Check(firstperson::Length(firstperson::Flat(Sub(g_controllerBodyAnchor.head,
            {VR().m_rawHeadPose.m[0][3],VR().m_rawHeadPose.m[1][3],-VR().m_rawHeadPose.m[2][3]})))<.035f,
            "production whole-body turns keep the held mesh anchor centered beneath the tracked eye");
        const auto stoppedHead=VR().m_rawHeadPose;
        const auto stoppedLeft=VR().m_rawControllerPose[0],stoppedRight=VR().m_rawControllerPose[1];
        const float stoppedYaw=g_bodyFollower.yaw;
        for(int frame=1;frame<=180;++frame) {
            Head(3.6f+.2f*std::sin(frame*.035f));
            for(int axis=0;axis<3;++axis) VR().m_rawHeadPose.m[axis][3]=stoppedHead.m[axis][3];
            VR().m_rawHeadPose.m[0][3]+=.03f*std::sin(frame*.025f);
            VR().m_rawHeadPose.m[2][3]+=.03f*std::sin(frame*.019f);
            VR().m_rawControllerPose[0]=stoppedLeft;VR().m_rawControllerPose[1]=stoppedRight;
            for(int hand=0;hand<2;++hand) {
                VR().m_rawControllerPose[hand].m[0][3]+=.0065f*std::sin(frame*(hand ? 1.1f : 1.7f));
                VR().m_rawControllerPose[hand].m[2][3]+=.0065f*std::cos(frame*(hand ? 1.9f : 1.3f));
            }
            advanceTrackingClock();
            if(frame==60) g_bodyFollower.Reset(); // Jump/stance restarts must retain controller-owned facing.
            const auto camera=g_scenePose;render();
            Check(std::fabs(firstperson::BodyAngle(g_bodyFollower.yaw-stoppedYaw))<.00001f,
                "production mesh stays facing the same direction through subtle head movements after roomscale turning");
            Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)),"controller-owned body yaw leaves the view unchanged");
        }
        trackingClock.End();
        for(int holdGunStatus:{0,4}) {
        trackingtest::Clock holdClock;
        *Ptr<int16_t>(dll.lara+2)=int16_t(holdGunStatus);
        setTracking(0);VR().m_firstPersonNeutral[1]=1.7f;VR().m_firstPersonNeutralEyeNeckY=.08f;ResetBodyFollower();render();
        g_controllerBodyAnchor.moved=TurnTime()-.2;
        const auto heldPose=render();
        Check(BodyControllersStationary(),"render fixture holds actual stationary controller positions");
        const auto heldLeft=VR().m_rawControllerPose[0],heldRight=VR().m_rawControllerPose[1];
        const auto sceneBefore=g_scenePose;
        const auto heldHead=VR().m_rawHeadPose;
        for(int i=1;i<=20;++i) {
            Head(1.2f);VR().m_rawHeadPose.m[0][3]=heldHead.m[0][3]+.03f*i/20;
            VR().m_rawHeadPose.m[1][3]=heldHead.m[1][3];VR().m_rawHeadPose.m[2][3]=heldHead.m[2][3];
            VR().m_rawControllerPose[0]=heldLeft;VR().m_rawControllerPose[1]=heldRight;
            VR().m_rawControllerPose[0].m[0][3]+=.041f*i/20;
            VR().m_rawControllerPose[1].m[0][3]+=.010f*i/20;
            holdClock.Advance(1./90);const auto pose=render();
            for(int joint:{0,7}) Check(firstperson::Length(firstperson::Flat(Sub(pose[joint].origin,heldPose[joint].origin)))<.1f,
                "final rendered pelvis and torso remain stationary through a sideways glance and unequal hand drift");
            Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)),"planted inference leaves the camera untouched");
        }
        Head(0);VR().m_rawHeadPose=heldHead;
        VR().m_rawControllerPose[0]=heldLeft;VR().m_rawControllerPose[1]=heldRight;
        ResetControllerBodyAnchor();UpdateControllerBodyAnchor();
        const float hipToNeck=std::clamp(g_groundTorso.hipBelowEye/LiveWorldUnitsPerMetre()-.08f,.45f,.95f);
        for(int frame=1;frame<=360;++frame) {
            const float side=hipToNeck*.8f*std::sin(frame*.025f);
            Head(.7f*std::sin(frame*.025f),-1.f);
            VR().m_rawHeadPose.m[0][3]=heldHead.m[0][3]+side;
            VR().m_rawHeadPose.m[2][3]=heldHead.m[2][3];
            const float neckDrop=std::max(0.f,.08f-(.08f*VR().m_rawHeadPose.m[1][1]-.12f*VR().m_rawHeadPose.m[1][2]));
            VR().m_rawHeadPose.m[1][3]=heldHead.m[1][3]-neckDrop-
                (hipToNeck-std::sqrt(hipToNeck*hipToNeck-side*side));
            VR().m_rawControllerPose[0]=heldLeft;VR().m_rawControllerPose[1]=heldRight;
            holdClock.Advance(1./90);const auto tracked=VR().m_rawHeadPose;const auto pose=render();
            for(int joint:{0,7}) Check(firstperson::Length(Sub(pose[joint].origin,heldPose[joint].origin))<.5f,
                "looking down around either holster holds the rendered pelvis and torso in all three world axes");
            Check(!std::memcmp(&tracked,&VR().m_rawHeadPose,sizeof(tracked)) &&
                !std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)),"posture inference never modifies headset tracking or scene camera");
        }
        Head(0);VR().m_rawHeadPose=heldHead;
        VR().m_rawControllerPose[0]=heldLeft;VR().m_rawControllerPose[1]=heldRight;
        ResetControllerBodyAnchor();UpdateControllerBodyAnchor();
        for(int frame=0;frame<180;++frame) {
            const float yaw=1.5f*std::sin(frame*.4f),dx=.35f*std::sin(frame*.04f),dz=.25f*std::cos(frame*.04f);
            Head(yaw,-1.f);VR().m_rawHeadPose.m[1][3]-=.25f;VR().m_rawHeadPose.m[0][3]+=dx;VR().m_rawHeadPose.m[2][3]-=dz;
            VR().m_rawControllerPose[0]=heldLeft;VR().m_rawControllerPose[1]=heldRight;
            VR().m_rawControllerPose[0].m[0][3]+=(frame%2 ? .0065f : -.0065f);
            VR().m_rawControllerPose[1].m[0][3]-=(frame%2 ? .0065f : -.0065f);
            if(frame==10) g_bodyFollower.Reset(); // Native jump/landing can restart the stance solver.
            g_torsoPose.time=TurnTime()-1./90;
            const auto pose=render();
            Check(BodyControllersStationary(),"head movement does not release fixed controller positions");
            Check(locomotion::Length(DragBody(itemMemory))==0,"stationary controllers prevent head-only roomscale root movement");
            for(int joint:{0,7}) Check(firstperson::Length(firstperson::Flat(Sub(pose[joint].origin,heldPose[joint].origin)))<.1f,
                  "head translation and rotation cannot slide pelvis or chest while controllers remain fixed in space");
            Check(std::fabs(g_bodyFollower.yaw)<.0001f,"stationary controllers freeze physical body yaw");
            for(int hand=0;hand<2;++hand) {
                Frame wrist{};Basis controller{};Check(BuildControllerWrist(hand,wrist,controller),"body hold retains controller targets");
                Check(firstperson::Length(Sub(pose[hand ? 10 : 13].origin,wrist.origin))<.1f,
                      "stationary body retains exact tracked hands");
            }
            Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)),"body hold never alters the camera anchor");
        }
        Head(0);
        ResetControllerBodyAnchor();
        }
        *Ptr<int16_t>(dll.lara+2)=0;
        config.firstPersonBodyHandYawWeight=savedHandWeight;
        VR()=savedVR;
    }
    {
        const auto savedBits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
        *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x3fff;
        for(int frame=0;frame<=40;++frame) {
            VR().m_rawHeadPose.m[1][3]=initialHeight-.4f+frame*.01f;
            g_torsoPose.time=TurnTime()-1./90;render();
            fixture[14*12+3]=123;
            const auto sceneBefore=g_scenePose;
            Detour_GetJoints(itemMemory,palette,0);
            uint64_t mask=0;int count=0;
            const auto shown=FirstPersonRenderBodyPalette(mask,count);
            const auto native=FirstPersonBodyPalette(mask,count);
            Check(shown && count==15 && !std::memcmp(shown+14*12,shown+7*12,12*sizeof(float)),
                  "production crouch recovery uploads a rigid hidden collar");
            Check(native && native[14*12+3]==123,"hidden collar correction cannot enter the physics palette");
            Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)),"hidden collar correction cannot move the view");
        }
        *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=savedBits;
        VR().m_rawHeadPose.m[1][3]=initialHeight;
    }
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
    config.firstPersonBodyForwardCm=cm;g_torsoClearance.Reset();ResetControllerBodyAnchor();
    VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1];
    const auto neutral=render();float largest=0;
    for(float drop:{.05f,.08f,.09f}) for(int frame=0;frame<40;++frame) {
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-drop;
        g_torsoClearance.time-=1./90;
        // Exercise clearance in an explicitly confirmed moving stance.
        // Initialization now starts planted, so resetting is not a release.
        UpdateControllerBodyAnchor();g_controllerBodyAnchor.held=false;
        g_controllerBodyAnchor.moved=TurnTime();
        g_controllerBodyAnchor.stepIntent.moving=true;g_controllerBodyAnchor.stepIntent.last=TurnTime();
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
    const auto savedHead=VR().m_rawHeadPose;
    const float savedNeck=VR().m_firstPersonNeutralEyeNeckY;
    VR().m_firstPersonNeutralEyeNeckY=.08f;
    auto aimed=[&](float pitch,float crouch) {
        Head(0,pitch);
        VR().m_rawHeadPose.m[0][3]=savedHead.m[0][3];
        VR().m_rawHeadPose.m[2][3]=savedHead.m[2][3];
        const float neck=.08f*(1-std::cos(pitch))-.12f*std::sin(pitch);
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-neck-crouch;
        g_torsoPose.Reset();return render();
    };
    for(int state:{1,16,2}) {
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=int16_t(state);
        const auto forward=aimed(0,0),down=aimed(-1.3f,0);
        Check(std::fabs(down[7].origin.y-forward[7].origin.y)<LiveWorldUnitsPerMetre()*.03f,
              "neck arc cannot drop the running or standing torso into a squat");
        const auto bent=aimed(-1.3f,.25f);
        const float hipDrop=bent[0].origin.y-down[0].origin.y;
        Check(hipDrop>LiveWorldUnitsPerMetre()*.15f && hipDrop<LiveWorldUnitsPerMetre()*.35f,
              "physical crouch lowers the fitted body once, without adding the neck arc again");
    }
    VR().m_rawHeadPose=savedHead;VR().m_firstPersonNeutralEyeNeckY=savedNeck;
    // Jump rendering must retain the successful standing size without running
    // standing leg IK, and must keep physics on the untouched native palette.
    const auto savedFlags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
    const float savedForward=config.firstPersonBodyForwardCm;
    const float fixedSize=g_bodySize.factor;
    bool correctedAir=false;
    config.firstPersonBodyForwardCm=11;
    *reinterpret_cast<uint32_t*>(itemMemory+0x1820)|=8;
    for(int jump:{3,9,15,28,25,26,27}) for(float drop:{0.f,.1f,.2f}) for(bool gravity:{true,false}) {
        if(!gravity && jump!=3 && jump!=9 && jump!=28) continue;
        *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=gravity ? 8 : 0;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=int16_t(jump);
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-drop;
        ++VR().m_poseSerial;
        Check(CanFitAirborneBody(itemMemory),"jump fixture enters airborne visual fitting");
        float nativeBefore[180]{};std::memcpy(nativeBefore,fixture,sizeof(nativeBefore));
        if(jump==3 || jump==9 || jump==15 || jump==28) {
            Frame hip{};ReadIKJoint(objectInfo,fixture,15,0,nullptr,nullptr,0,hip);
            const auto bow=firstperson::TorsoClearance::Bend(hip.origin,{1,0,0},.9f);
            for(int j=7;j<15;++j) WriteRows(Multiply(bow,ReadRows(fixture+j*12)),fixture+j*12);
        }
        const auto sceneBefore=g_scenePose;
        const auto itemBefore=std::string(reinterpret_cast<const char*>(itemMemory),sizeof(itemMemory));
        Detour_GetJoints(itemMemory,palette,0);
        Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)) &&
              itemBefore==std::string(reinterpret_cast<const char*>(itemMemory),sizeof(itemMemory)),
              "jump visual correction cannot move the camera or change native gameplay state");
        uint64_t mask=0;int count=0;const float* physics=FirstPersonBodyPalette(mask,count);
        Check(physics && count==15,"airborne physics capture remains available");
        Frame native[15]{},shown[15]{};
        for(int j=0;j<15;++j) {
            ReadIKJoint(objectInfo,physics,15,0,nullptr,nullptr,j,native[j]);
            ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,shown[j]);
        }
        const auto camera=BodyPaletteCameraOrigin(itemMemory,native[14]);
        const auto& current=*reinterpret_cast<const PHD_3DPOS*>(itemMemory+off::item_pos);
        const auto& previous=*reinterpret_cast<const PHD_3DPOS*>(itemMemory+off::item_pos_prev);
        const float t=std::clamp(*Ptr<int32_t>(dll.frameFrac),0,256)/256.f;
        const Vec pivot{native[0].origin.x,previous.y_pos+(current.y_pos-previous.y_pos)*t-camera.y,native[0].origin.z};
        const auto offset=BodyMeshOffset(itemMemory);
        for(int j=0;j<7;++j) {
            const Vec expected=Add(Add(pivot,Scale(Sub(native[j].origin,pivot),fixedSize)),{offset.x,0,offset.z});
            Check(firstperson::Length(Sub(shown[j].origin,expected))<.1f,
                  "jump hips and legs retain scaled native articulation without foot planting or eye anchoring");
            Check(std::fabs(firstperson::Length(Transform(shown[j].basis,{1,0,0}))-fixedSize)<.001f,
                  "airborne body does not revert to the larger native mesh");
            Check(std::fabs(firstperson::Length(Transform(native[j].basis,{1,0,0}))-1)<.001f,
                  "chest physics keeps the unscaled native animation palette");
        }
        correctedAir=correctedAir || g_torsoClearance.before<1;
        Check(g_torsoClearance.valid && g_torsoClearance.measured && g_torsoClearance.after>=.999f,
              "measured torso clearance stays active and clears the downward jump view");
        for(int hand=0;hand<2;++hand) {
            Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
            Check(firstperson::Length(Sub(Add(shown[hand?10:13].origin,camera),wrist.origin))<.1f,
                  "airborne visual fit retains exact tracked wrists");
        }
        Frame inverseChest{};Inverse(native[7],inverseChest);
        const auto chestChange=Multiply(shown[7],inverseChest);
        for(int shoulder:{8,11}) {
            Frame reference[3]{},actual[3]{};
            for(int j=0;j<3;++j) { reference[j]=Multiply(chestChange,native[shoulder+j]);actual[j]=shown[shoulder+j]; }
            CheckRigidTrackedReach(reference,actual);
        }
        Check(g_bodySize.factor==fixedSize,"jump never recalibrates visual size");
        if(jump==3 || jump==9 || jump==15 || jump==28) {
            Frame inverse{};Inverse({firstperson::RotationMatrix(firstperson::RotationOf(g_groundTorso.chest)),{}},inverse);
            const auto delta=firstperson::RotationOf(Multiply(inverse.basis,shown[7].basis));
            Check(2*std::atan2(firstperson::Length(delta.v),std::fabs(delta.w))<.21f,
                  "forward and straight-up jump render bounds collar tilt plus the existing clearance correction");
        }
        std::memcpy(fixture,nativeBefore,sizeof(nativeBefore));
    }

    // Temporary hand tracking loss or a hands-busy flag must not resize a
    // body that is already calibrated and flying through the same jump.
    {
        auto& status=*Ptr<int16_t>(dll.lara+2);const auto oldStatus=status;
        const bool left=VR().m_controllerPoseValid[0],right=VR().m_controllerPoseValid[1];
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=3;*reinterpret_cast<uint32_t*>(itemMemory+0x1820)|=8;
        for(int condition:{0,1,2,3,0}) {
            status=condition==1 ? 1 : 0;
            VR().m_controllerPoseValid[0]=condition!=2;VR().m_controllerPoseValid[1]=condition!=3;
            ++VR().m_poseSerial;
            Detour_GetJoints(itemMemory,palette,0);
            Frame chest{};
            Check(ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,7,chest),"airborne tracking fixture resolves chest");
            Check(std::fabs(firstperson::Length(Transform(chest.basis,{1,0,0}))-fixedSize)<.001f,
                  "mid-flight controller loss or hands-busy state cannot restore the larger native torso");
            float sphere[4]{};uint64_t visible=0;
            Check(FirstPersonBodyClearance(sphere,visible) && sphere[3]<LiveWorldUnitsPerMetre()*.031f,
                  "temporary hand loss cannot expand the body clipping sphere from three to twenty centimeters");
        }
        status=oldStatus;VR().m_controllerPoseValid[0]=left;VR().m_controllerPoseValid[1]=right;
    }
    Check(correctedAir,"airborne fixture actually exercises a chest intrusion correction");
    *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=savedFlags;
    *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
    VR().m_rawHeadPose=savedHead;config.firstPersonBodyForwardCm=savedForward;
    const float heldFactor=g_bodySize.factor,heldReserve=g_bodySize.eyeGap;
    VR().m_rawHeadPose.m[1][3]-=.05f;render();
    Check(g_bodySize.factor==heldFactor && g_bodySize.eyeGap==heldReserve,
          "head movement cannot change the captured visual fit");
    g_holsters=savedHolsters;skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
}
