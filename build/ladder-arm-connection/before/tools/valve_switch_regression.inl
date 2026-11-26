namespace {
tr::firstperson::SwitchAnim valveAnimations[3]{};
tr::firstperson::SwitchAnim* valveAnimationPtr=valveAnimations;
int valveActivations=0,valveLinkedTriggers=0,valveControlCalls=0;
void __cdecl FakeValveMatrices(uint8_t* item) {
    using namespace tr;
    const int animation=*reinterpret_cast<int16_t*>(item+24),frame=*reinterpret_cast<int16_t*>(item+26);
    const float angle=animation==0 ? 0.f : -std::clamp((frame-70)/80.f,0.f,1.f)*5.21f;
    const float c=std::cos(angle),s=std::sin(angle);
    motiongun::WriteRows({{{{c,-s,0},{s,c,0},{0,0,1}}},{-18,-491,436}},reinterpret_cast<float*>(item+7832));
}
void __cdecl FakeValveAnimate(uint8_t* item) {
    auto& a=*reinterpret_cast<int16_t*>(item+24);auto& f=*reinterpret_cast<int16_t*>(item+26);
    if(a==0){a=1;f=1;*reinterpret_cast<int16_t*>(item+18)=1;}
    else if(a==1 && ++f>166){a=2;f=167;}
}
void __cdecl FakeValveControl(int16_t) {
    ++valveControlCalls;
    if(*reinterpret_cast<int16_t*>(switchMemory+24)==1 && *reinterpret_cast<int16_t*>(switchMemory+26)==165) ++valveLinkedTriggers;
    FakeValveAnimate(switchMemory);
}
void __cdecl FakeValveCollision(int16_t,uint8_t* lara,void*) {
    using namespace tr;
    if(!(actionInput&0x40)) return;
    Check(*reinterpret_cast<int16_t*>(lara+18)==2 && *reinterpret_cast<int16_t*>(lara+24)==103,"valve commit supplies native standing precondition");
    Check(Detour_SwitchTestPosition(switchBounds,switchMemory,lara)==1 &&
          Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,"valve commit bypasses icon alignment without moving Lara");
    ++valveActivations;*reinterpret_cast<uint32_t*>(switchMemory+0x1820)=3;
    *reinterpret_cast<int16_t*>(switchMemory+20)=1;FakeValveAnimate(switchMemory);
    *reinterpret_cast<int16_t*>(lara+18)=40;*reinterpret_cast<int16_t*>(lara+24)=470;
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;
}
template<class Reset,class Rva,class Pose>
void TestPhysicalValve(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    game=dll.game=1;reset();pose(0,0,0,0);FirstPersonRecenter();
    SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
    table.collision=rva(reinterpret_cast<void*>(&FakeValveCollision));
    table.bounds=rva(switchBounds);table.getBounds=rva(reinterpret_cast<void*>(&FakeSwitchBounds));
    table.animate=rva(reinterpret_cast<void*>(&FakeValveAnimate));table.calcMatrices=rva(reinterpret_cast<void*>(&FakeValveMatrices));
    table.changes=rva(&leverChangePtr);table.ranges=rva(&leverRangePtr);table.commands=rva(&leverCommandPtr);
    MotionDll motion{};motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;
    dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));dll.objects=rva(leverObject);dll.anims=rva(&valveAnimationPtr);
    g_switchDll=&table;g_hSwitchCollision.m_trampoline=reinterpret_cast<void*>(&FakeValveCollision);
    g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
    std::memset(leverObject,0,sizeof(leverObject));std::memset(switchMemory,0,sizeof(switchMemory));
    auto* object=leverObject+274*3792;*reinterpret_cast<int16_t*>(object)=1;
    *reinterpret_cast<Fn_SwitchControl*>(object+24)=&FakeValveControl;
    *reinterpret_cast<Fn_SwitchCollision*>(object+56)=&FakeValveCollision;
    *reinterpret_cast<const char**>(object+off::object_geom)="AIRLOCK_SWITCH_ANDREA2";
    auto* bind=reinterpret_cast<float*>(object+1676);bind[0]=bind[5]=bind[10]=bind[15]=1;
    *reinterpret_cast<int16_t*>(switchMemory+16)=274;*reinterpret_cast<int16_t*>(switchMemory+48)=10;
    for(auto& a:valveAnimations){a={};a.frames=dummyLeverFrames;a.interpolation=1;}
    valveAnimations[0].jump=1;valveAnimations[0].jumpFrame=1;
    valveAnimations[1].state=1;valveAnimations[1].first=1;valveAnimations[1].last=166;
    valveAnimations[1].jump=2;valveAnimations[1].jumpFrame=167;
    valveAnimations[2].state=1;valveAnimations[2].first=valveAnimations[2].last=valveAnimations[2].jumpFrame=167;valveAnimations[2].jump=2;
    const int16_t bounds[6]={-147,117,-599,-370,340,439};std::memcpy(leverBounds,bounds,sizeof(bounds));
    g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
    *reinterpret_cast<int16_t*>(itemMemory+24)=103;
    VR().m_controllerPoseValid[1]=true;VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
    VR().m_rawControllerPose[1].m[1][3]-=.2f;
    motiongun::Vec hand{};float handY=0,headY=0,distance=0;
    Check(SwitchHand(hand,handY,headY),"valve has controller tracking");
    auto& target=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);
    target.x_pos=int32_t(std::lround(hand.x-97));target.y_pos=int32_t(std::lround(hand.y+484.6f));target.z_pos=int32_t(std::lround(hand.z-353));
    *reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)=target;
    Check(IsPhysicalValve(switchMemory) && SwitchReach(switchMemory,itemMemory,hand,distance,false),"Trajan valve rim is physically reachable");
    const auto originalController=VR().m_rawControllerPose[1];const auto originalHand=hand;
    auto setHand=[&](motiongun::Vec world) {
        const auto delta=Sub(world,originalHand);VR().m_rawControllerPose[1]=originalController;
        VR().m_rawControllerPose[1].m[0][3]+=delta.x/LiveWorldUnitsPerMetre();
        VR().m_rawControllerPose[1].m[1][3]-=delta.y/LiveWorldUnitsPerMetre();
        VR().m_rawControllerPose[1].m[2][3]-=delta.z/LiveWorldUnitsPerMetre();
    };
    const auto now=GetTickCount64();
    Check(UpdatePhysicalSwitchGrip(true,true,now),"RG discovers valve independently of Action icon");
    Check(g_switchPreview.valve && g_switchPreview.path.count==168,"RG selects circular valve path");
    Check(!AdvancePhysicalSwitch(itemMemory,nullptr,now) && !valveActivations,"holding still cannot activate valve");
    auto turnTo=[&](float progress) {
        auto& p=g_switchPreview;const float a=progress*p.valveTurn.direction,c=std::cos(a),s=std::sin(a);
        const auto v=p.valveLocal;
        setHand(Sub(motiongun::Transform(p.valveFrame,{c*v.x-s*v.y,s*v.x+c*v.y,v.z}),p.handOffset));
        AdvancePhysicalSwitch(itemMemory,nullptr,now);
    };
    turnTo(-.12f);turnTo(0);
    Check(g_switchPreview.valveTurn.progress<.001f && !valveActivations,"start-stop return does not create a rotation");
    for(int i=1;i<=25;++i)turnTo(i*.06f);
    Check(g_switchPreview.valveTurn.progress>1.49f && !valveActivations,"partial turn displays without firing native mechanism");
    const auto parkedFrame=*reinterpret_cast<int16_t*>(switchMemory+26);const auto progress=g_switchPreview.valveTurn.progress;
    UpdatePhysicalSwitchGrip(false,true,now);
    Check(g_parkedSwitches.size()==1 && *reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame,"release keeps valve position");
    FakeValveMatrices(switchMemory);auto frame=motiongun::ReadRows(reinterpret_cast<float*>(switchMemory+7832));
    frame.origin=Add(frame.origin,{float(target.x_pos),float(target.y_pos),float(target.z_pos)});
    setHand(motiongun::Transform(frame,{-109,6.4f,-83}));
    Check(UpdatePhysicalSwitchGrip(true,true,now),"regrab different point on rotated rim");
    Check(std::fabs(g_switchPreview.valveTurn.progress-progress)<.002f && g_parkedSwitches.empty(),"new rim grip retains circular progress");
    Check(std::fabs(g_switchPreview.valveLocal.x+109)<1.f,"new visible rim point becomes attachment");
    for(float p=progress+.06f;p<5.21f;p+=.06f) turnTo(p);
    turnTo(5.21f);
    Check(g_switchCompleted && valveActivations==1 && valveLinkedTriggers==1 && valveControlCalls==166,"full turn fires native valve handler and linked trigger once");
    Check(*reinterpret_cast<int16_t*>(itemMemory+18)==2 && *reinterpret_cast<int16_t*>(itemMemory+24)==103 &&
        *reinterpret_cast<int16_t*>(laraMemory+2)==0 && !g_switchPreview.attached,"completion restores Lara and releases hand before native post-use motion");
    for(int i=0;i<5;++i)AdvancePhysicalSwitch(itemMemory,nullptr,now);
    Check(valveLinkedTriggers==1,"holding after completion cannot retrigger valve");
    UpdatePhysicalSwitchGrip(false,true,now);ResetPhysicalSwitch();g_switchDll=nullptr;g_motionDll=nullptr;
}
}
