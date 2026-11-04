#include <array>
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
void __cdecl JumpJoint(uint8_t*,tr::PHD_VECTOR* point,int,int) { *point={0,-700,0}; }
void TestJumpArmPalettes(uint8_t* objectInfo) {
    using namespace tr;using namespace tr::motiongun;
    const auto identity=firstperson::IdentityBasis();
    const Vec positions[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,-60},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&FakeBodyJoint)-g_boundBase);
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    config.firstPersonBodyScale=1;config.firstPersonJoint=14;
    config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=config.firstPersonInteractionAnchorZ=0;
    g_renderArm=-1;g_bodySkinScope=true;g_shadowDll=nullptr;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
    g_scenePose={0,-850,0,0,0,0,0};worldCameraValid=true;
    g_bodyVisualOffset=g_bodyWallOffset={};g_groundEye.valid=true;g_groundEye.local={0,-850,0};
    for(int count:{15,33}) {
        ResetBodyFollower();
        float fixture[33*12]{},poses[33*12]{},palette[33*12]{},repeat[33*12]{};
        int32_t mapping[33]{};Frame binds[33]{},baseline[15]{};
        auto* geom=objectInfo+off::object_geom;
        *reinterpret_cast<int32_t*>(geom+28)=count==15 ? 0 : count;
        *reinterpret_cast<int32_t**>(geom+48)=mapping;*reinterpret_cast<float**>(geom+72)=poses;
        skinFixture=fixture;skinFixtureCount=count;
        for(int step=0;step<5;++step) {
            state=int16_t(step==0 || step==4 ? 2 : step==1 ? 15 : 28);
            flags=state==28 ? 8 : 0;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=state==28 ? 91 : 103;
            ++VR().m_poseSerial;
            for(int eye=0;eye<2;++eye) {
                const Vec camera{300.f+6*eye,-200,-500};
                worldCameraPos[0]=camera.x;worldCameraPos[1]=camera.y;worldCameraPos[2]=camera.z;
                for(int j=0;j<15;++j) bodyJointWorld[j]=positions[j];
                // A floating HD frame and integer gameplay wrist need not
                // agree at an animation boundary. Neither moves the camera.
                if(step) for(int j:{10,13}) bodyJointWorld[j].z+=step%2 ? 160.f : -130.f;
                for(int i=0;i<count;++i) {
                    const int j=count==15 ? i : (i*7)%15;mapping[i]=j;
                    Frame frame{identity,Sub(positions[j],camera)};
                    if(step && j>=7 && j<=13) {
                        const Frame turn{firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},step*.35f)),{0,120,-90}};
                        frame=Multiply(turn,{identity,positions[j]});frame.origin=Sub(frame.origin,camera);
                    }
                    const Frame ib{identity,{float(i*3),float(i*2),float(-i)}};
                    Inverse(ib,binds[i]);
                    if(count==15) {
                        auto* m=reinterpret_cast<float*>(objectInfo+1676)+i*16;
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c) m[c*4+r]=ib.basis.r[r][c];
                        m[12]=ib.origin.x;m[13]=ib.origin.y;m[14]=ib.origin.z;m[15]=1;
                    } else {
                        auto raw=ib;for(int r=0;r<3;++r) {raw.basis.r[r][1]=ib.basis.r[r][2];raw.basis.r[r][2]=-ib.basis.r[r][1];}
                        WriteRows(raw,poses+i*12);
                    }
                    WriteRows(Multiply(frame,ib),fixture+i*12);
                }
                bits=0x3fff;
                Check(UnarmedIKReady(),"takeoff, flight and touchdown never drop controller arm ownership");
                Detour_GetJoints(itemMemory,palette,0);
                Detour_GetJoints(itemMemory,repeat,0);
                Check(!std::memcmp(palette,repeat,count*12*sizeof(float)),"repeated body draws cannot advance jump arm recovery");
                for(int i=0;i<count;++i) {
                    const int j=mapping[i];
                    auto actual=Multiply(ReadRows(palette+i*12),binds[i]);actual.origin=Add(actual.origin,camera);
                    if(step==0 && eye==0) baseline[j]=actual;
                    if(j>=8 && j<=13) {
                        Check(firstperson::Length(Sub(actual.origin,baseline[j].origin))<.1f,
                              "native takeoff and landing shoulder snaps cannot flash the tracked arms in either eye");
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                            Check(std::fabs(actual.basis.r[r][c]-baseline[j].basis.r[r][c])<.001f,
                                  "jump boundaries preserve arm orientation without a one-frame roll flash");
                    }
                    if(state==28 && j<8) Check(!std::memcmp(palette+i*12,fixture+i*12,12*sizeof(float)),
                                              "airborne torso and legs retain their native animation");
                    if(j==10 || j==13) {
                        Frame target{};Basis basis{};BuildControllerWrist(j==10 ? 1 : 0,target,basis);
                        Check(firstperson::Length(Sub(actual.origin,target.origin))<.04f,
                              "jump continuity never lags controller wrists or inherits gameplay joint interpolation");
                    }
                }
                uint64_t visible=0;int captured=0;
                const auto* physics=FirstPersonBodyPalette(visible,captured);
                Check(physics && captured==count && !std::memcmp(physics,fixture,count*12*sizeof(float)),
                      "jump arm stabilization never changes the physics skeleton");
                uint8_t itemBefore[sizeof(itemMemory)];std::memcpy(itemBefore,itemMemory,sizeof(itemBefore));
                const auto cameraBefore=g_scenePose;
                for(float cm:{1.f,5.f,20.f,10.f,0.f}) {
                    config.firstPersonBodyForwardCm=cm;
                    Detour_GetJoints(itemMemory,repeat,0);
                    for(int i=0;i<count;++i) {
                        const int j=mapping[i];
                        const auto old=Multiply(ReadRows(palette+i*12),binds[i]);
                        const auto adjusted=Multiply(ReadRows(repeat+i*12),binds[i]);
                        if(j<8 || j==14) Check(firstperson::Length(Sub(adjusted.origin,Add(old.origin,{0,0,cm*10})))<.05f,
                            "live body position moves torso and legs forward by the chosen centimetres, including zero reset");
                        if(j==10 || j==13) {
                            Check(firstperson::Length(Sub(adjusted.origin,old.origin))<.04f,"body offset never moves tracked wrists");
                            for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                                Check(std::fabs(adjusted.basis.r[r][c]-old.basis.r[r][c])<.001f,"body offset preserves controller wrist orientation");
                        }
                    }
                    Check(!std::memcmp(itemBefore,itemMemory,sizeof(itemBefore)) &&
                          !std::memcmp(&cameraBefore,&g_scenePose,sizeof(cameraBefore)),
                          "body slider cannot move Lara's simulation position or camera");
                    Check(!std::memcmp(FirstPersonBodyPalette(visible,captured),fixture,count*12*sizeof(float)),
                          "body slider leaves native physics palette unchanged");
                }
                bits=0x3600;Detour_GetJoints(itemMemory,repeat,0);
                for(int i=0;i<count;++i) if(mapping[i]>=8 && mapping[i]<=13)
                    for(int k=0;k<12;++k) Check(std::fabs(repeat[i*12+k]-palette[i*12+k])<.04f,
                        "separate hand meshes meet the same stabilized arm pose");
            }
        }
    }
    skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
    std::memset(objectInfo,0,off::object_stride);
}

#include "body_transition_regression.inl"

void TestJumpArmRecovery() {
    using namespace tr;using namespace tr::motiongun;
    firstperson::JumpArmIK history;
    const auto identity=firstperson::IdentityBasis();
    const Frame reference{identity,{200,-700,300}};
    const Frame initial[3]={{identity,{100,-600,300}},{identity,{150,-400,330}},{identity,{100,-200,370}}};
    Frame previous[3]{};
    for(int step=0;step<65;++step) {
        Frame chain[3];std::memcpy(chain,initial,sizeof(chain));
        const bool jump=step>=1 && step<=40;
        if(step) {
            const auto turn=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},1.f));
            for(auto& f:chain) {f.origin=Add(initial[0].origin,Transform(turn,Sub(f.origin,initial[0].origin)));f.origin.z+=180;f.basis=turn;}
        }
        Check(history.Apply(itemMemory,step,step/90.,jump,true,reference,chain),"jump arm recovery accepts a live render frame");
        if(step>0) for(int j=0;j<3;++j)
            Check(firstperson::Length(Sub(chain[j].origin,previous[j].origin))<90,
                  "landing returns to the body with bounded movement instead of a flash");
        for(int j=1;j<3;++j) Check(std::fabs(firstperson::Length(Sub(chain[j].origin,chain[j-1].origin))-
            firstperson::Length(Sub(initial[j].origin,initial[j-1].origin)))<.01f,
            "landing blend preserves bone lengths through large native arm turns");
        if(step==64) Check(std::fabs(chain[0].origin.z-initial[0].origin.z-180)<.01f && !history.current.recovering,
                          "landing recovery finishes and returns ownership to the standing body");
        std::memcpy(previous,chain,sizeof(chain));
    }
}
}
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p){return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.input=rva(&actionInput);dll.analogInput=rva(analogMemory);dll.frameFrac=rva(&fraction);
    dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&JumpJoint));g_boundDll=&dll;
    alignas(16) static uint8_t objectInfo[off::object_stride]{};
    dll.objects=rva(objectInfo);
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;g_motionHooksReady=true;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&FakeDraw);
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    const auto reset=[&](int which,bool fullBody) {
        RestoreHeadMesh();ResetMovementStabilization();
        config=Config{};config.enabled=config.firstPerson=config.firstPersonMotionGuns=true;
        config.firstPersonFullBodyIK=fullBody;config.firstPersonUnarmedIK=true;
        config.positionalTracking=config.firstPersonHeadTranslation=true;
        config.firstPersonRoomscaleMove=config.firstPersonWallCollision=config.firstPersonBodyFollowsHead=false;
        game=dll.game=which;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(appMemory,0,sizeof(appMemory));std::memset(cameraMemory,0,sizeof(cameraMemory));
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        state=2;bits=0x7fff;status=0;
        appMemory[0x9e4]=1;water=0;cutseq=cutseqNumber=cutseqTransition=spotCamera=0;
        g_active=g_runtimeInitialized=g_runtimeEnabled=g_haveHeading=g_scenePoseValid=true;
        g_headingItem=itemMemory;g_headingLevel=0;g_headingBase=0;g_scenePose={0,-700,0,0,0,0,0};
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(0);
        VR().RecenterFirstPersonHead();VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        for(int hand=0;hand<2;++hand) {
            VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[hand].m[0][3]+=(hand ? .2f : -.2f);
            VR().m_rawControllerPose[hand].m[1][3]-=.3f;
        }
        actionInput=0;std::memset(analogMemory,0,sizeof(analogMemory));fraction=256;
    };
    for(int which:{0,1}) for(bool fullBody:{false,true}) for(bool recovery:{false,true}) {
        reset(which,fullBody);
        if(recovery) {
            state=19;FirstPersonBodyMask(itemMemory);
            g_mountBodyTransition.active=true;
        }
        for(int jumpState:{15,28,28,2}) {
            state=int16_t(jumpState);
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=jumpState==28 ? 91 : 103;
            *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=jumpState==28 ? 8 : 0;
            PHD_3DPOS camera{};UpdateSceneCamera(camera);
            Check((bits&firstperson::ArmMeshBits)==firstperson::ArmMeshBits,"classic visibility retains all six arm and hand meshes during up-jump");
            Check((FirstPersonBodyMask(itemMemory)&firstperson::ArmMeshBits)==firstperson::ArmMeshBits,"mount recovery cannot restrict straight-up jump to hands only");
            const int previousDraws=draws;Detour_DrawCreatureHD(itemMemory,0,0);
            Check(draws==previousDraws+1 && (drawnBodyBits&firstperson::ArmMeshBits)==firstperson::ArmMeshBits,"HD body draw keeps both complete arms");
            if(jumpState==15 || jumpState==28) {
                Check(UnarmedIKReady(),"straight-up rise and descent retain controller hands with full-body IK on or off");
                Check(FullBodyIKReady()==fullBody,"full-body jump eligibility follows selected IK mode");
                for(int hand:{0,1}) {
                    motiongun::Frame wrist{},moved{};GunBasis basis{};
                    Check(BuildControllerWrist(hand,wrist,basis),"up-jump wrist available");
                    VR().m_rawControllerPose[hand].m[0][3]+=.1f;
                    Check(BuildControllerWrist(hand,moved,basis) && firstperson::Length(Sub(moved.origin,wrist.origin))>90,"raised or moved hands keep tracking in flight");
                }
            }
        }
        state=10;Check(!UnarmedIKReady() && FullBodyGripState(itemMemory),"ledge catch retains native grip ownership");
        state=28;status=1;Check(!UnarmedIKReady(),"native hands-busy state retains ownership");
        status=0;config.firstPersonUnarmedIK=false;Check(!UnarmedIKReady(),"disabled arm IK stays disabled");
        config.firstPersonUnarmedIK=true;water=1;Check(!UnarmedIKReady(),"water retains native handling");water=0;
        VR().m_controllerPoseValid[0]=false;Check(!UnarmedIKReady(),"tracking loss uses native fallback");
    }
    for(bool fullBody:{false,true}) {
        reset(0,fullBody);
        TestUnarmedHandMeshes();
    }
    for(int which:{0,1}) for(bool fullBody:{false,true}) {
        reset(which,fullBody);
        TestJumpArmPalettes(objectInfo);
    }
    for(int which:{0,1}) for(bool fullBody:{false,true}) {
        reset(which,fullBody);TestBodyTransitions(objectInfo);
        for(int rollState:{23,45,74}) {
            reset(which,fullBody);state=int16_t(rollState);status=4;
            *reinterpret_cast<int16_t*>(laraMemory+4)=1;
            Check(IsRollState(itemMemory),"running somersault joins ordinary roll visibility");
            SetMeshVisibility(true,true,false,false,false,false,true);
            const int before=draws;Detour_DrawCreatureHD(itemMemory,0,0);
            Check(g_rollHidden && bits==0 && draws==before,"tracked weapons cannot reveal the somersaulting body");
        }
    }
    TestPistolAnimationBlend();
    TestJumpArmRecovery();
    reset(0,false);
    TestUnarmedIK();
    VR().m_system=nullptr;
    std::printf("PASS: %d straight-up jump arm checks\n",checks);
}
