// Included inside the existing native/HD body fixtures.
namespace {
tr::PHD_VECTOR flareCurrent{},flarePrevious{};
int flareCalls=0,flareFallbacks=0,flareAge=-1;
float flareDrawPalette[33*12]{};
uint32_t flareDrawMask=0;
alignas(8) uint8_t flareLights[2][40]{};
int flareLightAllocations=0;
bool flareMirror=false;
bool flareReuseSlot=false;
uint8_t* __cdecl NativeFlareDynamic(int32_t x,int32_t y,int32_t z,int32_t falloff,int32_t red,int32_t green,int32_t blue) {
    auto* light=flareLights[flareReuseSlot ? 0 : (flareLightAllocations++)%2];
    std::memset(light,0,40);
    auto* xyz=reinterpret_cast<int32_t*>(light);
    xyz[0]=xyz[1]=x;xyz[2]=xyz[3]=y;xyz[4]=xyz[5]=z;
    light[24]=1;light[25]=uint8_t(red);light[26]=uint8_t(green);light[27]=uint8_t(blue);
    *reinterpret_cast<uint16_t*>(light+28)=uint16_t(falloff<<8);
    light[31]=0x55;*reinterpret_cast<float*>(light+32)=1.f/1024;
    return light;
}
void __cdecl ObserveFlareDraw(uint8_t* item,int32_t masked,int32_t pass) {
    Check(masked==1,"flare attachment draw retains explicit visibility mask");
    flareDrawMask=*reinterpret_cast<uint32_t*>(item+tr::off::item_mesh_bits);
    tr::Detour_GetJoints(item,flareDrawPalette,pass);
}
void __cdecl NativeFlareJoint(uint8_t*,tr::PHD_VECTOR* p,int32_t) {
    ++flareFallbacks;*p={100,200,300};
}
void __cdecl NativeFlareInHand(int32_t age) {
    ++flareCalls;flareAge=age;
    flareCurrent=flarePrevious={11,-64,41}; // Both native HD flare-tip offsets.
    tr::Detour_GunJoint(itemMemory,&flareCurrent,13);
    tr::Detour_FlarePreviousJoint(itemMemory,&flarePrevious,13);
    flareLightAllocations=0;
    if(age>0 && age<1350) for(int i=0;i<(flareMirror ? 2 : 1);++i) {
        // Reproduce the native light's extra Y bias and post-allocation writes.
        auto* light=tr::Detour_FlareDynamic(flareCurrent.x,flareCurrent.y-256,
            i ? 20000-flareCurrent.z : flareCurrent.z,16,220,140,60);
        auto* xyz=reinterpret_cast<int32_t*>(light);
        xyz[1]=flarePrevious.x;xyz[3]=flarePrevious.y-256;
        xyz[5]=i ? 20000-flarePrevious.z : flarePrevious.z;
        light[30]|=1;light[31]=(light[31]&15)|128;
    }
    tr::PHD_VECTOR other{};
    tr::Detour_GunJoint(itemMemory,&other,10);
    Check(other.x==100 && other.y==200 && other.z==300,"flare scope leaves right-hand gameplay joint native");
}
void TestFlareTracking(int count,const tr::motiongun::Frame* binds,tr::motiongun::Vec camera) {
    using namespace tr;using namespace tr::motiongun;
    const auto settings=config;const auto savedVR=VR();const auto savedGame=dll.game;
    const auto savedItem=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
    const auto savedLara=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
    const auto savedHistory=g_flareHandHistory;const bool savedReady=g_flareHooksReady;
    const auto savedLights=g_flareLights;const bool savedCapture=g_captureFlareLights;
    const auto dynamic=g_hFlareDynamic.m_trampoline;
    g_hFlareDynamic.m_trampoline=reinterpret_cast<void*>(&NativeFlareDynamic);
    const auto savedFollower=g_bodyFollower;const auto savedTorso=g_torsoPose;
    const auto savedHeading=g_headingBase;const auto savedOffset=g_bodyVisualOffset;
    const auto savedScope=g_bodySkinScope;const int savedArm=g_renderArm;
    const auto inHand=g_hFlareInHand.m_trampoline,prev=g_hFlarePreviousJoint.m_trampoline,joint=g_hGunJoint.m_trampoline;
    const auto savedDraw=g_hDrawCreatureHD.m_trampoline;
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& body=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
    g_hFlareInHand.m_trampoline=reinterpret_cast<void*>(&NativeFlareInHand);
    g_hFlarePreviousJoint.m_trampoline=g_hGunJoint.m_trampoline=reinterpret_cast<void*>(&NativeFlareJoint);
    g_flareHooksReady=true;config.firstPersonUnarmedIK=true;g_bodyVisualOffset={};
    g_bodySkinScope=true;g_renderArm=-1;status=0;gun=7;state=2;
    auto nearPoint=[](PHD_VECTOR p,Vec v) {return std::fabs(p.x-v.x)<=.51f && std::fabs(p.y-v.y)<=.51f && std::fabs(p.z-v.z)<=.51f;};
    for(int which:{0,1}) for(bool full:{false,true}) {
        dll.game=which;config.firstPersonFullBodyIK=full;
        flareMirror=which==0;
        g_flareHandHistory={};g_bodyFollower.Reset();g_torsoPose.Reset();
        Vec lastTip{};flareCalls=flareFallbacks=0;
        for(int wave=0;wave<8;++wave) {
            auto& hand=VR().m_rawControllerPose[0];
            hand=savedVR.m_rawControllerPose[0];
            hand.m[0][3]+=.03f*wave;hand.m[1][3]+=.02f*wave;hand.m[2][3]-=.04f*wave;
            const float roll=wave*.25f;hand.m[0][0]=hand.m[1][1]=std::cos(roll);
            hand.m[0][1]=-std::sin(roll);hand.m[1][0]=std::sin(roll);
            ++VR().m_poseSerial;
            Check(FlareIKReady() && UnarmedIKReady() && FullBodyIKReady()==full,"held flare enters arm and optional full-body IK in TR4/5");
            Check(!MotionReady() && !MotionTriggerMode() && !WeaponControlMode(),"flare never becomes a firearm or gun equip/trigger target");
            Frame wrist{};Basis controller{};Check(BuildControllerWrist(0,wrist,controller),"flare controller wrist builds");
            float palette[33*12]{};Detour_GetJoints(itemMemory,palette,0);
            Frame actual{};
            for(int i=0;i<count;++i) if((count==15 ? i : (i*7)%15)==13) actual=Multiply(ReadRows(palette+i*12),binds[i]);
            const auto tip=Transform(wrist,{11,-64,41});
            Check(firstperson::Length(Sub(Add(actual.origin,camera),wrist.origin))<.05f,"flare mesh wrist follows left controller position with rigid arm IK");
            Check(firstperson::Length(Sub(Add(Transform(actual,{11,-64,41}),camera),tip))<.06f,"flare mesh tip follows controller rotation and matches light origin");
            const auto savedBits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
            *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x3000;
            g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveFlareDraw);
            Detour_DrawCreatureHD(itemMemory,1,0);
            Check(flareDrawMask==0x3000 && *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)==0x3000,
                  "native flare/left forearm attachment stays visible and native mask is restored");
            for(int i=0;i<count;++i) if((count==15 ? i : (i*7)%15)==13) {
                const auto attachment=Multiply(ReadRows(flareDrawPalette+i*12),binds[i]);
                Check(firstperson::Length(Sub(Add(Transform(attachment,{11,-64,41}),camera),tip))<.06f,
                      "actual flare attachment render follows the same tracked tip as body and light");
            }
            *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=savedBits;
            g_hDrawCreatureHD.m_trampoline=savedDraw;
            Detour_FlareInHand(wave+20);
            Check(flareCalls==wave+1 && flareAge==wave+20,"flare update runs native burn/age logic exactly once");
            Check(nearPoint(flareCurrent,tip) && nearPoint(flarePrevious,wave ? lastTip : tip),"flare lighting interpolates between tracked wrist tips, never native arm positions");
            Check(!g_flareHandFrames && flareFallbacks==wave+1,"flare query override is scoped and preserves unrelated joint queries");
            const auto metadata=std::string(reinterpret_cast<char*>(flareLights[0]+24),16);
            // Move again without a simulation tick: light must follow the
            // render pose, not the last native update or native 256-unit bias.
            hand.m[0][3]+=.08f;++VR().m_poseSerial;
            Frame renderWrist{};Check(BuildControllerWrist(0,renderWrist,controller),"render-time flare wrist builds");
            const auto renderTip=Transform(renderWrist,{11,-64,41});
            UpdateFlareLightPosition();
            for(int slot=0;slot<(flareMirror ? 2 : 1);++slot) {
                const auto* xyz=reinterpret_cast<const int32_t*>(flareLights[slot]);
                Vec expected=renderTip;if(slot) expected.z=20000-expected.z;
                Check(nearPoint({xyz[0],xyz[2],xyz[4]},expected) && nearPoint({xyz[1],xyz[3],xyz[5]},expected),
                      "room/object dynamic light endpoints track rendered flare tip without offset or animation lag, including TR4 mirror");
            }
            Check(!std::memcmp(metadata.data(),flareLights[0]+24,16),"native flare brightness, falloff, flicker and priority are unchanged");
            lastTip=tip;
            if(full) {
                Check(g_bodyFollower.valid,"flare solve retains body-facing follower");
                TurnBodyToHead(itemMemory,1.f/60);
                Check(body.y_rot==Angle(g_bodyFollower.yaw),"body rotates with full-body flare facing");
            }
        }
    }
    // The simulation gate may be unavailable even though the render is tracked.
    g_scenePoseValid=false;Detour_FlareInHand(100);g_scenePoseValid=true;
    Frame renderWrist{};Basis renderController{};BuildControllerWrist(0,renderWrist,renderController);
    const auto latestTip=Transform(renderWrist,{11,-64,41});
    UpdateFlareLightPosition();
    auto lightPoint=[] {auto* p=reinterpret_cast<int32_t*>(flareLights[0]);return PHD_VECTOR{p[0],p[2],p[4]};};
    Check(nearPoint(lightPoint(),latestTip),"render-time flare light follows hand even when simulation wrist queries were native");
    // Native allocation can reuse the held light's pool slot for a dropped
    // flare or gun flash; that slot must immediately lose tracked ownership.
    flareLightAllocations=0;Detour_FlareDynamic(1,2,3,8,100,80,60);
    UpdateFlareLightPosition();
    Check(nearPoint(lightPoint(),{1,2,3}),"reused light slot never drags a dropped flare or gun flash with the hand");
    Detour_FlareInHand(100);flareLights[0][24]=0;UpdateFlareLightPosition();
    Check(!g_flareLights.slots[0].address,"native light pool reset invalidates cached held flare slot");
    Detour_FlareInHand(0);Check(!g_flareLights.count,"unlit flare creates no synthetic light");
    Detour_FlareInHand(1350);Check(!g_flareLights.count,"burned-out flare creates no synthetic light");
    Detour_FlareInHand(100);g_flareLights.time-=1;UpdateFlareLightPosition();
    Check(!g_flareLights.count,"stale held-flare light expires");
    Detour_FlareInHand(100);++g_flareLights.level;UpdateFlareLightPosition();
    Check(!g_flareLights.count,"level change discards held-flare light ownership");
    flareMirror=flareReuseSlot=true;Detour_FlareInHand(100);
    Check(!g_flareLights.count,"exhausted light pool cannot produce an invalid mirror plane");
    flareMirror=flareReuseSlot=false;
    // Before the first valid skin solve, body turning must not freeze.
    g_bodyFollower.Reset();config.firstPersonFullBodyIK=config.firstPersonBodyFollowsHead=true;
    config.firstPersonBodyDeadzoneDegrees=0;body.y_rot=0;g_headingBase=1.f;Head(0);
    TurnBodyToHead(itemMemory,1.f/60);
    Check(body.y_rot>0,"full-body mode turns while the flare follower has no valid pose yet");
    for(int reason=0;reason<8;++reason) {
        const auto oldApp=appMemory[0x9e4];const bool active=g_active,tracking=VR().m_controllerPoseValid[0];
        const int oldWater=water;
        status=0;gun=7;state=2;
        if(reason==0) status=2;if(reason==1) status=3;if(reason==2) status=1;
        if(reason==3) gun=8;if(reason==4) VR().m_controllerPoseValid[0]=false;
        if(reason==5) appMemory[0x9e4]&=~1;if(reason==6) g_active=false;if(reason==7) water=1;
        Check(!FlareIKReady(),"flare pickup/draw/throw, torch, lost tracking, classic graphics, third person and water stay native");
        Detour_FlareInHand(100);
        Check(flareCurrent.x==100 && flarePrevious.z==300 && !g_flareHandFrames && !g_flareHandHistory.item,
              "unsupported flare context restores native joints and clears interpolation history");
        const auto nativeLight=std::string(reinterpret_cast<char*>(flareLights[0]),40);
        UpdateFlareLightPosition();
        Check(!g_flareLights.count && !std::memcmp(nativeLight.data(),flareLights[0],40),
              "throw, draw and unsupported flare modes keep native light position");
        appMemory[0x9e4]=oldApp;g_active=active;VR().m_controllerPoseValid[0]=tracking;water=oldWater;
    }
    g_hFlareInHand.m_trampoline=inHand;g_hFlarePreviousJoint.m_trampoline=prev;g_hGunJoint.m_trampoline=joint;
    g_hFlareDynamic.m_trampoline=dynamic;g_flareLights=savedLights;g_captureFlareLights=savedCapture;
    config=settings;VR()=savedVR;dll.game=savedGame;g_flareHooksReady=savedReady;g_flareHandHistory=savedHistory;
    g_bodyFollower=savedFollower;g_torsoPose=savedTorso;g_headingBase=savedHeading;g_bodyVisualOffset=savedOffset;
    g_bodySkinScope=savedScope;g_renderArm=savedArm;
    std::memcpy(itemMemory,savedItem.data(),sizeof(itemMemory));std::memcpy(laraMemory,savedLara.data(),sizeof(laraMemory));
}
}
