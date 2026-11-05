namespace {
float vciPoses[168][24]{};
int vciActivations=0,vciControls=0,vciAnimateCalls=0;
void __cdecl FakeVciMatrices(uint8_t* item) {
    using namespace tr;
    const int frame=*reinterpret_cast<int16_t*>(item+26);
    Check(frame>=0 && frame<168,"VCI authored frame index valid");
    const auto& pos=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    motiongun::Frame rotation{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},Radians(pos.y_rot))),{}};
    for(int j=0;j<2;++j) motiongun::WriteRows(motiongun::Multiply(rotation,motiongun::ReadRows(vciPoses[frame]+j*12)),
        reinterpret_cast<float*>(item+7832)+j*12);
}
void __cdecl FakeVciAnimate(uint8_t* item) {
    ++vciAnimateCalls;
    auto& animation=*reinterpret_cast<int16_t*>(item+24);auto& frame=*reinterpret_cast<int16_t*>(item+26);
    auto& state=*reinterpret_cast<int16_t*>(item+18);
    if(animation==0) { animation=2;frame=2;state=0; }
    else if(animation==1) { animation=3;frame=85;state=1; }
    else if(++frame>leverAnimations[animation].last) {
        frame=leverAnimations[animation].jumpFrame;animation=leverAnimations[animation].jump;
        *reinterpret_cast<uint32_t*>(item+0x1820)=5;
    }
}
void __cdecl FakeVciControl(int16_t index) {
    Check(index==0,"VCI commit controls only grabbed item");++vciControls;
    *reinterpret_cast<int16_t*>(switchMemory+44)|=0x3e00;
    --*reinterpret_cast<int16_t*>(switchMemory+42);
    FakeVciAnimate(switchMemory);
}
void __cdecl FakeVciCollision(int16_t index,uint8_t* lara,void*) {
    using namespace tr;
    if(!(actionInput&0x40)) return;
    Check(index==0 && *reinterpret_cast<int16_t*>(lara+18)==2 && *reinterpret_cast<int16_t*>(lara+24)==103,
          "VCI native standing precondition retained");
    Check(Detour_SwitchTestPosition(switchBounds,switchMemory,lara)==1 &&
          Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,"VCI hand commit needs no native icon/alignment");
    ++vciActivations;const int goal=1-*reinterpret_cast<int16_t*>(switchMemory+18);
    *reinterpret_cast<int16_t*>(switchMemory+20)=int16_t(goal);*reinterpret_cast<uint32_t*>(switchMemory+0x1820)=3;
    FakeVciAnimate(switchMemory);
    *reinterpret_cast<int16_t*>(lara+18)=int16_t(goal ? 40 : 41);*reinterpret_cast<int16_t*>(lara+24)=int16_t(goal ? 196 : 195);
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;std::memset(laraMemory+224,0,12);
}

template<class Reset,class Rva,class Pose>
void TestPhysicalVciLever(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    FILE* file=nullptr;fopen_s(&file,"build/vci-lever-poses.bin","rb");Check(file!=nullptr,"installed VCI pose fixture available");
    uint32_t cases=0;Check(std::fread(&cases,4,1,file)==1 && cases==2,"both VCI flip lever models");
    for(unsigned model=0;model<cases;++model) {
        char name[64]{};float bone[3]{};int16_t bounds[6]{};
        Check(std::fread(name,64,1,file)==1 && std::fread(bone,sizeof(bone),1,file)==1 &&
              std::fread(bounds,sizeof(bounds),1,file)==1 && std::fread(vciPoses,sizeof(vciPoses),1,file)==1,"complete measured VCI fixture");
        for(int initial:{0,1}) for(int turn:{0,1,2,3}) {
            game=dll.game=1;reset();pose(0,0,0,0);FirstPersonRecenter();
            SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
            table.collision=rva(reinterpret_cast<void*>(&FakeVciCollision));table.bounds=rva(switchBounds);
            table.getBounds=rva(reinterpret_cast<void*>(&FakeSwitchBounds));table.animate=rva(reinterpret_cast<void*>(&FakeVciAnimate));
            table.calcMatrices=rva(reinterpret_cast<void*>(&FakeVciMatrices));
            table.changes=rva(&leverChangePtr);table.ranges=rva(&leverRangePtr);table.commands=rva(&leverCommandPtr);
            MotionDll motion{};motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;
            dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));dll.objects=rva(leverObject);dll.anims=rva(&leverAnimationPtr);
            g_switchDll=&table;g_hSwitchCollision.m_trampoline=reinterpret_cast<void*>(&FakeVciCollision);
            g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
            std::memset(leverObject,0,sizeof(leverObject));std::memset(switchMemory,0,sizeof(switchMemory));
            *reinterpret_cast<int16_t*>(leverObject)=15;
            for(int j=0;j<15;++j) { auto* b=reinterpret_cast<float*>(leverObject+1676)+j*16;b[0]=b[5]=b[10]=b[15]=1; }
            auto* object=leverObject+266*3792;*reinterpret_cast<int16_t*>(object)=2;
            *reinterpret_cast<Fn_SwitchControl*>(object+24)=&FakeVciControl;*reinterpret_cast<Fn_SwitchCollision*>(object+56)=&FakeVciCollision;
            *reinterpret_cast<const char**>(object+off::object_geom)=name;
            for(int j=0;j<2;++j) {
                auto* b=reinterpret_cast<float*>(object+1676)+j*16;b[0]=b[5]=b[10]=b[15]=1;
                if(j) { b[12]=-bone[0];b[13]=-bone[1];b[14]=-bone[2]; }
            }
            *reinterpret_cast<int16_t*>(switchMemory+16)=266;*reinterpret_cast<int16_t*>(switchMemory+48)=4;
            *reinterpret_cast<int16_t*>(switchMemory+18)=*reinterpret_cast<int16_t*>(switchMemory+20)=int16_t(initial);
            *reinterpret_cast<int16_t*>(switchMemory+24)=*reinterpret_cast<int16_t*>(switchMemory+26)=int16_t(1-initial);
            *reinterpret_cast<int16_t*>(switchMemory+42)=100;
            for(auto& a:leverAnimations) { a={};a.frames=dummyLeverFrames;a.interpolation=1; }
            leverAnimations[0].state=1;leverAnimations[0].changeCount=1;
            leverAnimations[1].first=leverAnimations[1].last=leverAnimations[1].jumpFrame=1;
            leverAnimations[1].jump=1;leverAnimations[1].changeCount=1;leverAnimations[1].changeIndex=1;
            leverAnimations[2].first=2;leverAnimations[2].last=84;leverAnimations[2].jump=1;leverAnimations[2].jumpFrame=1;leverAnimations[2].commandCount=1;
            leverAnimations[3].state=1;leverAnimations[3].first=85;leverAnimations[3].last=167;leverAnimations[3].commandCount=1;
            leverChanges[0]={0,1,0};leverChanges[1]={1,1,1};leverCommands[0]=4;
            leverRanges[0]={0,1,2,2};leverRanges[1]={1,2,3,85};
            std::memcpy(leverBounds,bounds,sizeof(bounds));
            g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
            *reinterpret_cast<int16_t*>(itemMemory+24)=103;
            VR().m_controllerPoseValid[1]=true;VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[1].m[1][3]-=.2f;
            motiongun::Vec originalHand{};float handY=0,headY=0;
            Check(SwitchHand(originalHand,handY,headY),"VCI calibrated palm tracking available");
            const auto originalController=VR().m_rawControllerPose[1];
            auto setHand=[&](motiongun::Vec world) {
                const auto d=Sub(world,originalHand);VR().m_rawControllerPose[1]=originalController;
                VR().m_rawControllerPose[1].m[0][3]+=d.x/LiveWorldUnitsPerMetre();
                VR().m_rawControllerPose[1].m[1][3]-=d.y/LiveWorldUnitsPerMetre();
                VR().m_rawControllerPose[1].m[2][3]-=d.z/LiveWorldUnitsPerMetre();
            };
            auto& target=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);
            target={300,0,500,0,int16_t(turn*16384),0,0};
            *reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)=target;
            auto& body=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
            const auto bodyOffset=locomotion::Rotate({0,250},Radians(target.y_rot));
            body.x_pos+=int32_t(bodyOffset.x);body.z_pos+=int32_t(bodyOffset.z);
            auto meshPoint=[&](motiongun::Vec point) {
                FakeVciMatrices(switchMemory);
                auto f=motiongun::ReadRows(reinterpret_cast<float*>(switchMemory+7832)+12);
                f.origin=Add(f.origin,{float(target.x_pos),float(target.y_pos),float(target.z_pos)});
                return motiongun::Transform(f,Sub(point,{bone[0],bone[1],bone[2]}));
            };
            uint64_t now=GetTickCount64();vciActivations=vciControls=vciAnimateCalls=0;switchSightBlocked=false;
            const auto grip=meshPoint({0,-82,-42.5f});setHand(grip);
            motiongun::Vec local{},contact{};
            Check(IsPhysicalVciLever(switchMemory) && VciLeverGripPoint(switchMemory,grip,local,contact) &&
                  firstperson::Length(Sub(grip,contact))<.01f,"both idle states and wall headings grab the real bar with inverse bind");
            float distance=0;
            Check(!SwitchReach(switchMemory,itemMemory,meshPoint({0,0,-42.5f}),distance,false),"backplate/pivot is not a grab point");
            switchSightBlocked=true;Check(!UpdatePhysicalSwitchGrip(true,true,++now),"VCI switch cannot be grabbed through a wall");
            UpdatePhysicalSwitchGrip(false,true,++now);switchSightBlocked=false;
            const bool attached=UpdatePhysicalSwitchGrip(true,true,++now);
            if(!attached) {
                motiongun::Vec measured{};SwitchHand(measured,handY,headY);
                std::printf("VCI debug model=%u initial=%d turn=%d ready=%d reach=%d candidate=%d hand=%.2f,%.2f,%.2f actual=%.2f,%.2f,%.2f count=%d path=%d joint=%d\n",
                    model,initial,turn,PhysicalSwitchReady(),SwitchReach(switchMemory,itemMemory,measured,distance,false),g_switchCandidate.index,
                    grip.x,grip.y,grip.z,measured.x,measured.y,measured.z,switchCount,g_switchPreview.path.count,g_switchPreview.joint);
            }
            Check(attached && g_switchPreview.vci && g_switchPreview.joint==1,"RG attaches to verified VCI grip bar without Action icon");
            Check(g_switchPreview.path.count==85,"full authored VCI transition retained");
            Check(!AdvancePhysicalSwitch(itemMemory,nullptr,++now) && !vciActivations,"holding handle stationary never toggles switch");
            for(int i=1;i<=40;++i) {
                setHand(Sub(g_switchPreview.points[i],g_switchPreview.handOffset));
                AdvancePhysicalSwitch(itemMemory,nullptr,++now);
                Check(g_switchGripClaimed && !vciActivations,"partial VCI flip follows hand without firing trigger");
            }
            const int parked=g_switchPreview.selected;const auto parkedFrame=*reinterpret_cast<int16_t*>(switchMemory+26);
            UpdatePhysicalSwitchGrip(false,true,++now);
            Check(g_parkedSwitches.size()==1 && *reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame,"VCI partial flip retains pose after release");
            const auto newGrip=meshPoint({40,-82,-42.5f});setHand(newGrip);
            Check(UpdatePhysicalSwitchGrip(true,true,++now),"regrab a different point of partially flipped VCI handle");
            Check(g_switchPreview.selected==parked && firstperson::Length(Sub(g_switchPreview.grip,newGrip))<.05f &&
                  *reinterpret_cast<int16_t*>(switchMemory+26)==parkedFrame,"regrip updates hand contact without snapping lever or floating hand");
            motiongun::Frame wrist{firstperson::IdentityBasis(),{100,200,300}};
            AttachPhysicalSwitchHand(1,wrist,newGrip);
            Check(firstperson::Length(Sub(motiongun::Transform(wrist,SwitchPalmMesh),newGrip))<.05f,"visible palm seals onto selected handle point");
            const auto laraBefore=body;const auto inputBefore=actionInput;
            for(int i=parked+1;i<g_switchPreview.path.count && !g_switchCompleted;++i) {
                setHand(Sub(g_switchPreview.points[i],g_switchPreview.handOffset));AdvancePhysicalSwitch(itemMemory,nullptr,++now);
            }
            Check(g_switchCompleted && vciActivations==1 && vciControls==1 && vciAnimateCalls==84,
                  "one complete flip runs native collision once and all completion commands");
            Check(*reinterpret_cast<int16_t*>(switchMemory+18)==1-initial && (*reinterpret_cast<uint32_t*>(switchMemory+0x1820)&6)==4 &&
                  *reinterpret_cast<int16_t*>(switchMemory+42)==99,"native target state, completion status and single timer tick preserved");
            Check(!std::memcmp(&body,&laraBefore,sizeof(body)) && *reinterpret_cast<int16_t*>(itemMemory+18)==2 &&
                  *reinterpret_cast<int16_t*>(itemMemory+24)==103 && *reinterpret_cast<int16_t*>(laraMemory+2)==0 && actionInput==inputBefore,
                  "VCI flip preserves Lara pose, camera/body position, weapon status and input");
            for(int i=0;i<4;++i)AdvancePhysicalSwitch(itemMemory,nullptr,++now);
            Check(vciActivations==1,"held grip after flip cannot retrigger mechanism");
            UpdatePhysicalSwitchGrip(false,true,++now);ResetPhysicalSwitch();
            for(int type:{1,2,3,5,6,7,8,10}) {
                *reinterpret_cast<int16_t*>(switchMemory+48)=int16_t(type);
                Check(!IsPhysicalVciLever(switchMemory),"button/keypad/valve OCBs excluded from VCI lever profile");
            }
            *reinterpret_cast<int16_t*>(switchMemory+48)=4;
            game=dll.game=0;Check(!IsPhysicalVciLever(switchMemory),"TR4 behavior not reclassified by VCI profile");game=dll.game=1;
            g_switchDll=nullptr;g_motionDll=nullptr;
        }
    }
    std::fclose(file);
}
}
