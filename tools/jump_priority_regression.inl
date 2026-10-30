// Real input/movement hooks with a native standing/run jump consumer stub.
namespace {
alignas(16) uint8_t jumpAnimations[256*48]{};
uint8_t* jumpAnimationTable=jumpAnimations;
int jumpTicks=0,jumpClipSeen=-1;
uint64_t jumpBitsSeen=0;
bool jumpClearance=true;
int16_t compressionAxes[2]{};
void __cdecl NativeCompressionConsumer(uint8_t* item,void*) {
    using namespace tr;
    ++jumpTicks;jumpBitsSeen=actionInput;
    compressionAxes[0]=analogMemory[0];compressionAxes[1]=analogMemory[1];
    auto& body=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    const int status=*reinterpret_cast<int16_t*>(laraMemory+2),weapon=*reinterpret_cast<int16_t*>(laraMemory+4);
    const bool armed=weapon!=8 && (status==4 || (status==2 && weapon!=7));
    // Verified against real lara_as_compress/ModernControlsRotation by
    // verify_directional_jump.py: only unarmed analog rotates before dispatch.
    if(NewControls() && !armed && (analogMemory[0] || analogMemory[1]))
        body.y_rot=Angle(Radians(body.y_rot)+.5f);
    if(!jumpClearance) return;
    const auto direction=actionInput&locomotion::Directions;
    const int target=direction==locomotion::Left ? 27 : direction==locomotion::Right ? 26 :
        direction==locomotion::Back ? 25 : 3;
    *reinterpret_cast<int16_t*>(item+off::item_goal_state)=int16_t(target);
    *reinterpret_cast<int16_t*>(laraMemory+off::lara_move_angle)=
        Angle(Radians(body.y_rot)+(target==27 ? -kPi/2 : target==26 ? kPi/2 : target==25 ? kPi : 0));
}
float edgeYaw=0;
int edgeDistance=0,edgeDrop=1024,edgeQueries=0,edgeLaunch=-1;
bool edgeMissing=false;
void* __cdecl EdgeFloor(int32_t,int32_t,int32_t,int16_t* room) {
    ++edgeQueries; *room=9; return edgeMissing ? nullptr : &floorToken;
}
int32_t __cdecl EdgeHeight(void*,int32_t x,int32_t,int32_t z) {
    const float travel=x*std::sin(edgeYaw)+z*std::cos(edgeYaw);
    return travel>=edgeDistance ? edgeDrop : 0;
}
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
void __cdecl NativeEdgeJumpConsumer(uint8_t* item,void* collision) {
    using namespace tr;
    auto& state=*reinterpret_cast<int16_t*>(item+off::item_anim_state);
    auto& frame=*reinterpret_cast<int16_t*>(item+26);
    auto& p=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    const bool running=state==1;
    NativeGroundJumpConsumer(item,collision);
    if (running && state==3) frame=360;
    else if(state==3) ++frame;
    if(state==3 && frame==367) {
        edgeLaunch=int(std::lround(p.x_pos*std::sin(edgeYaw)+p.z_pos*std::cos(edgeYaw)));
        *reinterpret_cast<uint32_t*>(item+0x1820)|=8;
    }
    // Native clips 0,16,18 all move 47 units per grounded tick.
    if(edgeLaunch<0) {
        const float yaw=Radians(p.y_rot);
        p.x_pos+=(int(std::sin(yaw)*16384.f)*47)>>14;
        p.z_pos+=(int(std::cos(yaw)*16384.f)*47)>>14;
    }
}
#include "instant_jump_regression.inl"
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
    for(int which:{0,1}) for(int status:{0,1,2,3,4}) for(int weapon:{1,7,8})
    for(float heading:{0.f,.7f,-2.9f}) for(bool clear:{false,true})
    for(locomotion::Vec direction:{locomotion::Vec{-1,.2f},locomotion::Vec{1,-.2f},locomotion::Vec{.2f,-1}}) {
        reset(which,true,15,73);goal=15;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=2;
        *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);
        *reinterpret_cast<int16_t*>(laraMemory+4)=int16_t(weapon);
        Head(heading);body.y_rot=Angle(heading);jumpClearance=clear;
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&NativeCompressionConsumer);
        float x=direction.x,y=direction.z,r=0;FirstPersonInput(x,y,r,false,true);
        analogMemory[0]=12000;analogMemory[1]=24000;
        const uint64_t keep=(uint64_t(1)<<40)|0x10|0x40;
        actionInput=keep;
        Detour_LaraAboveWater(itemMemory,nullptr);
        const auto expected=locomotion::MovementAction(direction,true)&locomotion::Directions;
        const bool armed=weapon!=8 && (status==4 || (status==2 && weapon!=7));
        Check(body.y_rot==Angle(heading) && (jumpBitsSeen&locomotion::Directions)==expected && (jumpBitsSeen&keep)==keep,
              "armed and unarmed FP directional takeoff keep head facing and requested jump direction");
        Check(armed ? (compressionAxes[0] || compressionAxes[1]) : (!compressionAxes[0] && !compressionAxes[1]),
              "only the native unarmed rotation branch has its analog axes suppressed");
        Check(std::hypot(float(analogMemory[0]),float(analogMemory[1]))>26000 && jumpTicks==1 &&
              body.x_pos==0 && body.y_pos==0 && body.z_pos==0 && speed==47 && !(flags&8),
              "compression restores analog steering and preserves native speed and one simulation tick");
        Check(goal==(clear ? (expected==locomotion::Left ? 27 : expected==locomotion::Right ? 26 : 25) : 15),
              "native floor clearance still accepts or rejects directional jump");
        if(clear) Check(std::fabs(Wrap(Radians(*reinterpret_cast<int16_t*>(laraMemory+off::lara_move_angle))-
                                      locomotion::MovementYaw(direction,heading)))<.001f,
                        "unarmed takeoff move angle matches the requested cardinal direction");
    }
    for(int which:{0,1}) for(int reason=0;reason<13;++reason) {
        reset(which,true,15,73);
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=2;
        g_haveManualInput=true;g_manualLocal={-1,0};analogMemory[0]=1234;analogMemory[1]=5678;
        if(reason==0) g_active=false;
        if(reason==1) g_headingItem=nullptr;
        if(reason==2) g_haveHeading=false;
        if(reason==3) g_haveManualInput=false;
        if(reason==4) g_shifted=true;
        if(reason==5) water=1;
        if(reason==6) *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=0;
        if(reason==7) state=25;
        if(reason==8) state=3;
        if(reason==9) g_manualLocal={0,1};
        if(reason==10) config.gamepadEnabled=false;
        if(reason==11) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=0;
        if(reason==12) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=1;
        DirectionalJumpAxesScope scope(itemMemory);
        Check(analogMemory[0]==1234 && analogMemory[1]==5678,
              "directional takeoff scope excludes third person, airborne steering, forward jumps and ineligible contexts");
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
    const auto oldFloor=dll.getFloor,oldHeight=dll.getHeight;
    const auto edgeReset=[&](int which,bool first) {
        reset(which,first,1,0);goal=1;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=2;
        dll.getFloor=rva(&EdgeFloor);dll.getHeight=rva(&EdgeHeight);
        for(int c:{16,18}) {
            auto* a=jumpAnimations+c*48;
            *reinterpret_cast<int16_t*>(a+10)=3;
            *reinterpret_cast<int32_t*>(a+12)=47<<16;
            *reinterpret_cast<int16_t*>(a+28)=360;*reinterpret_cast<int16_t*>(a+30)=366;
        }
        edgeDistance=4096;edgeDrop=1024;edgeQueries=0;edgeLaunch=-1;edgeMissing=false;edgeYaw=0;
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&NativeEdgeJumpConsumer);
    };
    for(int which:{0,1}) for(bool first:{false,true}) for(float yaw:{0.f,.65f,-.65f,1.5707963f,-1.5707963f,2.4f,-2.4f,3.1415926f})
    for(int edge:{1024,2048,4096}) {
        edgeReset(which,first);edgeDistance=edge;edgeYaw=yaw;
        g_headingBase=yaw;body.y_rot=Angle(yaw);
        float x=0,y=1,r=0;
        for(int tick=0;tick<110 && edgeLaunch<0;++tick) {
            x=0;y=1;r=0; // Each poll starts with raw controller axes.
            FirstPersonInput(x,y,r,false,true);actionInput=locomotion::Forward|0x10|(uint64_t(1)<<40);
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check((actionInput&((uint64_t(1)<<40)|0x10))==((uint64_t(1)<<40)|0x10),"edge wait restores physical Jump and preserves high input bits");
            if(tick==0) {Check(state==1 && g_groundJump.edgeWaiting,"held forward Jump waits instead of taking off far from edge");g_groundJump.until=TurnTime()-1;}
        }
        Check(edgeLaunch>=0 && edgeDistance-edgeLaunch>0 && edgeDistance-edgeLaunch<=49,
              "native seven-frame run-up launches in the last supported run tick in TR4/5, both views and cardinal/diagonal headings");
        Check(g_groundJump.edgeBlocked && !g_groundJump.edgeWaiting,"takeoff consumes automatic hold even beyond tap timeout");
        state=goal=1;clip=0;flags=0;body.x_pos=body.z_pos=0;
        x=0;y=1;r=0;
        FirstPersonInput(x,y,r,false,true);actionInput=locomotion::Forward|0x10;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(!(jumpBitsSeen&0x10) && state==1,"held button cannot rearm after landing");
    }
    for(int which:{0,1}) for(int terrain:{0,256,384,-512,-32512,1024}) {
        edgeReset(which,true);edgeDistance=200;edgeDrop=terrain;edgeMissing=terrain==1024;
        float x=0,y=1,r=0;FirstPersonInput(x,y,r,false,true);actionInput=locomotion::Forward|0x10;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(bool(jumpBitsSeen&0x10)==(terrain==-32512),"flat ground, normal steps, walls and missing query do not masquerade as a ledge; NO_HEIGHT is a drop");
    }
    for(int mode=0;mode<6;++mode) {
        edgeReset(0,true);float x=0,y=1,r=0;FirstPersonInput(x,y,r,false,true);actionInput=locomotion::Forward|0x10;
        Detour_LaraAboveWater(itemMemory,nullptr);
        if(mode==0) FirstPersonInput(x,y,r,false,false); // quick release
        if(mode==1) {g_groundJump.until=TurnTime()-1;FirstPersonInput(x,y,r,false,false);}
        if(mode==2) {y=0;FirstPersonInput(x,y,r,false,true);} // stop
        if(mode==3) {x=1;y=0;FirstPersonInput(x,y,r,false,true);} // side
        if(mode==4) g_groundJump.Bind(itemMemory,999);
        if(mode==5) {water=1;g_groundJump.Observe(true,false,TurnTime());water=0;}
        actionInput=locomotion::Forward|((mode<2)?0:0x10);
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(bool(jumpBitsSeen&0x10)==(mode==0),"short release jumps manually; long release, stop, direction change, level and water cancel automatic hold");
        if(mode==3) Check((jumpBitsSeen&locomotion::Directions)==locomotion::StepRight,"cancelling an automatic hold restores normal sidestep movement");
    }
    edgeReset(0,true);float ex=0,ey=1,er=0;FirstPersonInput(ex,ey,er,false,true);
    state=goal=2;clip=103;actionInput=locomotion::Forward|0x10;
    Detour_LaraAboveWater(itemMemory,nullptr);
    Check(state==2 && !(jumpBitsSeen&0x10),"holding forward plus Jump before running cannot invoke standing jump priority");
    edgeReset(0,true);edgeDistance=350;jumpClearance=false;
    ex=er=0;ey=1;FirstPersonInput(ex,ey,er,false,true);actionInput=locomotion::Forward|0x10;
    Detour_LaraAboveWater(itemMemory,nullptr);
    Check(state==1 && g_groundJump.edgeWaiting && !g_groundJump.edgeBlocked,"native rejection cannot consume automatic Jump before takeoff");
    edgeReset(0,false);*reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=0;
    ex=er=0;ey=1;FirstPersonInput(ex,ey,er,false,true);actionInput=locomotion::Forward|0x10;
    Detour_LaraAboveWater(itemMemory,nullptr);
    Check(state==3 && !g_groundJump.edgeWaiting,"third-person classic controls retain native stride-based jump timing");
    dll.getFloor=oldFloor;dll.getHeight=oldHeight;
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
    TestInstantJump(reset);
}
} // namespace
