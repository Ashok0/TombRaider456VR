// Native collision has resolved the impact, but the jump state can remain
// active until AnimateLara reaches its next landing dispatch window.
int recoveryGoal=2;
void __cdecl RecoveryContactTick(uint8_t* item,void*) {
    using namespace tr;
    ++jumpTicks;Detour_AnimateLara(item);++instantCollisions;
    *reinterpret_cast<uint32_t*>(item+0x1820)&=~8u;
    *reinterpret_cast<int16_t*>(item+20)=int16_t(recoveryGoal);
    *reinterpret_cast<int16_t*>(item+34)=*reinterpret_cast<int16_t*>(item+36)=0;
    *reinterpret_cast<int16_t*>(item+38)=975;
    auto& p=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    p.x_pos=123;p.y_pos=512;p.z_pos=456;
}
template<class Setup>
void TestLandingRecovery(const Setup& setup) {
    using namespace tr;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+18);
    auto& goal=*reinterpret_cast<int16_t*>(itemMemory+20);
    auto& clip=*reinterpret_cast<int16_t*>(itemMemory+24);
    auto& frame=*reinterpret_cast<int16_t*>(itemMemory+26);
    auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
    auto& position=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
    auto& hp=*reinterpret_cast<int16_t*>(itemMemory+38);
    for(int which:{0,1}) for(bool first:{false,true}) for(bool stabilized:{false,true})
    for(int airborne:{3,25,26,27,28}) for(int destination:{1,2}) {
        setup(which,first,false);config.firstPersonMovementStabilization=stabilized;
        // The run loop has native velocity even before it advances a frame.
        *reinterpret_cast<int32_t*>(jumpAnimations+12)=47<<16;
        state=goal=int16_t(airborne);flags=8;
        clip=int16_t(airborne==3?77:airborne==25?75:airborne==26?81:airborne==27?79:28);
        frame=*reinterpret_cast<int16_t*>(jumpAnimations+clip*48+28);
        recoveryGoal=destination;actionInput=0;g_haveManualInput=false;
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&RecoveryContactTick);
        g_torsoPose.recovering=true;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(position.x_pos==123 && position.y_pos==512 && position.z_pos==456 && hp==975 && flags==0,
              "landing recovery preserves the native collision position, impact damage and grounded flag");
        Check(instantAnimations==1 && instantCollisions==1 && jumpTicks==1 && instantCommands==0,
              "ending landing recovery never executes an extra animation, collision or takeoff");
        if(!first) {
            Check(state==airborne && goal==destination,"third person retains native post-impact jump/landing dispatch timing");
            continue;
        }
        Check(state==destination && clip==(destination==1?0:103) && !g_torsoPose.recovering && GroundJumpReady(itemMemory),
              "first-person contact immediately selects idle/run, releases recovery pose history and accepts another jump");
        // A fresh press released between polls must survive the first grounded
        // frame. Use the actual input and simulation hooks, including takeoff.
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&InstantAboveWater);
        float x=0,y=destination==1?1.f:0.f,r=0;
        FirstPersonInput(x,y,r,false,false);FirstPersonInput(x,y,r,false,true);FirstPersonInput(x,y,r,false,false);
        actionInput=destination==1?locomotion::Forward:0;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check((flags&8) && position.y_pos<512 && instantCommands==1 && hp==975,
              "a new A tap immediately after touchdown starts the next jump without replaying impact damage");
        Check(instantAnimations==2 && instantCollisions==2 && jumpTicks==2,
              "landing and immediate re-jump each use exactly one native simulation tick");
    }
    for(int which:{0,1}) for(bool first:{false,true}) for(int recovery:{31,82,92,99}) {
        setup(which,first,false);
        *reinterpret_cast<int32_t*>(jumpAnimations+12)=47<<16;
        state=goal=recovery==92?1:2;clip=int16_t(recovery);frame=17;
        *reinterpret_cast<int16_t*>(jumpAnimations+recovery*48+10)=state;
        position={123,512,456,0,0,0,0};const auto before=position;
        const bool finished=FinishFirstPersonLanding(itemMemory);
        Check(finished==first && !std::memcmp(&position,&before,sizeof(position)),
              "entering first person during a recovery clip ends the foot shuffle without moving the collision root");
        if(first) {
            Check(clip==(state==1?0:103) && GroundJumpReady(itemMemory),"all soft landing variants return directly to a jump-ready ground loop");
            for(int tick=0;tick<30 && state==2;++tick) {
                Detour_LaraAboveWater(itemMemory,nullptr);
                Check(clip==103 && state==2 && position.x_pos==123 && position.y_pos==512 && position.z_pos==456,
                      "stationary landing remains in idle without subsequent recovery foot shuffling or root drift");
            }
        } else Check(clip==recovery && frame==17,"third-person recovery animation is untouched");
    }
    for(int guard=0;guard<16;++guard) {
        setup(0,true,false);state=goal=2;clip=82;
        *reinterpret_cast<int16_t*>(jumpAnimations+82*48+10)=2;
        if(guard==0) g_active=false;
        if(guard==1) flags=8;
        if(guard==2) clip=24;
        if(guard==3) {state=goal=8;clip=99;} // TR5 death variant.
        if(guard==4) water=1;
        if(guard==5) hp=0;
        if(guard==6) goal=8;
        if(guard==7) *reinterpret_cast<int16_t*>(itemMemory+22)=19;
        if(guard==8) *reinterpret_cast<uint32_t*>(laraMemory+off::lara_movement_flags)=0x20;
        if(guard==9) *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=1;
        if(guard==10) dll.anims=0;
        if(guard==11) *reinterpret_cast<int16_t*>(jumpAnimations+103*48+40)=1;
        if(guard==12) {state=3;goal=2;clip=77;} // No observed contact.
        if(guard==13) {state=goal=19;clip=26;}
        if(guard==14) cutseq=1;
        if(guard==15) *reinterpret_cast<int16_t*>(jumpAnimations+82*48+10)=8;
        const auto before=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
        Check(!FinishFirstPersonLanding(itemMemory) && before==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),
              "landing shortcut excludes hard impacts, death, water, unconfirmed contact, interactions and invalid animation tables");
    }
}
