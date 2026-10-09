// Real input/movement hooks with a native standing/run jump consumer stub.
namespace {
alignas(16) uint8_t jumpAnimations[256*48]{};
uint8_t* jumpAnimationTable=jumpAnimations;
int jumpTicks=0,jumpClipSeen=-1;
uint64_t jumpBitsSeen=0;
bool jumpClearance=true;
void __cdecl NativeGroundJumpConsumer(uint8_t* item,void*) {
    using namespace tr;
    ++jumpTicks;jumpBitsSeen=actionInput;
    auto& state=*reinterpret_cast<int16_t*>(item+off::item_anim_state);
    auto& goal=*reinterpret_cast<int16_t*>(item+off::item_goal_state);
    auto& clip=*reinterpret_cast<int16_t*>(item+off::item_anim_number);
    auto& frame=*reinterpret_cast<int16_t*>(item+26);
    jumpClipSeen=clip;
    if (!(actionInput&0x10) || !jumpClearance) return;
    // Installed PDPs: standing entry has a narrow 184..185 compression
    // window; native AnimateLara increments the frame before GetChange.
    if(state==2) {
        goal=15;++frame;
        if ((clip==11 && frame>=184 && frame<=185) || clip==103) { state=15;clip=73; }
    } else if(state==1 && clip==0) { state=goal=3;clip=16; }
}
void TestGroundJumpPriority() {
    using namespace tr;
    const auto rva=[](const void* p) {return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
    auto& clip=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
    auto& frame=*reinterpret_cast<int16_t*>(itemMemory+26);
    auto& speed=*reinterpret_cast<int16_t*>(itemMemory+off::item_speed);
    auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
    auto& body=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
    const auto reset=[&](int which,bool first,int gait,int animation) {
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(appMemory,0,sizeof(appMemory));std::memset(cameraMemory,0,sizeof(cameraMemory));
        std::memset(jumpAnimations,0,sizeof(jumpAnimations));
        *reinterpret_cast<int16_t*>(jumpAnimations+11*48+10)=2;
        *reinterpret_cast<int16_t*>(jumpAnimations+11*48+28)=184;
        *reinterpret_cast<int16_t*>(jumpAnimations+11*48+30)=185;
        dll.anims=rva(&jumpAnimationTable);jumpAnimationTable=jumpAnimations;
        config.enabled=config.firstPerson=config.gamepadEnabled=true;
        config.firstPersonRoomscaleMove=false;
        game=dll.game=which;water=cutseq=cutseqNumber=cutseqTransition=spotCamera=0;vonCroyScene=0;
        g_runtimeEnabled=g_active=g_haveHeading=first;g_headingItem=first ? itemMemory : nullptr;
        g_haveManualInput=g_shifted=g_jumpPressed=false;g_headingBase=0;Head(0);
        VR().m_poseValid=true;g_renderTurn.Reset();g_bodyTime={};
        state=int16_t(gait);goal=2;clip=int16_t(animation);frame=185;speed=47;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        g_groundJump.Reset();
        actionInput=0;jumpTicks=0;jumpClearance=true;
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&NativeGroundJumpConsumer);
    };
    const struct {int state,clip;} gaits[]={
        {0,1},{0,2},{0,3},{0,4},{0,5},{0,7},{0,9},{0,20},{0,21},
        {1,0},{1,6},{1,8},{1,10},{2,11},{2,103},
        {16,38},{16,39},{16,40},{16,41},{22,65},{22,66},{21,67},{21,68}};
    for(int which:{0,1}) for(bool first:{false,true}) for(bool modern:{false,true})
    for(const auto& gait:gaits) for(bool tap:{false,true})
    for(locomotion::Vec direction:{locomotion::Vec{},locomotion::Vec{0,1},locomotion::Vec{0,-1},locomotion::Vec{-1,0},locomotion::Vec{1,0}}) {
        reset(which,first,gait.state,gait.clip);
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
        float x=direction.x,y=direction.z,r=0;
        FirstPersonInput(x,y,r,false,false);FirstPersonInput(x,y,r,false,true);
        if(tap) FirstPersonInput(x,y,r,false,false);
        const uint64_t keep=(uint64_t(1)<<40)|0x40|locomotion::Walk;
        actionInput=keep|(tap ? 0 : 0x10)|(first ? 0 : locomotion::MovementAction(direction,true));
        if (!first) {
            PHD_3DPOS camera{};
            for(int eye=0;eye<6;++eye) UpdateSceneCamera(camera);
            Check(g_groundJump.pending,"third-person stereo renders preserve the pending A press");
        }
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(!g_groundJump.pending,"actual compression/takeoff consumes the buffered press");
        const bool running=gait.state==1 && gait.clip==0;
        Check(state==(running ? 3 : 15),"A supersedes walking/start/stop immediately in either view, including released taps");
        Check((jumpBitsSeen&keep)==keep && (jumpBitsSeen&0x10),"jump preserves Action, Walk and 64-bit native input");
        Check((jumpBitsSeen&locomotion::Directions)==(locomotion::MovementAction(direction,true)&locomotion::Directions),
              "tap uses native forward/back/side jump directions");
        Check(jumpTicks==1 && !(flags&8) && body.x_pos==0 && body.y_pos==0 && body.z_pos==0,
              "jump priority runs native simulation once without fabricating takeoff or displacement");
        Check(bool(actionInput&0x10)==!tap,"only buffered synthetic Jump is removed after native tick");
        if(running) Check(jumpClipSeen==0 && speed==47,"running jump keeps native momentum and animation");
    }
    // Reproduce a requested compression goal while still finishing STOP.
    for(int which:{0,1}) for(bool first:{false,true}) for(int remaining:{0,1}) for(int requested:{2,15}) {
        reset(which,first,2,11);frame=int16_t(185-remaining);goal=int16_t(requested);
        float x=0,y=0,r=0;FirstPersonInput(x,y,r,false,true);FirstPersonInput(x,y,r,false,false);
        { GroundJumpInputScope scope(itemMemory,TurnTime()); }
        Check(g_groundJump.pending,"a goal request alone cannot consume the buffered jump");
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==15 && !g_groundJump.pending && jumpTicks==1,"last standing frame cannot swallow a released A tap");
    }
    for(int which:{0,1}) for(int reason=0;reason<27;++reason) {
        reset(which,true,0,1);
        if(reason==0) flags=8;
        if(reason==1) state=goal=3;
        if(reason==2) clip=12; // step-up
        if(reason==3) {state=2;clip=24;} // hard landing
        if(reason==4) *reinterpret_cast<int16_t*>(itemMemory+22)=19;
        if(reason==5) goal=19;
        if(reason==6) water=1;
        if(reason==7) *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=0;
        if(reason==8) *reinterpret_cast<uint32_t*>(laraMemory+off::lara_movement_flags)=0x20;
        if(reason==9) config.enabled=false;
        if(reason==10) g_shifted=true;
        if(reason==11) *reinterpret_cast<int32_t*>(cameraMemory+off::camera_type)=kCamFixed;
        if(reason==12) actionInput=0x100;
        if(reason==13) actionInput=0x1000;
        if(reason==14) dll.anims=0;
        if(reason==15) jumpAnimationTable=nullptr;
        if(reason==16) *reinterpret_cast<int16_t*>(jumpAnimations+11*48+40)=1;
        if(reason==17) *reinterpret_cast<int16_t*>(jumpAnimations+11*48+10)=3;
        if(reason==18) {state=goal=73;clip=223;} // sprint jump/dive remains native
        if(reason==19) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=1;
        if(reason==20) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InFMV)=1;
        if(reason==21) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InTitle)=1;
        if(reason==22) config.gamepadEnabled=false;
        if(reason==23) cutseq=1;
        if(reason==24) VR().m_poseValid=false;
        if(reason==25) {if(which==0) *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=4;else laraMemory[off::lara_skelebob]=1;}
        if(reason==26) {state=goal=10;clip=96;}
        actionInput|=0x10;
        const auto before=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
        { GroundJumpInputScope scope(itemMemory,TurnTime());scope.Prepare(); }
        Check(before==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),
              "jump priority preserves committed actions, sprint, vehicles, disabled contexts and invalid animation tables");
    }
    reset(0,true,0,1);jumpClearance=false;actionInput=0x10;
    Detour_LaraAboveWater(itemMemory,nullptr);
    Check(state==2 && !(flags&8) && jumpTicks==1,"native clearance rejection cannot be bypassed by jump priority");
    GroundJumpIntent intent;
    intent.Observe(true,true,1);intent.Observe(false,true,1.01);
    Check(intent.Wants(1.02) && !intent.Wants(1.26),"tap survives input polling but expires after 250 ms");
    intent.Observe(true,false,2);intent.Observe(true,true,2.01);
    Check(!intent.Wants(2.02),"holding A through an ineligible airborne state cannot queue a fresh jump");
    intent.Observe(false,true,2.03);intent.Observe(true,true,2.04);
    Check(intent.Wants(2.05),"a new grounded A press rearms buffering");
    intent.Bind(itemMemory,1);intent.Observe(false,true,3);intent.Observe(true,true,3.01);intent.Bind(itemMemory,2);
    Check(!intent.Wants(3.02),"level change cancels pending Jump");
    intent.Observe(false,true,4);intent.Observe(true,true,4.01);intent.Bind(nullptr,2);
    Check(!intent.Wants(4.02),"Lara replacement cancels pending Jump");
    reset(1,false,0,1);float x=0,y=1,r=0;FirstPersonInput(x,y,r,false,true);
    *reinterpret_cast<int32_t*>(cameraMemory+off::camera_type)=kCamFixed;
    UpdateGroundJumpCameraContext(itemMemory);
    Check(!g_groundJump.pending && g_groundJump.held,"camera takeover cancels tap without inventing a new press edge");
}
} // namespace
