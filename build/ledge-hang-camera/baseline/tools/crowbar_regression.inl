namespace {
alignas(16) uint8_t crowbarObjects[321*3792]{};
tr::firstperson::SwitchAnim crowbarAnimations[800]{};
tr::firstperson::SwitchAnim* crowbarAnimationPtr=crowbarAnimations;
int16_t crowbarCommands[26]{};int16_t* crowbarCommandPtr=crowbarCommands;
float crowbarPoses[119][12]{};
int crowbarBase=0,crowbarDirection=0,crowbarNativeCalls=0,crowbarItemCalls=0,crowbarEffects[9]{},crowbarTriggers=0;
bool crowbarNativeAccept=true,crowbarFailure=false,crowbarMesh=false;

void __cdecl FakeCrowbarJoint(uint8_t* item,tr::PHD_VECTOR* v,int joint,int) {
    using namespace tr;
    Check(item==itemMemory && joint==10,"crowbar contact queries native tool wrist only");
    const int f=std::clamp(int(*reinterpret_cast<int16_t*>(item+26))-12475,0,118);
    const auto& p=*reinterpret_cast<PHD_3DPOS*>(item+96);
    motiongun::Frame yaw{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},Radians(p.y_rot))),
        {float(p.x_pos),float(p.y_pos),float(p.z_pos)}};
    const auto frame=motiongun::Multiply(yaw,motiongun::ReadRows(crowbarPoses[f]));
    const auto point=motiongun::Transform(frame,{float(v->x),float(v->y),float(v->z)});
    *v={int32_t(std::lround(point.x)),int32_t(std::lround(point.y)),int32_t(std::lround(point.z))};
}
void __cdecl FakeCrowbarAnimate(uint8_t* item) {
    using namespace tr;++crowbarItemCalls;
    if(item!=switchMemory) return;
    auto& f=*reinterpret_cast<int16_t*>(item+26);
    const auto& a=crowbarAnimations[*reinterpret_cast<int16_t*>(item+24)];
    if(++f>a.last) {
        ++crowbarTriggers;*reinterpret_cast<uint32_t*>(item+0x1820)=5;
        *reinterpret_cast<int16_t*>(item+24)=a.jump;f=a.jumpFrame;
    }
}
void __cdecl FakeCrowbarLara(uint8_t* item) {
    ++crowbarNativeCalls;
    auto& f=*reinterpret_cast<int16_t*>(item+26);
    ++f;
    for(int i=0,at=0;i<9;++i) {
        const int kind=crowbarCommands[at];
        if(kind==3) { if(f>12593)++crowbarEffects[i];++at; }
        else {
            if(f==crowbarCommands[at+1]) {
                ++crowbarEffects[i];
                if(kind==6 && crowbarCommands[at+2]==8) crowbarMesh=!crowbarMesh;
            }
            at+=3;
        }
    }
    if(f>12593) {
        *reinterpret_cast<int16_t*>(item+18)=*reinterpret_cast<int16_t*>(item+20)=2;
        *reinterpret_cast<int16_t*>(item+24)=11;f=184;*reinterpret_cast<int16_t*>(laraMemory+2)=0;
    }
}
void __cdecl FakeCrowbarCollision(int16_t index,uint8_t* lara,void*) {
    using namespace tr;
    Check(index==0 && lara==itemMemory,"crowbar calls native collision with original identity");
    if(!crowbarNativeAccept || !laraMemory[0x169] || !(actionInput&0x40)) return;
    Check(!g_switchCommit,"crowbar never bypasses inventory/alignment checks");
    *reinterpret_cast<int16_t*>(lara+18)=*reinterpret_cast<int16_t*>(lara+20)=40;
    *reinterpret_cast<int16_t*>(lara+24)=crowbarFailure ? 436 : 420;
    *reinterpret_cast<int16_t*>(lara+26)=crowbarFailure ? 13988 : 12475;
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;
    *reinterpret_cast<uint32_t*>(laraMemory+68)&=~0x20u;
    if(crowbarFailure) return;
    *reinterpret_cast<int16_t*>(switchMemory+24)=int16_t(crowbarBase+2+crowbarDirection);
    *reinterpret_cast<int16_t*>(switchMemory+26)=crowbarAnimations[crowbarBase+2+crowbarDirection].first;
    *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=3;
}
void __cdecl FakeCrowbarControl(int16_t) {}

template<class Reset,class Rva,class Pose>
void TestPhysicalCrowbar(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    FILE* file=nullptr;fopen_s(&file,"build/crowbar-poses.bin","rb");Check(file!=nullptr,"crowbar asset fixture exists");
    uint32_t models=0;Check(std::fread(&models,4,1,file)==1 && models==3,"all three installed crowbar level assets");
    auto loadAnimation=[](const uint8_t* raw,firstperson::SwitchAnim& a) {
        a={};a.frames=dummyLeverFrames;std::memcpy(&a.interpolation,raw+4,36);
    };
    for(unsigned model=0;model<models;++model) {
        char name[64]{};uint8_t use[40]{},sockets[2][40]{};int16_t cmds[25]{};
        Check(std::fread(name,64,1,file)==1 && std::fread(use,40,1,file)==1 && std::fread(cmds,50,1,file)==1 &&
              std::fread(sockets,80,1,file)==1 && std::fread(crowbarPoses,sizeof(crowbarPoses),1,file)==1,"complete authored crowbar fixture");
        for(int direction:{0,1}) for(int turn:{0,1,2,3}) for(int hd:{0,1}) {
            game=dll.game=0;reset();pose(0,0,0,0);ResetPhysicalCrowbar();
            crowbarNativeAccept=true;crowbarFailure=false;crowbarDirection=direction;
            crowbarNativeCalls=crowbarItemCalls=crowbarTriggers=0;crowbarMesh=false;
            std::memset(crowbarEffects,0,sizeof(crowbarEffects));
            std::memset(crowbarObjects,0,sizeof(crowbarObjects));std::memset(switchMemory,0,sizeof(switchMemory));
            std::memset(crowbarAnimations,0,sizeof(crowbarAnimations));
            loadAnimation(use,crowbarAnimations[420]);crowbarAnimations[420].commandIndex=0;
            std::memcpy(crowbarCommands,cmds,sizeof(cmds));crowbarCommands[25]=4;
            firstperson::SwitchAnim first{},second{};loadAnimation(sockets[0],first);loadAnimation(sockets[1],second);
            crowbarBase=second.jump;first.commandIndex=second.commandIndex=25;
            crowbarAnimations[crowbarBase+2]=first;crowbarAnimations[crowbarBase+3]=second;
            SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
            table.commands=rva(&crowbarCommandPtr);g_switchDll=&table;
            CrowbarDll profile{0x696B4999,rva(reinterpret_cast<void*>(&FakeCrowbarCollision)),rva(reinterpret_cast<void*>(&FakeCrowbarControl))};
            g_crowbarDll=&profile;
            g_hCrowbarCollision.m_trampoline=reinterpret_cast<void*>(&FakeCrowbarCollision);
            g_hCrowbarAnimate.m_trampoline=reinterpret_cast<void*>(&FakeCrowbarAnimate);
            g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeCrowbarLara);
            dll.objects=rva(crowbarObjects);dll.anims=rva(&crowbarAnimationPtr);dll.frameFrac=rva(&fraction);
            dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeCrowbarJoint));
            MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;g_motionHooksReady=true;
            appMemory[0x9e4]=uint8_t(hd);config.firstPersonMotionGuns=config.firstPersonUnarmedIK=true;
            auto* o=crowbarObjects+320*3792;
            *reinterpret_cast<int16_t*>(o)=1;*reinterpret_cast<int16_t*>(o+66)=int16_t(crowbarBase);
            *reinterpret_cast<Fn_SwitchControl*>(o+24)=&FakeCrowbarControl;
            *reinterpret_cast<Fn_SwitchCollision*>(o+56)=&FakeCrowbarCollision;
            *reinterpret_cast<const char**>(o+off::object_geom)=model==0 ? "CROWBAR_SWITCH_HIGHSTRT" : "CROWBAR_SWITCH";
            *reinterpret_cast<int16_t*>(crowbarObjects)=15;
            for(int j=0;j<15;++j) { auto* b=reinterpret_cast<float*>(crowbarObjects+1676)+j*16;b[0]=b[5]=b[10]=b[15]=1; }
            *reinterpret_cast<int16_t*>(switchMemory+16)=320;
            auto& body=*reinterpret_cast<PHD_3DPOS*>(itemMemory+96);body.y_rot=int16_t(turn*16384);
            g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
            VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
            VR().m_rawControllerPose[0]=VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[1].m[1][3]-=.2f;
            *reinterpret_cast<int16_t*>(itemMemory+24)=103;laraMemory[0x169]=1;
            actionInput=0x40;g_switchCommit=nullptr;
            // Capture must also work after native auto-alignment marked hands busy.
            if(turn&1) { *reinterpret_cast<int16_t*>(laraMemory+2)=1;*reinterpret_cast<uint32_t*>(laraMemory+68)=0x20;*reinterpret_cast<int16_t*>(laraMemory+0x15c)=0; }
            Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(PhysicalCrowbarOwns(itemMemory) && Gate(),"only accepted TR4 use stays first person, including alignment");
            Check(UnarmedIKReady()==bool(hd),"remastered free hand stays tracked; classic renderer retains native tool");
            Check(actionInput==0x40,"insertion does not replace input");actionInput=0;
            auto tick=[&]() {
                if(direction==0) Detour_CrowbarAnimateItem(switchMemory);
                Detour_AnimateLara(itemMemory);
                if(direction==1) Detour_CrowbarAnimateItem(switchMemory);
            };
            for(int i=0;i<46;++i)tick();
            Check(*reinterpret_cast<int16_t*>(itemMemory+26)==12475+46 && crowbarMesh,"native insertion and tool swap run before physical hold");
            const auto bodyBefore=body;
            for(int i=0;i<80;++i)tick();
            Check(crowbarNativeCalls==46 && !crowbarTriggers && crowbarEffects[0]==1 && crowbarEffects[1]==0,"waiting cannot fire effects twice or complete socket");
            const int otherCalls=crowbarItemCalls;Detour_CrowbarAnimateItem(itemMemory);
            Check(crowbarItemCalls==otherCalls+1,"other animated objects always execute their native tick while crowbar is held");
            PHD_3DPOS eye{99,99,99,0,123,0,0};PhysicalCrowbarCamera(itemMemory,eye);
            Check(eye.x_pos==0 && eye.y_pos==-700 && eye.z_pos==-100 && eye.y_rot==123,"pry camera removes animation bob while preserving view yaw");
            motiongun::Frame wrist{};motiongun::Vec palm{};Check(CrowbarWrist(wrist,palm),"native inserted-tool contact available");
            motiongun::Frame attached{};AttachPhysicalCrowbarHand(1,attached);
            Check(firstperson::Length(Sub(attached.origin,wrist.origin))<.01f,"right wrist remains on the actual tool, not controller-offset above it");
            motiongun::Vec originalHand{};float y=0,headY=0;SwitchHand(originalHand,y,headY);
            const auto controller=VR().m_rawControllerPose[1];
            auto setHand=[&](motiongun::Vec p) {
                VR().m_rawControllerPose[1]=controller;const auto d=Sub(p,originalHand);
                VR().m_rawControllerPose[1].m[0][3]+=d.x/1000;
                VR().m_rawControllerPose[1].m[1][3]-=d.y/1000;
                VR().m_rawControllerPose[1].m[2][3]-=d.z/1000;
            };
            auto grip=motiongun::Transform(wrist,palm);setHand(Add(grip,{500,0,0}));
            Check(!FirstPersonSwitchGrip(true,true),"distant RG cannot grab crowbar");FirstPersonSwitchGrip(false,true);
            setHand(grip);Check(FirstPersonSwitchGrip(true,true),"RG grabs the inserted crowbar without an Action icon");
            for(int i=0;i<8;++i) { FirstPersonSwitchGrip(true,true);tick(); }
            Check(crowbarNativeCalls==46,"holding stationary cannot pry");
            setHand(Add(grip,{0,-100,0}));FirstPersonSwitchGrip(true,true);tick();
            Check(crowbarNativeCalls==46,"raising hand cannot pry");
            setHand(Add(grip,{0,40,0}));FirstPersonSwitchGrip(true,true);
            for(int i=0;i<6;++i)tick();
            Check(crowbarNativeCalls>=51 && crowbarNativeCalls<=52 && !crowbarTriggers,"downward motion advances only the authored pry");
            FirstPersonSwitchGrip(false,true);const int parked=crowbarNativeCalls;
            for(int i=0;i<80;++i)tick();
            Check(crowbarNativeCalls==parked && crowbarMesh && !crowbarTriggers,"release parks inserted tool with no snapback or early trigger");
            CrowbarWrist(wrist,palm);grip=motiongun::Transform(wrist,palm);setHand(grip);
            Check(FirstPersonSwitchGrip(true,true),"partial pry can be regripped at its current position");
            tick();Check(crowbarNativeCalls==parked,"regrip does not spend stale movement");
            setHand(Add(grip,{0,100,0}));FirstPersonSwitchGrip(true,true);
            for(int i=0;i<140 && *reinterpret_cast<int16_t*>(itemMemory+24)==420;++i)tick();
            Check(*reinterpret_cast<int16_t*>(itemMemory+24)==11 && !crowbarMesh && *reinterpret_cast<int16_t*>(laraMemory+2)==0,
                  "native removal restores tool mesh and releases Lara");
            for(int i=0;i<3 && !crowbarTriggers;++i)Detour_CrowbarAnimateItem(switchMemory);
            Check(crowbarTriggers==1,"one native socket completion in either controller ordering");
            for(int event:crowbarEffects) Check(event==1,"each native command runs exactly once, including both mesh swaps");
            Check(!std::memcmp(&body,&bodyBefore,sizeof(body)),"pry never relocates or rotates Lara");
            ResetPhysicalCrowbar();

            // New native uses exercise exclusions/fallback rather than fabricated success.
            auto start=[&]() {
                ResetPhysicalCrowbar();crowbarNativeAccept=true;crowbarFailure=false;actionInput=0x40;laraMemory[0x169]=1;
                *reinterpret_cast<int16_t*>(itemMemory+18)=2;*reinterpret_cast<int16_t*>(itemMemory+24)=103;
                *reinterpret_cast<int16_t*>(laraMemory+2)=0;*reinterpret_cast<uint32_t*>(laraMemory+68)=0;
                *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=0;
                std::memset(crowbarEffects,0,sizeof(crowbarEffects));crowbarMesh=false;
            };
            start();crowbarFailure=true;Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(!g_crowbar.active && *reinterpret_cast<int16_t*>(itemMemory+24)==436 && !Gate(),"failed pry stays entirely native, including third-person fallback");
            start();laraMemory[0x169]=0;Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(!g_crowbar.active,"no crowbar possession cannot start physical pry");
            start();crowbarNativeAccept=false;Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(!g_crowbar.active,"native alignment or inventory refusal cannot be bypassed");
            start();crowbarCommands[2]=9;Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(!g_crowbar.active,"unverified animation commands retain native behavior");crowbarCommands[2]=8;
            start();game=dll.game=1;Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(!g_crowbar.active,"TR5 never enters TR4 crowbar handler");game=dll.game=0;
            start();g_runtimeEnabled=false;Detour_CrowbarCollision(0,itemMemory,nullptr);
            Check(!g_crowbar.active,"third-person use remains native");g_runtimeEnabled=true;
            start();Detour_CrowbarCollision(0,itemMemory,nullptr);actionInput=0;
            for(int i=0;i<47;++i)tick();actionInput=0x40;tick();
            Check(g_crowbar.finishing && *reinterpret_cast<int16_t*>(itemMemory+26)>12475+46,"explicit Action resumes native animation at held pose");
            start();Detour_CrowbarCollision(0,itemMemory,nullptr);actionInput=0;
            for(int i=0;i<47;++i)tick();VR().m_controllerPoseValid[1]=false;
            g_crowbar.trackingLost=GetTickCount64()-501;tick();
            Check(g_crowbar.finishing,"tracking loss falls back to native cleanup instead of permanent lock");VR().m_controllerPoseValid[1]=true;
            start();Detour_CrowbarCollision(0,itemMemory,nullptr);actionInput=0;
            for(int i=0;i<48;++i)tick();*reinterpret_cast<int16_t*>(itemMemory+26)=12475;
            Check(!PhysicalCrowbarOwns(itemMemory),"loaded or restarted animation invalidates previous ownership");
            start();Detour_CrowbarCollision(0,itemMemory,nullptr);actionInput=0;
            for(int i=0;i<47;++i)tick();g_runtimeEnabled=false;tick();
            Check(!g_crowbar.active && *reinterpret_cast<int16_t*>(itemMemory+26)==12475+47,"mode exit immediately yields to native animation without pose rollback");
            g_runtimeEnabled=true;
            start();Detour_CrowbarCollision(0,itemMemory,nullptr);actionInput=0;
            for(int i=0;i<47;++i)tick();
            ++reinterpret_cast<PHD_3DPOS*>(switchMemory+96)->x_pos;
            Check(!PhysicalCrowbarOwns(itemMemory),"relocated/replaced socket cannot retain ownership");
            ResetPhysicalCrowbar();g_crowbarDll=nullptr;g_switchDll=nullptr;g_motionDll=nullptr;
        }
    }
    std::fclose(file);
}
}
