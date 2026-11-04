namespace {
void TestBodyTransitions() {
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) for(bool fullBody:{false,true})
    for(float yaw:{0.f,1.5707963f,-1.5707963f}) for(float cm:{0.f,5.f,20.f}) {
        BodyRig(game,false);testConfig.firstPersonFullBodyIK=fullBody;testConfig.firstPersonBodyScale=1;
        testConfig.firstPersonBodyForwardCm=cm;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_haveManualInput=false;g_heading.base=yaw;
        Field<PHD_3DPOS>(off::item_pos).y_rot=Field<PHD_3DPOS>(off::item_pos_prev).y_rot=locomotion::Angle(yaw);
        auto* obj=Ptr<uint8_t>(dll.objects);
        int step=0;
        for(int status:{0,2,4,3,0,0}) {
            *Ptr<int16_t>(dll.lara+2)=int16_t(status);
            Field<int16_t>(off::item_anim_state)=step<2 ? 1 : 2;
            Field<uint32_t>(off::item_mesh_bits)=0x3fff;g_bodySkinScope=true;
            for(int j=0;j<15;++j) {
                WriteRows({firstperson::IdentityBasis(),{step*13.f,0,-step*39.f}},ikNative+j*12);
                const auto* bind=Ptr<float>(dll.objects+192)+j*16;
                for(auto offset:{off::item_joints_cur,off::item_joints_prev}) {
                    auto* joint=reinterpret_cast<int32_t*>(item+offset+j*48);
                    joint[3]=int32_t((-bind[12]+step*13.f)*16384);
                    joint[11]=int32_t((-bind[14]-step*39.f)*16384);
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
            for(int hand=0;hand<2;++hand) {
                Frame wrist{},shown{};Basis controller{};BuildControllerWrist(hand,wrist,controller);
                Check(ReadIKJoint(obj,render,15,0,nullptr,nullptr,hand?10:13,shown) && firstperson::Length(Sub(shown.origin,wrist.origin))<.05f,
                      "draw and holster preserve live controller wrists");
            }
            Check(!std::memcmp(FirstPersonNativeBodySkin(),ikNative,15*12*sizeof(float)),"transition physics stays native");
            ++step;
        }
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
