void TestJumpVisibilityRecovery() {
    using namespace tr;
    const auto savedState=*reinterpret_cast<int16_t*>(item+off::item_anim_state);
    const auto savedFlags=*reinterpret_cast<uint16_t*>(item+off::item_flags);
    const auto savedVisibility=g_bodyVisibility;
    const bool savedRecovery=g_torsoPose.recovering;
    for(int jump:{3,9,15,28}) for(bool fromHang:{false,true}) for(bool recovering:{false,true}) {
        *reinterpret_cast<int16_t*>(item+off::item_anim_state)=int16_t(jump);
        *reinterpret_cast<uint16_t*>(item+off::item_flags)=8;
        g_torsoPose.recovering=recovering;
        const double now=TurnTime();g_bodyVisibility={};
        g_bodyVisibility.Update(item,AppFlag(drva::app_off::level),now,fromHang,!fromHang,false);
        Check(g_bodyVisibility.mask!=UINT32_MAX,"fixture starts with a real traversal or landing visibility restriction");
        Check(CanFitAirborneBody(item),"jump visibility fixture uses the calibrated airborne body");
        const auto camera=g_scenePose;
        for(int eye=0;eye<2;++eye) Check(FirstPersonBodyMask(item)==UINT32_MAX,
            "new normal jump immediately clears stale hide timers in both eyes");
        g_bodyVisibility.until=now-1;
        Check(FirstPersonBodyMask(item)==UINT32_MAX,"expiry cannot reveal the body partway through a jump");
        Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)),"visibility correction cannot alter the jump camera");
    }
    for(int state:{10,25,26,27}) {
        *reinterpret_cast<int16_t*>(item+off::item_anim_state)=int16_t(state);
        *reinterpret_cast<uint16_t*>(item+off::item_flags)=state==10?0:8;
        g_bodyVisibility={};
        Check(FirstPersonBodyMask(item)!=(uint32_t)UINT32_MAX,"hanging and protective directional jump visibility remain restricted");
    }
    *reinterpret_cast<int16_t*>(item+off::item_anim_state)=savedState;
    *reinterpret_cast<uint16_t*>(item+off::item_flags)=savedFlags;
    g_bodyVisibility=savedVisibility;g_torsoPose.recovering=savedRecovery;
}

// Independent contract for the restored rigid arm solve: exact lengths and
// the minimum shoulder displacement required by the reachable annulus.
void CheckRigidTrackedReach(const tr::motiongun::Frame (&native)[3],const tr::motiongun::Frame (&posed)[3]) {
    using namespace tr::motiongun;
    const float a=tr::firstperson::Length(Sub(native[1].origin,native[0].origin));
    const float b=tr::firstperson::Length(Sub(native[2].origin,native[1].origin));
    const float distance=tr::firstperson::Length(Sub(posed[2].origin,native[0].origin));
    const float margin=std::min(a,b)*.001f;
    const float excess=std::fabs(distance-std::clamp(distance,std::fabs(a-b)+margin,a+b-margin));
    Check(std::fabs(tr::firstperson::Length(Sub(posed[1].origin,posed[0].origin))-a)<.1f &&
          std::fabs(tr::firstperson::Length(Sub(posed[2].origin,posed[1].origin))-b)<.1f,
          "tracked arm keeps fixed upper and lower lengths at all reaches");
    Check(std::fabs(tr::firstperson::Length(Sub(posed[0].origin,native[0].origin))-excess)<.1f,
          "shoulder moves only by the unreachable excess, remaining anchored within reach");
}

// Exercise TR1-3's actual render hooks through takeoff, flight and touchdown.
namespace {
void TestBodyPosition() {
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) for(bool fullBody:{false,true}) for(bool armed:{false,true})
    for(float yaw:{0.f,1.5707963f,-1.5707963f}) {
        BodyRig(game,armed);testConfig.firstPersonFullBodyIK=fullBody;testConfig.firstPersonBodyScale=1;
        g_haveManualInput=false;g_heading.base=yaw;
        Field<PHD_3DPOS>(off::item_pos).y_rot=Field<PHD_3DPOS>(off::item_pos_prev).y_rot=locomotion::Angle(yaw);
        Field<uint32_t>(off::item_mesh_bits)=0x7fff;g_bodySkinScope=true;
        ++testVR.m_poseSerial;Detour_GetJoints(item);
        uint32_t mask=0;float original[384];std::memcpy(original,FirstPersonBodySkin(mask),sizeof(original));
        uint8_t itemBefore[sizeof(item)];std::memcpy(itemBefore,item,sizeof(item));
        const auto cameraBefore=g_scenePose;
        for(float cm:{1.f,10.f,20.f,0.f}) {
            testConfig.firstPersonBodyForwardCm=cm;Detour_GetJoints(item);
            const auto* render=FirstPersonBodySkin(mask);Check(render!=nullptr,"body slider retains rendered palette");
            for(int j=0;j<15;++j) {
                const auto before=ReadRows(original+j*12),after=ReadRows(render+j*12);
                if(j<8 || j==14) Check(firstperson::Length(Sub(after.origin,Add(before.origin,{std::sin(yaw)*cm*10,0,std::cos(yaw)*cm*10})))<.06f,
                    "body position follows Lara's forward axis with full-body IK on or off");
                if(j==10 || j==13) {
                    Frame target{};Basis controller{};BuildControllerWrist(j==10 ? 1 : 0,target,controller);
                    Frame actual{};Check(ReadIKJoint(Ptr<uint8_t>(dll.objects),render,15,0,nullptr,nullptr,j,actual),"body adjustment wrist resolves");
                    Check(firstperson::Length(Sub(actual.origin,ExpectedPublishedWrist(j==10 ? 1 : 0,target).origin))<.04f &&
                        WristBasisError(after.basis,before.basis)<.001f,"body adjustment keeps wrists within connected reach");
                }
            }
            Check(!std::memcmp(itemBefore,item,sizeof(item)) && !std::memcmp(&cameraBefore,&g_scenePose,sizeof(cameraBefore)),
                "body position never alters collision or camera");
            Check(!std::memcmp(FirstPersonNativeBodySkin(),ikNative,15*12*sizeof(float)),"body position leaves native physics intact");
        }
        testConfig.firstPersonBodyForwardCm=10;
        *Ptr<int32_t>(dll.renderPass)=4;Check(locomotion::Length(BodyMeshOffset(item))==0,"native shadows retain their original body placement");
        *Ptr<int32_t>(dll.renderPass)=0;Field<int16_t>(off::item_anim_state)=10;
        Check(locomotion::Length(BodyMeshOffset(item))==0,"ledge contacts keep their native body placement");
        Field<int16_t>(off::item_anim_state)=2;g_active=false;
        Check(locomotion::Length(BodyMeshOffset(item))==0,"third-person body position remains native");
    }
}
void TestJumpArms() {
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) for(bool fullBody:{false,true}) for(int count:{15,32}) {
        BodyRig(game,false);testConfig.firstPersonFullBodyIK=fullBody;
        testConfig.firstPersonBodyScale=1;testConfig.firstPersonMovementStabilization=true;
        g_groundEye.valid=true;g_groundEye.local={0,-850,0};
        TestJumpVisibilityRecovery();
        Field<uint32_t>(off::item_mesh_bits)=0x7fff;
        auto* obj=Ptr<uint8_t>(dll.objects);auto* geom=obj+off::object_geom;
        Frame native[15]{},inverse[15]{},bind[15]{},baseline[15]{};
        for(int j=0;j<15;++j) {
            Check(ReadIKJoint(obj,ikNative,15,0,nullptr,nullptr,j,native[j]) &&
                  ReadIKBind(obj,15,0,nullptr,nullptr,j,bind[j]) && Inverse(bind[j],inverse[j]),
                  "jump fixture reads native joint frames and bindings");
        }
        int mapping[32]{};float poses[32*12]{};
        for(int i=0;i<count;++i) {
            const int j=mapping[i]=count==15 ? i : (i*7)%15;
            auto raw=inverse[j];
            for(int r=0;r<3;++r) {raw.basis.r[r][1]=inverse[j].basis.r[r][2];raw.basis.r[r][2]=-inverse[j].basis.r[r][1];}
            WriteRows(raw,poses+i*12);
        }
        *reinterpret_cast<int*>(geom+28)=count==15 ? 0 : count;
        *reinterpret_cast<int**>(geom+48)=mapping;*reinterpret_cast<float**>(geom+72)=poses;
        for(int step=0;step<5;++step) {
            const int state=step==0 || step==4 ? 2 : step==1 ? 15 : 28;
            Field<int16_t>(off::item_anim_state)=Field<int16_t>(off::item_goal_state)=int16_t(state);
            Field<int16_t>(off::item_anim_number)=state==28 ? 91 : 103;
            Field<uint16_t>(off::item_flags)=state==28 ? 8 : 0;
            ++testVR.m_poseSerial;
            for(int eye=0;eye<2;++eye) {
                const motiongun::Vec camera{300.f+6*eye,-200,-500};
                auto* w=Ptr<int32_t>(dll.w2vMatrix);
                w[3]=int(camera.x);w[7]=int(camera.y);w[11]=int(camera.z);
                for(int i=0;i<count;++i) {
                    const int j=mapping[i];Frame frame=native[j];
                    if(step && j>=7 && j<=13) {
                        const Frame turn{firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},step*.35f)),{0,120,-90}};
                        frame=Multiply(turn,frame);
                    }
                    frame.origin=Sub(frame.origin,camera);
                    WriteRows(Multiply(frame,inverse[j]),ikNative+i*12);
                }
                // Deliberately disagree with floating HD wrist interpolation.
                for(int j:{10,13}) for(auto offset:{off::item_joints_prev,off::item_joints_cur}) {
                    auto* p=reinterpret_cast<int32_t*>(item+offset+j*48);
                    p[11]=int32_t((native[j].origin.z+(step%2 ? 160 : -130))*16384);
                }
                Field<uint32_t>(off::item_mesh_bits)=0x7fff;g_bodySkinScope=true;
                Check(UnarmedIKReady(),"all three games retain tracked arms through takeoff and touchdown");
                Detour_GetJoints(item);
                uint32_t mask=0;const auto* render=FirstPersonBodySkin(mask);
                Check(render && (FirstPersonBodyMask(item)&firstperson::ArmMeshBits)==firstperson::ArmMeshBits,
                      "jump does not hide arms or hands");
                float saved[384];std::memcpy(saved,render,sizeof(saved));
                Check(!std::memcmp(FirstPersonNativeBodySkin(),ikNative,count*12*sizeof(float)),
                      "jump stabilization preserves the complete physics skeleton");
                Detour_GetJoints(item);render=FirstPersonBodySkin(mask);
                Check(!std::memcmp(saved,render,count*12*sizeof(float)),"redrawing a stereo frame cannot advance arm recovery");
                for(int i=0;i<count;++i) {
                    const int j=mapping[i];
                    auto actual=Multiply(ReadRows(render+i*12),bind[j]);actual.origin=Add(actual.origin,camera);
                    if(step==0 && eye==0) baseline[j]=actual;
                    if(state==28 && (j==8 || j==11)) {
                        auto shoulder=Multiply(ReadRows(ikNative+i*12),bind[j]);shoulder.origin=Add(shoulder.origin,camera);
                        Frame inverse{};Inverse(shoulder,inverse);
                        const auto change=Multiply(actual,inverse);
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
                            float dot=0;for(int k=0;k<3;++k) dot+=change.basis.r[k][r]*change.basis.r[k][c];
                            Check(std::fabs(dot-(r==c?1.f:0.f))<.323f,
                                  "jump shoulder adjustment retains bounded arm geometry in both eyes");
                        }
                    }
                    if(state==28 && j<7) Check(!std::memcmp(render+i*12,ikNative+i*12,12*sizeof(float)),
                                              "native airborne legs remain animated while the collar keeps its upright reference");
                    if(j==10 || j==13) {
                        Frame target{};Basis controller{};BuildControllerWrist(j==10 ? 1 : 0,target,controller);
                        target=ExpectedPublishedWrist(j==10 ? 1 : 0,target);
                        Check(firstperson::Length(Sub(actual.origin,target.origin))<.04f &&
                              WristBasisError(actual.basis,target.basis)<.001f,"tracked wrists stay exact across native animation changes");
                    }
                }
                uint8_t itemBefore[sizeof(item)];std::memcpy(itemBefore,item,sizeof(itemBefore));
                const auto cameraBefore=g_scenePose;
                for(float cm:{1.f,5.f,20.f,10.f,0.f}) {
                    testConfig.firstPersonBodyForwardCm=cm;Detour_GetJoints(item);render=FirstPersonBodySkin(mask);
                    for(int i=0;i<count;++i) {
                        const int j=mapping[i];const auto old=Multiply(ReadRows(saved+i*12),bind[j]);
                        const auto adjusted=Multiply(ReadRows(render+i*12),bind[j]);
                        if(j<8 || j==14) Check(firstperson::Length(Sub(adjusted.origin,Add(old.origin,{0,0,cm*10})))<.05f,
                            "body slider moves torso and legs by the selected distance and restores exact zero");
                        if(j==10 || j==13) {
                            Frame target{};Basis controller{};BuildControllerWrist(j==10 ? 1 : 0,target,controller);
                            target=ExpectedPublishedWrist(j==10 ? 1 : 0,target);
                            Check(firstperson::Length(Sub(Add(adjusted.origin,camera),target.origin))<.04f &&
                                WristBasisError(adjusted.basis,old.basis)<.001f,"body position retains connected wrists and exact orientation");
                        }
                    }
                    Check(!std::memcmp(itemBefore,item,sizeof(itemBefore)) && !std::memcmp(&cameraBefore,&g_scenePose,sizeof(cameraBefore)),
                        "body adjustment does not change camera or simulated Lara");
                    Check(!std::memcmp(FirstPersonNativeBodySkin(),ikNative,count*12*sizeof(float)),
                        "body adjustment leaves native physics skeleton unchanged");
                }
                Field<uint32_t>(off::item_mesh_bits)=0x3600;Detour_GetJoints(item);render=FirstPersonBodySkin(mask);
                for(int i=0;i<count;++i) if(mapping[i]>=8 && mapping[i]<=13)
                    for(int k=0;k<12;++k) {
                        if(std::fabs(render[i*12+k]-saved[i*12+k])>=.04f) std::fprintf(stderr,"attachment game=%d full=%d count=%d step=%d eye=%d joint=%d element=%d got=%f want=%f\n",game,int(fullBody),count,step,eye,mapping[i],k,render[i*12+k],saved[i*12+k]);
                        Check(std::fabs(render[i*12+k]-saved[i*12+k])<.04f,
                        "hand attachments share the body's continuous arm pose");
                    }
            }
        }
        // Mount recovery must not turn the next straight-up jump into floating hands.
        Field<int16_t>(off::item_anim_state)=19;FirstPersonBodyMask(item);
        for(int state:{15,28}) {
            Field<int16_t>(off::item_anim_state)=int16_t(state);
            Check((FirstPersonBodyMask(item)&firstperson::ArmMeshBits)==firstperson::ArmMeshBits,
                  "previous ledge recovery cannot hide up-jump arms");
        }
        Field<int16_t>(off::item_anim_state)=28;
        testControllerOffset[0].x+=.1f;++testVR.m_poseSerial;
        Field<uint32_t>(off::item_mesh_bits)=0x7fff;Detour_GetJoints(item);
        Frame wrist{},target{};Basis controller{};
        Check(ReadIKJoint(obj,g_bodyRenderPalette,count,count==15 ? 0 : count,
                          count==15 ? nullptr : mapping,count==15 ? nullptr : poses,13,wrist) &&
              BuildControllerWrist(0,target,controller) &&
              (target=ExpectedPublishedWrist(0,target),true) &&
              firstperson::Length(Sub(Add(wrist.origin,{float(Ptr<int32_t>(dll.w2vMatrix)[3]),-200,-500}),target.origin))<.04f,
              "moving a controller during a jump is never delayed by arm continuity");
        for(int reason=0;reason<4;++reason) {
            if(reason==0) *Ptr<int16_t>(dll.lara+2)=1;
            if(reason==1) { *Ptr<int16_t>(dll.lara+2)=0;testWater=1; }
            if(reason==2) { testWater=0;testMissingHand=0; }
            if(reason==3) { testMissingHand=-1;testConfig.firstPersonUnarmedIK=false; }
            Check(!UnarmedIKReady(),"busy hands, water, tracking loss and disabled IK retain native ownership");
        }
    }
    for(int game:{1,2,3}) for(int restIndex:{1,6}) for(int variant:{1,3}) {
        IKSetup(game);Field<int16_t>(off::item_anim_state)=28;Field<uint32_t>(off::item_mesh_bits)=0x3600;
        auto* rest=Ptr<uint8_t>(dll.laraHands)+restIndex*off::geom_stride;
        auto* selected=rest+variant*off::geom_stride;auto* geom=Ptr<uint8_t>(dll.objects)+off::object_geom;
        std::memset(rest,1,off::geom_stride);std::memset(selected,2,off::geom_stride);
        *reinterpret_cast<void**>(rest+off::geom_mesh)=*reinterpret_cast<void**>(selected+off::geom_mesh)=reinterpret_cast<void*>(0x1234);
        std::memcpy(geom,selected,off::geom_stride);
        { UnarmedRestHandScope scope(item,1);Check(!std::memcmp(geom,rest,off::geom_stride),
            "bare and gloved jumping hands retain the standing hand pose"); }
        Check(!std::memcmp(geom,selected,off::geom_stride),"hand substitution restores the complete native descriptor");
        if(variant==3) {
            Field<int16_t>(off::item_anim_state)=11;
            { UnarmedRestHandScope scope(item,1);Check(!std::memcmp(geom,selected,off::geom_stride),
                "native reach keeps the authored open hand"); }
        }
    }
    firstperson::JumpArmIK history;
    const auto identity=firstperson::IdentityBasis();
    const Frame reference{identity,{200,-700,300}};
    const Frame initial[3]={{identity,{100,-600,300}},{identity,{150,-400,330}},{identity,{100,-200,370}}};
    Frame previous[3]{};
    for(int step=0;step<65;++step) {
        Frame chain[3];std::memcpy(chain,initial,sizeof(chain));
        if(step) {
            const auto turn=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},1.f));
            for(auto& f:chain) {f.origin=Add(initial[0].origin,Transform(turn,Sub(f.origin,initial[0].origin)));f.origin.z+=180;f.basis=turn;}
        }
        const motiongun::Vec shoulder=chain[0].origin;
        Check(history.Apply(item,step,step/90.,step>=1 && step<=40,true,reference,chain),"jump recovery accepts render sample");
        Check(firstperson::Length(Sub(chain[0].origin,shoulder))<.01f,"jump recovery cannot detach a shoulder from the live torso");
        if(step) for(int j=1;j<3;++j) Check(firstperson::Length(Sub(Sub(chain[j].origin,chain[0].origin),Sub(previous[j].origin,previous[0].origin)))<90,
            "landing blends arm directions smoothly relative to the attached shoulder");
        for(int j=1;j<3;++j) Check(std::fabs(firstperson::Length(Sub(chain[j].origin,chain[j-1].origin))-
            firstperson::Length(Sub(initial[j].origin,initial[j-1].origin)))<.01f,"landing recovery preserves bone lengths");
        if(step==64) Check(!history.current.recovering && std::fabs(chain[0].origin.z-initial[0].origin.z-180)<.01f,
            "landing blend finishes and returns to standing pose");
        std::memcpy(previous,chain,sizeof(chain));
    }
}
}
