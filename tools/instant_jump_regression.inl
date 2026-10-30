// Included in the jump fixture namespace. Metadata comes from installed PDPs;
// the native-code verifier separately executes the real takeoff commands.
int instantAnimations=0,instantCollisions=0,instantCommands=0,instantCompression=0;
int instantFloor=0,instantCeiling=-1024;
void __cdecl InstantClearance(tr::RoomCollision* coll,int32_t,int32_t,int32_t,int16_t,int32_t height) {
    Check(height==762 && coll->radius==100,"instant takeoff probes the native standing collision hull");
    coll->floorSamples[0]=instantFloor;coll->floorSamples[1]=instantCeiling;
}
void __cdecl InstantCompression(uint8_t* item,void* collision) {
    ++instantCompression;
    NativeCompressionConsumer(item,collision);--jumpTicks;
    if(!(actionInput&tr::locomotion::Directions))
        *reinterpret_cast<int16_t*>(item+tr::off::item_goal_state)=15;
}
void __cdecl InstantAnimate(uint8_t* item) {
    using namespace tr;
    ++instantAnimations;
    auto& state=*reinterpret_cast<int16_t*>(item+off::item_anim_state);
    auto& goal=*reinterpret_cast<int16_t*>(item+off::item_goal_state);
    auto& clip=*reinterpret_cast<int16_t*>(item+off::item_anim_number);
    auto& frame=*reinterpret_cast<int16_t*>(item+26);
    auto& flags=*reinterpret_cast<uint32_t*>(item+0x1820);
    auto enter=[&](int next) {
        clip=int16_t(next);frame=*reinterpret_cast<int16_t*>(jumpAnimations+next*48+28);
        state=*reinterpret_cast<int16_t*>(jumpAnimations+next*48+10);
    };
    ++frame;
    if(state==2 && goal==15) enter(73);
    else if(state==1 && goal==3) enter(frame<=11 ? 18 : 16);
    else if(clip==73 && frame>=1493 && goal!=15) {
        enter(goal==3?76:goal==25?74:goal==27?78:80);
    }
    const auto* a=jumpAnimations+clip*48;
    if(frame>*reinterpret_cast<const int16_t*>(a+30)) {
        if(clip==16 || clip==18 || clip==74 || clip==76 || clip==78 || clip==80 || clip==91) {
            ++instantCommands;flags|=8;
            *reinterpret_cast<int16_t*>(item+34)=clip==91 ? 2 : clip==16 || clip==18 ? 75 : 50;
            *reinterpret_cast<int16_t*>(item+36)=clip==91 ? -110 : -100;
        }
        enter(*reinterpret_cast<const int16_t*>(a+32));
    }
    if(flags&8) {
        auto& p=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
        auto& fall=*reinterpret_cast<int16_t*>(item+36);fall+=6;p.y_pos+=fall;
        const float yaw=Radians(*reinterpret_cast<int16_t*>(laraMemory+off::lara_move_angle));
        const int speed=*reinterpret_cast<int16_t*>(item+34);
        p.x_pos+=(int(std::sin(yaw)*16384.f)*speed)>>14;
        p.z_pos+=(int(std::cos(yaw)*16384.f)*speed)>>14;
    }
}
void __cdecl InstantAboveWater(uint8_t* item,void* collision) {
    using namespace tr;
    ++jumpTicks;
    auto& state=*reinterpret_cast<int16_t*>(item+off::item_anim_state);
    auto& goal=*reinterpret_cast<int16_t*>(item+off::item_goal_state);
    if(state==15) InstantCompression(item,collision);
    else if((state==1 || state==2) && (actionInput&0x10) && jumpClearance) goal=state==1 ? 3 : 15;
    Detour_AnimateLara(item);
    ++instantCollisions;
}
#include "landing_recovery_regression.inl"
template<class Reset>
void TestInstantJump(const Reset& reset) {
    using namespace tr;
    const auto oldDll=dll;const auto oldAnimate=g_hAnimateLara.m_trampoline;
    const auto oldAbove=g_hLaraAboveWater.m_trampoline;
    const auto rva=[](const void* p) {return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+18);
    auto& goal=*reinterpret_cast<int16_t*>(itemMemory+20);
    auto& clip=*reinterpret_cast<int16_t*>(itemMemory+24);
    auto& frame=*reinterpret_cast<int16_t*>(itemMemory+26);
    auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
    auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
    const auto setup=[&](int which,bool first,bool run) {
        reset(which,first,run?1:2,run?0:103);
        const auto anim=[](int n,int state,int first,int last,int next,int commands) {
            auto* a=jumpAnimations+n*48;
            *reinterpret_cast<int16_t*>(a+10)=int16_t(state);
            *reinterpret_cast<int16_t*>(a+28)=int16_t(first);
            *reinterpret_cast<int16_t*>(a+30)=int16_t(last);
            *reinterpret_cast<int16_t*>(a+32)=int16_t(next);
            *reinterpret_cast<int16_t*>(a+40)=int16_t(commands);
        };
        anim(0,1,0,21,0,6);anim(103,2,1915,1958,103,0);anim(73,15,1485,1495,91,which==0?2:4);
        anim(74,25,1496,1498,75,2);anim(75,25,1499,1538,75,0);
        anim(76,3,1539,1541,77,2);anim(77,3,1542,1581,77,0);
        anim(78,27,1582,1584,79,2);anim(79,27,1585,1624,79,0);
        anim(80,26,1625,1627,81,2);anim(81,26,1628,1667,81,0);
        anim(91,28,1767,1769,28,2);anim(28,28,524,534,28,0);
        anim(16,3,360,366,17,5);anim(17,3,367,377,77,0);
        anim(18,3,378,384,19,5);anim(19,3,385,395,77,0);
        for(int n:{16,18,73,74,76,78,80,91}) {
            auto* a=jumpAnimations+n*48;const int next=*reinterpret_cast<int16_t*>(a+32);
            *reinterpret_cast<int16_t*>(a+34)=*reinterpret_cast<int16_t*>(jumpAnimations+next*48+28);
            if(n==16 || n==18) *reinterpret_cast<int32_t*>(a+12)=47<<16;
        }
        frame=run?0:1915;goal=state;
        dll.laraAsCompress=rva(&InstantCompression);
        dll.getCollisionInfo=rva(&InstantClearance);instantFloor=0;instantCeiling=-1024;
        dll.getFloor=dll.getHeight=0; // Manual jumps in these cases.
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=2;
        instantAnimations=instantCollisions=instantCommands=instantCompression=0;
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&InstantAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&InstantAboveWater);
    };
    for(int which:{0,1}) for(bool first:{false,true}) for(bool armed:{false,true})
    for(int direction:{0,1,2,4,8}) for(bool tap:{false,true}) {
        setup(which,first,false);
        *reinterpret_cast<int16_t*>(laraMemory+2)=armed?4:0;
        *reinterpret_cast<int16_t*>(laraMemory+4)=1;
        float x=direction==4?-1.f:direction==8?1.f:0.f,y=direction==1?1.f:direction==2?-1.f:0.f,r=0;
        FirstPersonInput(x,y,r,false,true);if(tap) FirstPersonInput(x,y,r,false,false);
        actionInput=(uint64_t(1)<<40)|direction|(tap?0:0x10);
        Detour_LaraAboveWater(itemMemory,nullptr);
        if(first) {
            Check((flags&8) && pos.y_pos<0 && clip==(direction==0?28:direction==1?77:direction==2?75:direction==4?79:81),
                  "first-person standing/forward/side/back jumps reach native airborne clip on the input tick");
            Check(instantCommands==1 && instantCompression==1,"instant jump runs native direction selection and takeoff once");
        } else Check(!(flags&8) && clip==73 && frame==1485 && instantCommands==0,
                     "third person retains the complete feet-together compression animation");
        Check(instantAnimations==1 && instantCollisions==1 && jumpTicks==1,
              "immediate jump still executes exactly one animation and collision tick");
        Check((actionInput&(uint64_t(1)<<40)) && bool(actionInput&0x10)==!tap,"immediate jump preserves physical and buffered input ownership");
    }
    for(int which:{0,1}) for(bool first:{false,true}) for(int stride:{0,10,11,21}) {
        setup(which,first,true);frame=int16_t(stride);actionInput=0x10|locomotion::Forward;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(first ? (flags&8) && clip==(stride+1<=11?19:17) && instantCommands==1 :
              !(flags&8) && clip==(stride+1<=11?18:16) && instantCommands==0,
              "first-person running jump skips run-up and preserves the native leading-leg jump clip; third person keeps run-up");
        Check(instantAnimations==1 && instantCollisions==1,"running takeoff performs one native simulation tick");
        dll.getFloor=rva(&EdgeFloor);dll.getHeight=rva(&EdgeHeight);
        Check(RunningJumpLookahead()==(first?47:376),"ledge assist matches shortened first-person takeoff without launching eight steps early");
    }
    TestLandingRecovery(setup);
    for(int reason=0;reason<17;++reason) {
        setup(0,true,false);goal=15;
        if(reason==0) g_active=false;
        if(reason==1) flags=8;
        if(reason==2) water=1;
        if(reason==3) *reinterpret_cast<int16_t*>(itemMemory+38)=0;
        if(reason==4) *reinterpret_cast<int16_t*>(itemMemory+22)=19;
        if(reason==5) *reinterpret_cast<uint32_t*>(laraMemory+off::lara_movement_flags)=0x20;
        if(reason==6) dll.laraAsCompress=0;
        if(reason==7) dll.anims=0;
        if(reason==8) goal=2; // Native control rejected Jump.
        if(reason==9) clip=82; // Landing must complete normally.
        if(reason==10) {state=goal=73;clip=223;} // Sprint dive.
        if(reason==11) *reinterpret_cast<int16_t*>(jumpAnimations+76*48+40)=0;
        if(reason==12) *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=1;
        if(reason==13) instantCeiling=-99;
        if(reason==14) instantFloor=512;
        if(reason==15) instantFloor=-128;
        if(reason==16) dll.getCollisionInfo=0;
        const auto snapshot=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
        SkipFirstPersonJumpPreparation(itemMemory);
        Check(snapshot==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) && instantCompression==0,
              "immediate takeoff excludes third person, invalid metadata, falls, interactions, sprint, water and rejected input");
    }
    dll=oldDll;g_hAnimateLara.m_trampoline=oldAnimate;g_hLaraAboveWater.m_trampoline=oldAbove;
}
