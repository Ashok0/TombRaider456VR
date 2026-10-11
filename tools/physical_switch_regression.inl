namespace {
alignas(16) uint8_t switchMemory[9416]{};
uint8_t* switchItems=switchMemory;
int switchCount=1,switchActivations=0,switchPositionCalls=0;
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
alignas(16) uint8_t leverObject[3792]{};
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
        Check(!SwitchReach(switchMemory,itemMemory,hand,distance,false),"native body reach is preserved");
        body.x_pos=target.x_pos;
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
        VRSystem::HandState hands[2]{};hands[1].valid=true;hands[1].grip=1;
        XState out{};bool shifted=false,chord=false;
        std::memset(leverClip,0x39,sizeof(leverClip));
        BuildStateFromHands(hands,out,shifted,true,chord);
        Check(g_switchGripClaimed&&!(out.Gamepad.wButtons&XB_X),"grab consumes Walk");
        Check(g_switchPreview.active&&g_switchPreview.path.count==13,"grab builds native pose path without activation");
        Check(std::all_of(std::begin(leverClip),std::end(leverClip),[](uint8_t x){return x==0x39;}),
              "pose sampling preserves native viewport clipping");
        const auto originalController=VR().m_rawControllerPose[1];
        auto moveHand=[&](int poseIndex) {
            const auto delta=Sub(g_switchPreview.points[poseIndex],hand);
            VR().m_rawControllerPose[1]=originalController;
            VR().m_rawControllerPose[1].m[0][3]+=delta.x/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[1].m[1][3]-=delta.y/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[1].m[2][3]-=delta.z/LiveWorldUnitsPerMetre();
        };
        const int ticksBefore=leverNativeTicks;
        const int controlsBefore=leverControls;
        moveHand(6);Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
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
        hands[1].grip=0;BuildStateFromHands(hands,out,shifted,true,chord);
        Check(!g_switchPreview.active&&!g_switchPreview.attached&&leverNativeTicks==ticksBefore&&
              *reinterpret_cast<int16_t*>(switchMemory+24)==1-initial,"early release restores idle without activation");
        VR().m_rawControllerPose[1]=originalController;
        Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        hands[1].grip=1;BuildStateFromHands(hands,out,shifted,true,chord);
        Check(g_switchPreview.active,"lever can be grabbed again after cancellation");
        for(int cancel:{0,1,2}) {
            moveHand(4);Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
            if(cancel==0) { VR().m_controllerPoseValid[1]=false;Detour_PhysicalSwitchCollision(0,itemMemory,nullptr); }
            if(cancel==1) UpdatePhysicalSwitchGrip(true,true,g_switchPreview.lastTime+251);
            if(cancel==2) BuildStateFromHands(hands,out,shifted,false,chord);
            Check(!g_switchPreview.active&&!g_switchPreview.attached&&leverNativeTicks==ticksBefore&&
                *reinterpret_cast<int16_t*>(switchMemory+24)==1-initial,
                "tracking loss, stale samples and menus restore the unactivated lever");
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
        Detour_PhysicalSwitchCollision(0,itemMemory,nullptr);
        Check(switchActivations==previous+1&&g_switchCompleted,"physical movement invokes native activation once");
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
    actionInput=0;reset();
}
}
