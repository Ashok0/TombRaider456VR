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
            Check(std::fabs(chest.origin.x-standingChest.origin.x)<.06f && std::fabs(chest.origin.z-standingChest.origin.z)<.06f && std::fabs(chest.origin.y-standingChest.origin.y)<.06f,
                  "animated chest pitch and run-to-stop blend cannot slide the visible torso");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) Check(std::fabs(chest.basis.r[r][c]-standingChest.basis.r[r][c])<.001f,
                  "torso mesh orientation stays stable when native stop animation pitches backwards");
            float sphere[4]{};uint64_t clearanceMask=0;
            Check(FirstPersonBodyClearance(sphere,clearanceMask) && sphere[3]==200 && clearanceMask==0x40ff,
                  "headset clearance protects body surfaces and excludes all arm and hand joints");
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
