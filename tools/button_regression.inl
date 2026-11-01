namespace {
alignas(16) uint8_t buttonObjects[344*3792]{};
uint8_t buttonSequence=0;
int buttonActivations=0,buttonSequenceActivations=0;
float buttonRoot[3]{},buttonBone[3]{};
bool buttonReject=false;
void __cdecl FakeButtonMatrices(uint8_t* item) {
    using namespace tr;
    const float angle=Radians(reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot);
    const float c=std::cos(angle),s=std::sin(angle);
    motiongun::Frame frame{{{{c,0,s},{0,1,0},{-s,0,c}}},{}};
    frame.origin=Transform(frame.basis,{buttonRoot[0],buttonRoot[1],buttonRoot[2]});
    motiongun::WriteRows(frame,reinterpret_cast<float*>(item+7832));
    frame.origin=Add(frame.origin,Transform(frame.basis,{buttonBone[0],buttonBone[1],buttonBone[2]}));
    motiongun::WriteRows(frame,reinterpret_cast<float*>(item+7832)+12);
}
void FakeButtonActivate(uint8_t* lara,bool sequence) {
    using namespace tr;
    Check((actionInput&0x40) && *reinterpret_cast<int16_t*>(lara+18)==2 && *reinterpret_cast<int16_t*>(lara+24)==103,
        "button supplies native standing/Action conditions");
    Check(Detour_SwitchTestPosition(nullptr,switchMemory,lara)==1 && Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,
        "touch bypasses only icon/alignment");
    if(buttonReject || (sequence && buttonSequence>=3)) return;
    ++buttonActivations;if(sequence)++buttonSequenceActivations;
    *reinterpret_cast<int16_t*>(switchMemory+20)=1-*reinterpret_cast<int16_t*>(switchMemory+18);
    *reinterpret_cast<uint32_t*>(switchMemory+0x1820)|=3;
    *reinterpret_cast<int16_t*>(lara+18)=40;*reinterpret_cast<int16_t*>(lara+24)=197;
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;*reinterpret_cast<int32_t*>(laraMemory+224)=42;
}
void __cdecl FakeButtonCollision(int16_t,uint8_t* lara,void*) { FakeButtonActivate(lara,false); }
void __cdecl FakeSequenceCollision(int16_t,uint8_t* lara,void*) { FakeButtonActivate(lara,true); }
template<class Reset,class Rva,class Pose>
void TestPhysicalButton(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    FILE* file=nullptr;fopen_s(&file,"build/button-poses.txt","r");Check(file!=nullptr,"installed button pose fixtures available");
    struct Asset {int game,slot,joints,state;float root[3],bone[3];} assets[21]{};
    for(int i=0;i<18;++i) {
        auto& a=assets[i];Check(fscanf_s(file,"%d%d%d%d%f%f%f%f%f%f",&a.game,&a.slot,&a.joints,&a.state,&a.root[0],&a.root[1],&a.root[2],&a.bone[0],&a.bone[1],&a.bone[2])==10,"read actual root/joint offsets");
    }
    std::fclose(file);
    for(int i=18;i<21;++i) { assets[i]=assets[6+i-18];assets[i].game=5;assets[i].slot=329+i-18; }
    for(int p=0;p<21;++p) for(int hand=0;hand<2;++hand) for(int yaw=0;yaw<4;++yaw) {
        const auto& profile=firstperson::ButtonProfiles[p];const auto& asset=assets[p];
        game=dll.game=profile.game;reset();pose(0,0,0,0);FirstPersonRecenter();
        SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
        table.calcMatrices=rva(reinterpret_cast<void*>(&FakeButtonMatrices));table.collision=rva(reinterpret_cast<void*>(&FakeButtonCollision));
        ButtonDll buttons{};buttons.sequenceCollision=rva(reinterpret_cast<void*>(&FakeSequenceCollision));buttons.sequenceCount=rva(&buttonSequence);
        g_buttonDll=&buttons;g_switchDll=&table;
        MotionDll motion{};motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;
        dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));dll.objects=rva(buttonObjects);
        g_hSwitchCollision.m_trampoline=reinterpret_cast<void*>(&FakeButtonCollision);
        g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
        std::memset(buttonObjects,0,sizeof(buttonObjects));std::memset(switchMemory,0,sizeof(switchMemory));
        auto* object=buttonObjects+asset.slot*3792;
        *reinterpret_cast<int16_t*>(object)=int16_t(asset.joints);
        *reinterpret_cast<Fn_SwitchCollision*>(object+56)=profile.sequence ? &FakeSequenceCollision : &FakeButtonCollision;
        *reinterpret_cast<const char**>(object+off::object_geom)=profile.name;
        for(int j=0;j<asset.joints;++j) {
            auto* bind=reinterpret_cast<float*>(object+1676+j*64);bind[0]=bind[5]=bind[10]=bind[15]=1;
            if(j) {bind[12]=-asset.bone[0];bind[13]=-asset.bone[1];bind[14]=-asset.bone[2];}
        }
        std::memcpy(buttonRoot,asset.root,sizeof(buttonRoot));std::memcpy(buttonBone,asset.bone,sizeof(buttonBone));
        *reinterpret_cast<int16_t*>(switchMemory+16)=int16_t(asset.slot);
        *reinterpret_cast<int16_t*>(switchMemory+18)=int16_t(asset.state);
        const int type=profile.sequence ? p%3 : profile.joints==2 ? 5 : profile.face.right>300 ? 3 : 1;
        *reinterpret_cast<int16_t*>(switchMemory+48)=int16_t(type);
        *reinterpret_cast<int16_t*>(switchMemory+42)=900;
        buttonActivations=buttonSequenceActivations=0;buttonSequence=0;buttonReject=false;
        *reinterpret_cast<int16_t*>(itemMemory+24)=103;
        g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
        VR().m_controllerPoseValid[hand]=true;VR().m_controllerPoseValid[1-hand]=false;
        VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;VR().m_rawControllerPose[hand].m[1][3]-=.2f;
        motiongun::Vec initialHand{};Check(MonitorHand(hand,initialHand),"button tracked palm available");
        const float x=(profile.face.left+profile.face.right)/2,y=(profile.face.top+profile.face.bottom)/2;
        const float scale=LiveWorldUnitsPerMetre(),outside=profile.face.z-scale*.16f,touch=profile.face.z;
        auto& pos=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);pos.y_rot=int16_t(yaw*16384);
        const uint8_t* definition=nullptr;Check(ButtonObject(switchMemory,definition)==&profile,"registered button profile found");
        motiongun::Frame inverse{},frame{};Check(ButtonFrame(switchMemory,object,profile,inverse) && motiongun::Inverse(inverse,frame),"button frame with native inverse bind");
        const auto initial=motiongun::Transform(frame,{x,y,outside});
        pos.x_pos=int32_t(std::lround(initialHand.x-initial.x));pos.y_pos=int32_t(std::lround(initialHand.y-initial.y));pos.z_pos=int32_t(std::lround(initialHand.z-initial.z));
        *reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)=pos;
        Check(ButtonFrame(switchMemory,object,profile,inverse) && motiongun::Inverse(inverse,frame),"rotated world button frame");
        const auto originalController=VR().m_rawControllerPose[hand];
        auto setHand=[&](motiongun::Vec local) {
            const auto delta=Sub(motiongun::Transform(frame,local),initialHand);VR().m_rawControllerPose[hand]=originalController;
            VR().m_rawControllerPose[hand].m[0][3]+=delta.x/scale;
            VR().m_rawControllerPose[hand].m[1][3]-=delta.y/scale;
            VR().m_rawControllerPose[hand].m[2][3]-=delta.z/scale;
        };
        uint64_t now=GetTickCount64();auto tick=[&](motiongun::Vec v) {setHand(v);AdvancePhysicalButton(itemMemory,nullptr,++now);};
        tick({x,y,touch});Check(!buttonActivations,"tracking acquired on button is not a press");
        tick({x,y,outside});switchSightBlocked=true;tick({x,y,touch});switchSightBlocked=false;
        Check(!buttonActivations,"touch does not pass through walls");
        tick({x,y,outside});*reinterpret_cast<int16_t*>(laraMemory+2)=4;tick({x,y,touch});*reinterpret_cast<int16_t*>(laraMemory+2)=0;
        Check(!buttonActivations,"armed hand cannot press");
        tick({x,y,outside});g_active=false;tick({x,y,touch});g_active=true;
        Check(!buttonActivations,"third-person cannot press");
        tick({x,y,outside});g_cogClaimed=true;tick({x,y,touch});g_cogClaimed=false;
        Check(!buttonActivations,"holding wheel prevents accidental button press");
        tick({x,y,outside});VR().m_controllerPoseValid[hand]=false;AdvancePhysicalButton(itemMemory,nullptr,++now);
        VR().m_controllerPoseValid[hand]=true;tick({x,y,touch});Check(!buttonActivations,"tracking regain does not press");
        tick({x,y,outside});now+=300;tick({x,y,touch});Check(!buttonActivations,"stale tracking does not press");
        if(profile.sequence) {buttonSequence=3;tick({x,y,outside});tick({x,y,touch});Check(!buttonActivations,"full puzzle sequence cannot accept extra button");buttonSequence=0;}
        if(profile.joints==2) {*reinterpret_cast<int16_t*>(switchMemory+18)=0;tick({x,y,outside});tick({x,y,touch});Check(!buttonActivations,"used elevator button remains unavailable");*reinterpret_cast<int16_t*>(switchMemory+18)=1;}
        tick({x,y,outside});tick({x,y,touch});
        Check(buttonActivations==1 && buttonSequenceActivations==(profile.sequence ? 1 : 0),"either hand activates correct native button handler");
        Check(*reinterpret_cast<int16_t*>(itemMemory+18)==2 && *reinterpret_cast<int16_t*>(itemMemory+24)==103 &&
            *reinterpret_cast<int16_t*>(laraMemory+2)==0 && *reinterpret_cast<int32_t*>(laraMemory+224)==0 && actionInput==0 && !g_switchCommit,
            "button preserves camera/body/input ownership");
        Check(*reinterpret_cast<int16_t*>(switchMemory+42)==900 && buttonSequence==0,"touch does not consume native timer or alter sequence bookkeeping");
        // Even a native/script reset cannot make a resting hand retrigger.
        *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=0;
        for(int j=0;j<6;++j)tick({x,y,touch-scale*(j%2 ? .03f : 0.f)});
        Check(buttonActivations==1,"threshold jitter and resting hand cannot repeat after native reset");
        tick({x,y,outside});tick({x,y,touch});Check(buttonActivations==2,"withdrawal permits deliberate second press after native reset");
        *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=0;
        *reinterpret_cast<int16_t*>(switchMemory+48)=6;Check(profile.sequence || !ButtonObject(switchMemory,definition),"keypad OCB retains native UI");
        *reinterpret_cast<int16_t*>(switchMemory+48)=0;Check(profile.sequence || !ButtonObject(switchMemory,definition),"lever OCB retains physical grip interaction");
        ResetPhysicalButtons();g_buttonDll=nullptr;g_switchDll=nullptr;g_motionDll=nullptr;
    }
    const firstperson::ButtonSurface face{-20,20,-40,40,-34};
    Check(firstperson::ButtonTouch(face,{0,0,-200},{0,0,-34},1024),"button front touch");
    Check(!firstperson::ButtonTouch(face,{0,0,100},{0,0,-34},1024),"button rear touch rejected");
    Check(!firstperson::ButtonTouch(face,{80,0,-200},{80,0,-34},1024),"panel casing is not button");
    Check(!firstperson::ButtonTouch(face,{0,100,-200},{0,100,-34},1024),"above button rejected");
    Check(!firstperson::ButtonTouch(face,{0,0,-1000},{0,0,-34},1024),"large tracking jump rejected");
    Check(!firstperson::ButtonTouch(face,{NAN,0,-200},{0,0,-34},1024),"invalid tracking rejected");
    Check(!firstperson::FindButtonProfile(1,"SWITCH_TYPE3_RICH2_RICH1",21),"numeric keypad excluded");
    Check(!firstperson::FindButtonProfile(0,"SWITCH_TYPE1_KARNAK1",1),"wall hole excluded");
}
}
