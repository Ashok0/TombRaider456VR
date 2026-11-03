namespace {
alignas(16) uint8_t switchMemory[9416]{};
uint8_t* switchItems=switchMemory;
int switchCount=1,switchActivations=0,switchPositionCalls=0;
bool switchSightBlocked=false;
void* __cdecl FakeSwitchFloor(int32_t,int32_t,int32_t,int16_t*) { return nullptr; }
int32_t __cdecl FakeSwitchSight(tr::ShotVector*,tr::ShotVector*) { return !switchSightBlocked; }
int16_t switchBounds[12]={0,0,0,0,0,0,-1820,1820,-5460,5460,-1820,1820};
int16_t leverBounds[6]={-64,64,-900,0,-32,32};
tr::firstperson::SwitchAnim leverAnimations[4]{};
tr::firstperson::SwitchAnim* leverAnimationPtr=leverAnimations;
tr::firstperson::SwitchChange leverChanges[2]={{0,1,0},{1,1,1}};
tr::firstperson::SwitchChange* leverChangePtr=leverChanges;
tr::firstperson::SwitchRange leverRanges[2]={{0,1,2,2},{1,2,3,13}};
tr::firstperson::SwitchRange* leverRangePtr=leverRanges;
int16_t leverCommands[1]={4},dummyLeverFrames[16]{};
int16_t* leverCommandPtr=leverCommands;
alignas(16) uint8_t leverObject[319*3792]{};
int leverNativeTicks=0;
int leverControls=0;
uint8_t leverClip[40]{};
void __cdecl FakeLeverMatrices(uint8_t* item) {
    using namespace tr;
    std::memset(leverClip,0,sizeof(leverClip));
    const int a=*reinterpret_cast<int16_t*>(item+24),f=*reinterpret_cast<int16_t*>(item+26);
    float progress=a==0 ? 0.f : a==1 ? 1.f : a==2 ? (f-2)/10.f : 1-(f-13)/10.f;
    auto* matrices=reinterpret_cast<float*>(item+7832);
    motiongun::WriteRows({firstperson::IdentityBasis(),{}},matrices);
    motiongun::WriteRows({firstperson::IdentityBasis(),{0,progress*.2f*LiveWorldUnitsPerMetre(),
        std::sin(progress*3.14159265f)*.05f*LiveWorldUnitsPerMetre()}},matrices+12);
}
void __cdecl FakeLeverAnimate(uint8_t* item) {
    ++leverNativeTicks;
    auto& a=*reinterpret_cast<int16_t*>(item+24);auto& f=*reinterpret_cast<int16_t*>(item+26);
    auto& state=*reinterpret_cast<int16_t*>(item+18);
    if(a==0) { a=2;f=2;state=0; }
    else if(a==1) { a=3;f=13;state=1; }
    else if(++f>leverAnimations[a].last) {
        f=leverAnimations[a].jumpFrame;a=leverAnimations[a].jump;
        *reinterpret_cast<uint32_t*>(item+0x1820)=5;
    }
}
void __cdecl FakeLeverControl(int16_t index) {
    Check(index==0,"native lever control receives grabbed index");
    ++leverControls;
    *reinterpret_cast<int16_t*>(switchMemory+44)|=0x3e00;
    FakeLeverAnimate(switchMemory);
}
int16_t* __cdecl FakeSwitchBounds(uint8_t*) { return leverBounds; }
int32_t __cdecl FakeSwitchPosition(const void* bounds,uint8_t* target,uint8_t* lara) {
    ++switchPositionCalls;
    if(!bounds) return 1;
    const auto& a=*reinterpret_cast<tr::PHD_3DPOS*>(target+tr::off::item_pos);
    const auto& b=*reinterpret_cast<tr::PHD_3DPOS*>(lara+tr::off::item_pos);
    const auto local=tr::locomotion::Rotate({float(b.x_pos-a.x_pos),float(b.z_pos-a.z_pos)},-tr::Radians(a.y_rot));
    const float p[3]={local.x,float(b.y_pos-a.y_pos),local.z};
    const auto* box=static_cast<const int16_t*>(bounds);
    for(int i=0;i<3;++i) if(p[i]<box[2*i]||p[i]>box[2*i+1]) return 0;
    return b.y_rot==a.y_rot;
}
void __cdecl FakeSwitchCollision(int16_t index,uint8_t* lara,void*) {
    using namespace tr;
    if (!(actionInput&0x40)) return;
    Check(index==0,"native switch receives selected index");
    const int before=switchPositionCalls;
    Check(Detour_SwitchTestPosition(switchBounds,switchMemory,lara)==1 &&
          Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,"physical commit satisfies native alignment");
    Check(before==switchPositionCalls,"physical commit bypasses native pose movement");
    ++switchActivations;
    *reinterpret_cast<int16_t*>(switchMemory+20)=1-*reinterpret_cast<int16_t*>(switchMemory+18);
    *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=3;
    FakeLeverAnimate(switchMemory);
    std::memset(lara+18,41,10);
    std::memset(laraMemory+224,0,12);
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;
    *reinterpret_cast<uint32_t*>(laraMemory+68)&=~0x20u;
}

template<class Reset,class Rva,class Pose>
void TestPhysicalSwitch(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
    table.collision=rva(reinterpret_cast<void*>(&FakeSwitchCollision));
    const auto savedMotion=g_motionDll;MotionDll motion{};
    motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;
    const auto savedFloor=dll.getFloor;dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));
    table.bounds=rva(switchBounds);table.getBounds=rva(reinterpret_cast<void*>(&FakeSwitchBounds));
    table.animate=rva(reinterpret_cast<void*>(&FakeLeverAnimate));
    table.calcMatrices=rva(reinterpret_cast<void*>(&FakeLeverMatrices));
    table.changes=rva(&leverChangePtr);table.ranges=rva(&leverRangePtr);table.commands=rva(&leverCommandPtr);
    table.clipTop=rva(leverClip);
    for(int i=0;i<4;++i) { leverAnimations[i]={};leverAnimations[i].frames=dummyLeverFrames;leverAnimations[i].interpolation=1; }
    leverAnimations[0].state=1;leverAnimations[0].changeCount=1;
    leverAnimations[1].first=leverAnimations[1].last=leverAnimations[1].jumpFrame=1;
    leverAnimations[1].jump=1;leverAnimations[1].changeCount=1;leverAnimations[1].changeIndex=1;
    leverAnimations[2].first=2;leverAnimations[2].last=12;leverAnimations[2].jump=1;
    leverAnimations[2].jumpFrame=1;leverAnimations[2].commandCount=1;
    leverAnimations[3].state=1;leverAnimations[3].first=13;leverAnimations[3].last=23;leverAnimations[3].commandCount=1;
    const auto savedAnims=dll.anims,savedObjects=dll.objects;
    dll.anims=rva(&leverAnimationPtr);dll.objects=rva(leverObject);
    *reinterpret_cast<int16_t*>(leverObject)=2;
    *reinterpret_cast<tr::Fn_SwitchControl*>(leverObject+24)=&FakeLeverControl;
    *reinterpret_cast<Fn_SwitchCollision*>(leverObject+56)=&FakeSwitchCollision;
    g_hSwitchCollision.m_trampoline=reinterpret_cast<void*>(&FakeSwitchCollision);
    g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
    const auto oldAnimation=*reinterpret_cast<int16_t*>(itemMemory+24);
    for(int which:{0,1}) for(int initial:{0,1}) {
        game=dll.game=which;reset();pose(0,0,0,0);FirstPersonRecenter();g_switchDll=&table;
        g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
        *reinterpret_cast<int16_t*>(itemMemory+24)=103;
        *reinterpret_cast<int16_t*>(laraMemory+2)=0;
        *reinterpret_cast<uint32_t*>(laraMemory+68)=0;
        std::memset(switchMemory,0,sizeof(switchMemory));
        *reinterpret_cast<int16_t*>(switchMemory+18)=int16_t(initial);
        *reinterpret_cast<int16_t*>(switchMemory+24)=*reinterpret_cast<int16_t*>(switchMemory+26)=int16_t(1-initial);
        VR().m_controllerPoseValid[1]=true;
        VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
        VR().m_rawControllerPose[1].m[1][3]-=.2f;
        motiongun::Vec hand{};float handY,headY,distance;
        Check(PhysicalSwitchReady()&&SwitchHand(hand,handY,headY),"physical switch has usable FP tracking");
        auto& target=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);
        target.x_pos=int(hand.x);target.z_pos=int(hand.z)+32;
        auto& body=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
        body.x_pos=target.x_pos;body.z_pos=target.z_pos-100;
        Check(SwitchReach(switchMemory,itemMemory,hand,distance,false),"nearby hand reaches wall lever");
        auto distantHand=hand;distantHand.x+=LiveWorldUnitsPerMetre();
        Check(!SwitchReach(switchMemory,itemMemory,distantHand,distance,false),"distant hand cannot grab");
        body.z_pos=target.z_pos+100;
        Check(!SwitchReach(switchMemory,itemMemory,hand,distance,false),"backside cannot grab through wall");
        body.z_pos=target.z_pos-100;
        body.x_pos+=1000;
        Check(!SwitchReach(switchMemory,itemMemory,hand,distance,false),"remote body cannot grab");
        body.x_pos=target.x_pos;
        const auto savedBody=body,savedTarget=target;
        for(int yaw:{0,0x4000,0x8000,0xc000}) {
            target.y_rot=int16_t(yaw);
            const auto world=[&](float x,float z) {
                return locomotion::Rotate({x,z},Radians(int16_t(yaw)));
            };
            const auto offset=world(450,-500),nearOffset=world(0,-32);
            body.x_pos=target.x_pos+int32_t(std::round(offset.x));
            body.z_pos=target.z_pos+int32_t(std::round(offset.z));
            body.y_rot=target.y_rot;
            const motiongun::Vec nearHand{target.x_pos+nearOffset.x,hand.y,target.z_pos+nearOffset.z};
            int16_t nativeZone[12];std::memcpy(nativeZone,switchBounds,sizeof(nativeZone));
            nativeZone[0]=-320;nativeZone[1]=320;nativeZone[4]=-232;nativeZone[5]=232;
            Check(!FakeSwitchPosition(nativeZone,switchMemory,itemMemory),"native Action zone rejects offset body pose");
            Check(SwitchReach(switchMemory,itemMemory,nearHand,distance,false),"hand reaches lever beyond Action-icon zone at each wall heading");
            ++body.y_pos;
            Check(!SwitchReach(switchMemory,itemMemory,nearHand,distance,false),"different floor height remains rejected");
            --body.y_pos;
        }
        body=savedBody;target=savedTarget;
        body.x_pos=target.x_pos+450;body.z_pos=target.z_pos-500;
        body.y_rot=Angle(1.2f);
        Check(SwitchReach(switchMemory,itemMemory,hand,distance,false)&&body.y_rot==Angle(1.2f),
              "tracked grab replaces facing without rotating Lara");
        *reinterpret_cast<int16_t*>(switchMemory+48)=4;
        Check(SwitchReach(switchMemory,itemMemory,hand,distance,false)==(which==1),"only TR5 type four is a bidirectional lever");
        *reinterpret_cast<int16_t*>(switchMemory+48)=2;
        Check(!SwitchReach(switchMemory,itemMemory,hand,distance,false),"special switch remains native");
        *reinterpret_cast<int16_t*>(switchMemory+48)=0;
        actionInput=0;Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        Check(g_switchCandidate.index==0,"native collision discovers nearby switch");
        g_switchCandidate={};
        switchSightBlocked=true;
        Check(FindPhysicalSwitch(itemMemory,hand,GetTickCount64()).index<0,"direct grab cannot cross a wall");
        switchSightBlocked=false;
        *reinterpret_cast<Fn_SwitchCollision*>(leverObject+56)=nullptr;
        Check(FindPhysicalSwitch(itemMemory,hand,GetTickCount64()).index<0,"direct grab ignores objects without ordinary switch handler");
        *reinterpret_cast<Fn_SwitchCollision*>(leverObject+56)=&FakeSwitchCollision;
        VRSystem::HandState hands[2]{};hands[1].valid=true;hands[1].grip=1;
        XState out{};bool shifted=false,chord=false;
        std::memset(leverClip,0x39,sizeof(leverClip));
        BuildStateFromHands(hands,out,shifted,true,chord);
        Check(g_switchGripClaimed&&!(out.Gamepad.wButtons&XB_X),"grab consumes Walk");
        Check(g_switchCandidate.index==0,"grip discovers lever without a collision candidate or Action icon");
        Check(g_switchPreview.active&&g_switchPreview.path.count==13,"grab builds native pose path without activation");
        Check(std::all_of(std::begin(leverClip),std::end(leverClip),[](uint8_t x){return x==0x39;}),
              "pose sampling preserves native viewport clipping");
        const auto originalController=VR().m_rawControllerPose[1];
        auto moveHand=[&](int poseIndex) {
            const auto delta=Sub(Sub(g_switchPreview.points[poseIndex],g_switchPreview.handOffset),hand);
            VR().m_rawControllerPose[1]=originalController;
            VR().m_rawControllerPose[1].m[0][3]+=delta.x/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[1].m[1][3]-=delta.y/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[1].m[2][3]-=delta.z/LiveWorldUnitsPerMetre();
        };
        const int ticksBefore=leverNativeTicks;
        const int controlsBefore=leverControls;
        moveHand(6);AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
        Check(g_switchPreview.selected==6&&!g_switchCompleted&&leverNativeTicks==ticksBefore,
              "half pull moves lever without native playback or commands");
        const auto heldFrame=*reinterpret_cast<int16_t*>(switchMemory+26);
        for(int i=0;i<6;++i) Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        Check(*reinterpret_cast<int16_t*>(switchMemory+26)==heldFrame,"stationary hand holds lever stationary");
        motiongun::Frame wrist{firstperson::IdentityBasis(),{20,30,40}};
        const auto wristBefore=wrist;
        const auto tracked=Add(g_switchPreview.grip,{5,6,7});
        AttachPhysicalSwitchHand(1,wrist,tracked);
        Check(Near(wrist.origin.x,wristBefore.origin.x-5)&&Near(wrist.origin.y,wristBefore.origin.y-6)&&
              Near(wrist.origin.z,wristBefore.origin.z-7),"Lara grip is attached to the displayed lever pose");
        moveHand(3);Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        Check(g_switchPreview.selected==3&&!g_switchCompleted,"reversing hand reverses lever before completion");
        const auto parkedFrame=*reinterpret_cast<int16_t*>(switchMemory+26);
        hands[1].grip=0;BuildStateFromHands(hands,out,shifted,true,chord);
        Check(!g_switchPreview.active&&!g_switchPreview.attached&&leverNativeTicks==ticksBefore&&
              *reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame && g_parkedSwitches.size()==1,
              "release parks wall switch without activation or snap-back");
        *reinterpret_cast<int16_t*>(itemMemory+18)=1;
        BuildStateFromHands(hands,out,shifted,true,chord);
        Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        Check(*reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame,"walking after release leaves wall switch at its partial position");
        *reinterpret_cast<int16_t*>(itemMemory+18)=2;
        VR().m_rawControllerPose[1]=originalController;
        Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        hands[1].grip=1;BuildStateFromHands(hands,out,shifted,true,chord);
        Check(g_switchPreview.active && g_switchPreview.selected==3 && g_parkedSwitches.empty(),"wall switch resumes same partial pose after regrabbing");
        for(int cancel:{0,1,2}) {
            moveHand(4);Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
            const auto beforeCancel=*reinterpret_cast<int16_t*>(switchMemory+26);
            if(cancel==0) { VR().m_controllerPoseValid[1]=false;Detour_PhysicalSwitchCollision(0,itemMemory,nullptr); }
            if(cancel==1) UpdatePhysicalSwitchGrip(true,true,g_switchPreview.lastTime+251);
            if(cancel==2) BuildStateFromHands(hands,out,shifted,false,chord);
            Check(!g_switchPreview.active&&!g_switchPreview.attached&&leverNativeTicks==ticksBefore&&
                *reinterpret_cast<int16_t*>(switchMemory+26)==beforeCancel,
                "tracking loss, stale samples and lost input eligibility park the unactivated lever");
            VR().m_controllerPoseValid[1]=true;VR().m_rawControllerPose[1]=originalController;
            hands[1].grip=0;BuildStateFromHands(hands,out,shifted,true,chord);
            Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
            hands[1].grip=1;BuildStateFromHands(hands,out,shifted,true,chord);
            Check(g_switchPreview.active,"cancelled lever requires and accepts a fresh grip press");
        }
        uint8_t savedItem[sizeof(itemMemory)],savedLara[sizeof(laraMemory)];
        std::memcpy(savedItem,itemMemory,sizeof(savedItem));std::memcpy(savedLara,laraMemory,sizeof(savedLara));
        const auto savedCamera=g_scenePose;
        moveHand(g_switchPreview.path.count-1);
        const int previous=switchActivations;actionInput=0x800;
        AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
        Check(switchActivations==previous+1&&g_switchCompleted,"simulation tick completes pull without object collision or Action icon");
        Check(*reinterpret_cast<int16_t*>(switchMemory+20)==1-initial,"lever requests opposite native state");
        Check((*reinterpret_cast<uint32_t*>(switchMemory+0x1820)&6)==4&&
              leverNativeTicks==ticksBefore+12&&!g_switchPreview.active,
              "completion runs native commands once and skips timed playback");
        Check(leverControls==controlsBefore+1&&(*reinterpret_cast<int16_t*>(switchMemory+44)&0x3e00)==0x3e00,
              "completion retains native switch code bits without accelerating the timer");
        Check(!std::memcmp(savedItem,itemMemory,sizeof(savedItem))&&!std::memcmp(savedLara,laraMemory,sizeof(savedLara)),
              "native activation leaves Lara pose and animation intact");
        Check(actionInput==0x800&&Gate()&&!std::memcmp(&savedCamera,&g_scenePose,sizeof(savedCamera)),
              "native activation retains first-person camera and input");
        Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        Check(switchActivations==previous+1,"held grip does not retrigger");
        Check(UpdatePhysicalSwitchGrip(true,true,GetTickCount64()+1000),"completed grab consumes Walk until release");
        hands[1].grip=0;BuildStateFromHands(hands,out,shifted,true,chord);
        Check(!g_switchGripClaimed,"release ends grab");
        g_switchCandidate={};hands[1].grip=1;BuildStateFromHands(hands,out,shifted,true,chord);
        Check((out.Gamepad.wButtons&XB_X)!=0,"Right Grip retains Walk away from switches");
        hands[0].grip=1;BuildStateFromHands(hands,out,shifted,true,chord);
        Check(chord&&!g_switchGripClaimed,"dual-grip Action retains priority");
        const auto calls=switchPositionCalls;
        Detour_SwitchMovePosition(nullptr,switchMemory,itemMemory);
        Check(switchPositionCalls==calls+1,"ordinary native alignment remains enabled");
    }
    ResetPhysicalSwitch();g_switchDll=nullptr;
    g_hSwitchCollision.m_trampoline=g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=nullptr;
    *reinterpret_cast<int16_t*>(itemMemory+24)=oldAnimation;
    dll.anims=savedAnims;dll.objects=savedObjects;
    dll.getFloor=savedFloor;g_motionDll=savedMotion;
    actionInput=0;reset();
}

// The train floor lever has the same endpoint height in both positions, a
// long horizontal sweep, and a different native handler from a wall lever.
int railCalls=0;
void __cdecl FakeRailMatrices(uint8_t* item) {
    using namespace tr;
    const int a=*reinterpret_cast<int16_t*>(item+24),f=*reinterpret_cast<int16_t*>(item+26);
    const float progress=a==0 ? 0.f : a==2 ? 1.f : a==1 ? (f-1)/94.f : 1-(f-97)/94.f;
    const float angle=-kPi/4-progress*kPi/2,c=std::cos(angle),s=std::sin(angle);
    const float yaw=Radians(reinterpret_cast<PHD_3DPOS*>(item+off::item_pos)->y_rot);
    const float cy=std::cos(yaw),sy=std::sin(yaw);
    motiongun::Basis basis{{{cy,sy*s,sy*c},{0,c,-s},{-sy,cy*s,cy*c}}};
    auto* matrices=reinterpret_cast<float*>(item+7832);
    motiongun::WriteRows({firstperson::IdentityBasis(),{}},matrices);
    motiongun::WriteRows({basis,{}},matrices+12);
}
void __cdecl FakeRailAnimate(uint8_t* target) {
    ++leverNativeTicks;
    auto& a=*reinterpret_cast<int16_t*>(target+24);auto& f=*reinterpret_cast<int16_t*>(target+26);
    auto& state=*reinterpret_cast<int16_t*>(target+18);
    if(a==0) { a=1;f=1;state=2; }
    else if(a==2) { a=3;f=97;state=2; }
    else if(++f>leverAnimations[a].last) {
        f=leverAnimations[a].jumpFrame;a=leverAnimations[a].jump;state=leverAnimations[a].state;
        *reinterpret_cast<uint32_t*>(target+0x1820)=5;
    }
}
void __cdecl FakeRailControl(int16_t index) {
    Check(index==0,"floor lever control receives selected item");++leverControls;
    *reinterpret_cast<uint16_t*>(switchMemory+44)|=0x3e00;
    FakeRailAnimate(switchMemory);
}
void __cdecl FakeRailCollision(int16_t index,uint8_t* lara,void*) {
    using namespace tr;++railCalls;
    if (!(actionInput&0x40)) return;
    Check(index==0 && *reinterpret_cast<int16_t*>(lara+18)==2 &&
        *reinterpret_cast<int16_t*>(lara+24)==103,"floor commit satisfies native standing precondition temporarily");
    Check(Detour_SwitchTestPosition(switchBounds,switchMemory,lara)==1 &&
        Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,"floor commit skips native auto-alignment");
    // The reverse native branch temporarily flips Lara by half a turn.
    auto& yaw=reinterpret_cast<PHD_3DPOS*>(lara+off::item_pos)->y_rot;
    if(*reinterpret_cast<int16_t*>(switchMemory+18)==1) { yaw^=int16_t(0x8000);yaw^=int16_t(0x8000); }
    ++switchActivations;
    *reinterpret_cast<int16_t*>(switchMemory+20)=1-*reinterpret_cast<int16_t*>(switchMemory+18);
    *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=3;FakeRailAnimate(switchMemory);
    std::memset(lara+18,41,10);*reinterpret_cast<int16_t*>(laraMemory+2)=1;
}
template<class Reset,class Rva,class Pose>
void TestPhysicalRailSwitch(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
    table.collision=rva(reinterpret_cast<void*>(&FakeSwitchCollision));
    table.bounds=rva(switchBounds);table.getBounds=rva(reinterpret_cast<void*>(&FakeSwitchBounds));
    table.animate=rva(reinterpret_cast<void*>(&FakeRailAnimate));table.calcMatrices=rva(reinterpret_cast<void*>(&FakeRailMatrices));
    table.changes=rva(&leverChangePtr);table.ranges=rva(&leverRangePtr);table.commands=rva(&leverCommandPtr);
    MotionDll motion{};motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));
    g_motionDll=&motion;dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));
    dll.anims=rva(&leverAnimationPtr);dll.objects=rva(leverObject);
    auto* object=leverObject+318*off::object_stride;
    *reinterpret_cast<int16_t*>(object)=2;
    *reinterpret_cast<Fn_SwitchControl*>(object+24)=FakeRailControl;
    *reinterpret_cast<Fn_SwitchCollision*>(object+56)=FakeRailCollision;
    // Captured from Desert Railroad: the render path applies this nonidentity
    // inverse bind to mesh vertices after CalcItemMatrices has posed joint 1.
    auto* railBind=reinterpret_cast<float*>(object+1676)+16;
    for(int i=0;i<16;++i) railBind[i]=i%5==0 ? 1.f : 0.f;
    railBind[13]=89;railBind[14]=78;
    motiongun::Vec railLocal{};
    Check(RailHandleJointLocal(object,railLocal) && Near(railLocal.x,0) && Near(railLocal.y,1) && Near(railLocal.z,-622),
          "train mesh contact is transformed through the captured native inverse bind");
    const float liveJoint[12]={-.742798388f,-.006661505f,.669482052f,1.546087503f,
        .669496298f,.000128276f,.742815435f,-78.f,-.005034148f,.999977767f,.004364570f,.001678467f};
    const auto liveContact=motiongun::Transform(motiongun::ReadRows(liveJoint),railLocal);
    Check(firstperson::Length(Sub(liveContact,{-414.8784f,-540.0311f,-1.7131f}))<.01f,
          "handle contact matches native skinning in the live train pose");
    auto* palmBind=reinterpret_cast<float*>(leverObject+1676)+10*16;
    for(int i=0;i<16;++i) palmBind[i]=i%5==0 ? 1.f : 0.f;
    palmBind[12]=-69;palmBind[13]=-5;palmBind[14]=4;
    for(int i=0;i<4;++i) { leverAnimations[i]={};leverAnimations[i].frames=dummyLeverFrames;leverAnimations[i].interpolation=1; }
    leverAnimations[0].changeCount=1;
    leverAnimations[1].state=2;leverAnimations[1].first=1;leverAnimations[1].last=95;
    leverAnimations[1].jump=2;leverAnimations[1].jumpFrame=96;leverAnimations[1].commandCount=1;
    leverAnimations[2].state=1;leverAnimations[2].first=leverAnimations[2].last=96;
    leverAnimations[2].jump=2;leverAnimations[2].jumpFrame=96;leverAnimations[2].changeCount=1;leverAnimations[2].changeIndex=1;
    leverAnimations[3].state=2;leverAnimations[3].first=97;leverAnimations[3].last=191;leverAnimations[3].commandCount=1;
    leverChanges[0]={1,1,0};leverChanges[1]={0,1,1};
    leverRanges[0]={0,1,1,1};leverRanges[1]={96,97,3,97};
    g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
    g_hRailSwitchCollision.m_trampoline=reinterpret_cast<void*>(&FakeRailCollision);
    g_hSwitchCollision.m_trampoline=reinterpret_cast<void*>(&FakeSwitchCollision);
    for(int initial:{0,1}) for(int stance:{2,71,80}) for(int heading:{0,0x4000,0x8000,0xc000}) {
        game=dll.game=0;reset();pose(0,0,0,0);FirstPersonRecenter();g_switchDll=&table;
        g_railSwitchCollision=rva(reinterpret_cast<void*>(&FakeRailCollision));
        g_scenePose={0,-700,-200,0,0,0,0};g_scenePoseValid=true;
        *reinterpret_cast<int16_t*>(itemMemory+18)=int16_t(stance);
        *reinterpret_cast<int16_t*>(itemMemory+24)=int16_t(stance==2 ? 103 : 222);
        auto& body=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);body.y_rot=Angle(1.2f);
        std::memset(switchMemory,0,sizeof(switchMemory));
        *reinterpret_cast<int16_t*>(switchMemory+16)=318;*reinterpret_cast<int16_t*>(switchMemory+18)=int16_t(initial);
        *reinterpret_cast<int16_t*>(switchMemory+24)=int16_t(initial*2);*reinterpret_cast<int16_t*>(switchMemory+26)=int16_t(initial*96);
        reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos)->y_rot=int16_t(heading);
        const auto offset=locomotion::Rotate({0,initial ? 600.f : -600.f},Radians(int16_t(heading)));
        body.x_pos=int32_t(std::round(offset.x));body.z_pos=int32_t(std::round(offset.z));
        VR().m_controllerPoseValid[1]=true;VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
        const auto place=[&](motiongun::Vec desired) {
            motiongun::Vec current{};float hy,ey;Check(SwitchHand(current,hy,ey),"floor hand tracking valid");
            const auto d=Sub(desired,current);auto& raw=VR().m_rawControllerPose[1];
            raw.m[0][3]+=d.x/1000;raw.m[1][3]-=d.y/1000;raw.m[2][3]-=d.z/1000;
        };
        motiongun::Vec grip{};Check(RailSwitchGripPoint(switchMemory,grip),"floor handle endpoint is available");place(grip);
        float distance=0;Check(SwitchReach(switchMemory,itemMemory,grip,distance,false),"floor lever reachable standing or crouched from either end");
        Check(!SwitchReach(switchMemory,itemMemory,{0,0,0},distance,false),"floor base is not the handle grip");
        *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=8;
        Check(!SwitchReach(switchMemory,itemMemory,grip,distance,false),"airborne Lara cannot activate floor lever");
        *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=0;
        const auto grab=[&]() {
            UpdatePhysicalSwitchGrip(false,true,GetTickCount64());g_switchCandidate={};
            motiongun::Vec currentGrip{};Check(RailSwitchGripPoint(switchMemory,currentGrip),"current floor grip is available for regrab");place(currentGrip);
            Check(UpdatePhysicalSwitchGrip(true,true,GetTickCount64()),"RG directly grabs train floor lever without icon");
            Check(g_switchPreview.joint==1 && g_switchPreview.path.count==97,"train floor lever samples complete authored arc");
        };
        const int before=switchActivations,ticks=leverNativeTicks;
        grab();place(g_switchPreview.points[20]);AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
        Check(g_switchPreview.selected>0 && switchActivations==before && leverNativeTicks==ticks,"hand drives floor lever without early activation");
        const auto parkedFrame=*reinterpret_cast<int16_t*>(switchMemory+26);
        UpdatePhysicalSwitchGrip(false,true,GetTickCount64());
        Check(*reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame && switchActivations==before && g_parkedSwitches.size()==1,"floor release retains partial progress");
        *reinterpret_cast<int16_t*>(itemMemory+18)=1;
        const int callsBefore=railCalls;Detour_PhysicalRailCollision(0,itemMemory,nullptr);
        UpdatePhysicalSwitchGrip(false,true,GetTickCount64());
        Check(railCalls==callsBefore+1 && *reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame,"moving after release preserves floor lever pose");
        const auto paused=g_parkedSwitches.front();
        ++*reinterpret_cast<int16_t*>(switchMemory+26);
        RestoreParkedSwitches();
        Check(*reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame+1,"cleanup does not overwrite a script-modified lever");
        *reinterpret_cast<int16_t*>(switchMemory+26)=parkedFrame;g_parkedSwitches.push_back(paused);
        *reinterpret_cast<int16_t*>(itemMemory+18)=int16_t(stance);
        motiongun::Vec partialGrip{};Check(RailSwitchGripPoint(switchMemory,partialGrip),"partial handle is available");
        place(Add(partialGrip,{75,0,0}));
        Check(UpdatePhysicalSwitchGrip(true,true,GetTickCount64()),"slightly offset regrab attaches to retained handle");
        AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
        Check(g_switchPreview.selected==20,"regrabbing without moving cannot advance or rewind the lever");
        UpdatePhysicalSwitchGrip(false,true,GetTickCount64());
        grab();uint8_t savedItem[sizeof(itemMemory)],savedLara[sizeof(laraMemory)];
        Check(g_switchPreview.selected==20 && g_parkedSwitches.empty(),"floor regrab resumes at the retained pose");
        // Reach uses the same visible palm as rendering, before attachment.
        motiongun::Frame freeWrist{};GunBasis freeBasis{};motiongun::Vec palm{},rawGrip{};
        Check(TrackedControllerWrist(1,freeWrist,freeBasis,&rawGrip) && SwitchPalmLocal(palm),"unattached palm transform available");
        motiongun::Vec reachPalm{};float hy,ey;
        Check(SwitchHand(reachPalm,hy,ey) && firstperson::Length(Sub(reachPalm,motiongun::Transform(freeWrist,{-7,35,4})))<.001f,
              "grab target matches visible palm with real wrist bind and controller calibration");
        Check(firstperson::Length(Sub(reachPalm,motiongun::Transform(freeWrist,rawGrip)))>100,
              "test detects displaced raw controller origin instead of accidentally using it");
        motiongun::Frame wrist{firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},.7f)),{100,200,300}};
        AttachPhysicalSwitchHand(1,wrist,{400,500,600});
        const auto visiblePalm=motiongun::Transform(wrist,{-7,35,4});
        Check(firstperson::Length(Sub(visiblePalm,g_switchPreview.grip))<.001f,"visible hand palm contacts lever independently of gun/controller offset");
        const auto right=wrist;AttachPhysicalSwitchHand(0,wrist,{1,2,3});
        Check(!std::memcmp(&right,&wrist,sizeof(wrist)),"unheld left hand remains unchanged");
        std::memcpy(savedItem,itemMemory,sizeof(savedItem));std::memcpy(savedLara,laraMemory,sizeof(savedLara));
        for(int cancel:{0,1,2}) {
            const auto held=*reinterpret_cast<int16_t*>(switchMemory+26);
            if(cancel==0) {
                const auto side=locomotion::Rotate({300,0},Radians(int16_t(heading)));
                place(Add(g_switchPreview.grip,{side.x,0,side.z}));
                AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
            } else if(cancel==1) {
                *reinterpret_cast<int16_t*>(itemMemory+18)=1;
                AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
                *reinterpret_cast<int16_t*>(itemMemory+18)=int16_t(stance);
            } else {
                UpdateSwitchPreview(switchMemory,Add(g_switchPreview.lastHand,{500,0,0}),GetTickCount64());
            }
            Check(!g_switchGripClaimed && !g_switchPreview.attached && g_parkedSwitches.size()==1 &&
                *reinterpret_cast<int16_t*>(switchMemory+26)==held && switchActivations==before,
                "off-arc pulls, moving Lara and tracking jumps retain lever progress without activation");
            grab();Check(g_switchPreview.selected==20,"fresh grip resumes after an off-arc release");
        }
        for(int frame=24;frame<97;frame+=4) {
            place(g_switchPreview.points[frame]);AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
            if(frame==40 || frame==60) {
                UpdatePhysicalSwitchGrip(false,true,GetTickCount64());
                Check(!g_switchGripClaimed && g_parkedSwitches.size()==1,"multiple releases free the controller for movement");
                grab();Check(g_switchPreview.selected==frame && switchActivations==before,"each regrab preserves accumulated progress without activation");
            }
        }
        Check(g_switchCompleted && switchActivations==before+1,"full floor arc activates native rail switch once");
        Check(*reinterpret_cast<int16_t*>(switchMemory+18)==1-initial,"native floor lever reaches opposite state");
        Check(!std::memcmp(savedItem,itemMemory,sizeof(savedItem)) && !std::memcmp(savedLara,laraMemory,sizeof(savedLara)),"floor activation restores crouch, facing and all Lara state");
        AdvancePhysicalSwitch(itemMemory,nullptr,GetTickCount64());
        Check(switchActivations==before+1,"held floor grip cannot repeat activation");
        UpdatePhysicalSwitchGrip(false,true,GetTickCount64());
        const int calls=railCalls;g_active=false;actionInput=0;
        Detour_PhysicalRailCollision(0,itemMemory,nullptr);
        Check(railCalls==calls+1,"third-person floor collision passes through unchanged");
        ResetPhysicalSwitch();g_switchDll=nullptr;
    }
    g_railSwitchCollision=0;g_motionDll=nullptr;
    g_hRailSwitchCollision.m_trampoline=g_hSwitchCollision.m_trampoline=nullptr;
    g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=nullptr;
}
}
