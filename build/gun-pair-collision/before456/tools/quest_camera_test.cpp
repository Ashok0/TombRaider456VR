// Only the quest-camera regression is executed, never the full self-test.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
alignas(16) uint8_t questSocket[9416]{};
uint8_t* questItems=questSocket;
int questCount=1;
}
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p) { const auto n=reinterpret_cast<uint64_t>(p)-g_boundBase;
        Check(n<=UINT32_MAX,"fixture RVA fits");return uint32_t(n); };
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.frameFrac=rva(&fraction);dll.analogInput=rva(analogMemory);dll.input=rva(&actionInput);
    dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeJoint));g_boundDll=&dll;
    SwitchDll sockets{};sockets.items=rva(&questItems);sockets.count=rva(&questCount);g_switchDll=&sockets;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&FakeDraw);
    g_hDrawHair.m_trampoline=reinterpret_cast<void*>(&FakeHair);
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
    auto& animation=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    auto& slot=*reinterpret_cast<int16_t*>(questSocket+off::item_object);
    auto& trigger=*reinterpret_cast<int16_t*>(questSocket+48);
    const PHD_3DPOS native{100,200,300,10,20,30,0};
    const auto reset=[&](int which) {
        RestoreHeadMesh();ResetMovementStabilization();UpdateControlMode(false);
        config=Config{};config.enabled=config.firstPerson=config.positionalTracking=true;
        config.firstPersonHeadTranslation=config.firstPersonHideHead=true;
        config.firstPersonMotionGuns=config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=false;
        config.firstPersonRoomscaleMove=config.firstPersonWallCollision=config.firstPersonBodyFollowsHead=false;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(cameraMemory,0,sizeof(cameraMemory));std::memset(appMemory,0,sizeof(appMemory));
        game=dll.game=which;water=0;state=goal=2;animation=103;bits=0x7fff;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        slot=int16_t(which==0 ? 260 : 245);trigger=-423;
        questItems=questSocket;questCount=1;
        g_runtimeInitialized=g_runtimeEnabled=true;g_active=g_haveHeading=g_scenePoseValid=false;
        g_headingItem=nullptr;g_groundEye.Reset();g_autoBodyCalibration={};
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(.4f);
        VR().RecenterFirstPersonHead();
    };
    for(int which:{0,1}) for(int useState:{42,43,89}) for(int graphics:{0,1}) {
        reset(which);appMemory[0x9e4]=uint8_t(graphics);
        PHD_3DPOS camera=native;UpdateSceneCamera(camera);
        Check(g_active && g_runtimeEnabled && g_headHidden,"standing first person active before quest use");
        const float heading=g_lastHeadWorld;
        goal=int16_t(useState);Check(Gate(),"queued use/approaching socket does not switch camera early");
        state=int16_t(useState);goal=2;animation=int16_t(useState==89 ? 423 : useState==42 ? 131 : 134);
        g_gunTriggers.active=true;g_gunTriggers.pending[0]=true;g_gunEquip.pending=true;g_haveManualInput=true;
        auto itemBefore=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
        *reinterpret_cast<uint32_t*>(itemBefore.data()+off::item_mesh_bits)=0x7fff;
        const auto laraBefore=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
        for(int frame=0;frame<5;++frame) {
            if(useState==89 && frame==2) slot+=int16_t(which==0 ? 12 : 8); // Native PuzzleDone.
            camera=native;UpdateSceneCamera(camera);
            Check(!g_active && !g_scenePoseValid && g_runtimeEnabled,"quest use temporarily suspends first person without changing preference");
            Check(!std::memcmp(&camera,&native,sizeof(camera)) && bits==0x7fff && !g_headHidden,
                "complete insertion animation uses native camera and visible head/body");
            Check(!g_gunTriggers.WantsShot() && !g_gunEquip.pending && !g_haveManualInput,
                "temporary camera clears queued first-person firing/equip/movement");
            Check(!std::memcmp(itemBefore.data(),itemMemory,sizeof(itemMemory)) &&
                !std::memcmp(laraBefore.data(),laraMemory,sizeof(laraMemory)),"camera never changes native insertion state or inventory");
        }
        const int beforeDraw=draws,beforeHair=hairs;
        Detour_DrawCreatureHD(itemMemory,0,0);Detour_DrawHair(0);
        Check(draws==beforeDraw+1 && hairs==beforeHair+1,"third-person insertion uses complete native body and hair");
        state=goal=2;animation=103;Head(-.4f);camera=native;UpdateSceneCamera(camera);
        Check(g_active && g_runtimeEnabled && Near(Wrap(g_lastHeadWorld-heading),0),
            "insertion completion resumes first person and previous viewing heading");
        // Explicit third-person selection during the animation wins.
        state=int16_t(useState);animation=int16_t(useState==89 ? 423 : 134);UpdateSceneCamera(camera);
        FirstPersonToggle();Check(!g_runtimeEnabled,"user can select third person during insertion");
        state=goal=2;animation=103;camera=native;UpdateSceneCamera(camera);
        Check(!g_active && !g_runtimeEnabled && !std::memcmp(&camera,&native,sizeof(camera)),
            "finishing an insertion never overrides user's third-person selection");
    }
    for(int which:{0,1}) {
        reset(which);state=89;animation=423;
        Check(QuestItemCameraOverride(itemMemory),"custom insertion follows its actual puzzle socket");
        for(int guard=0;guard<6;++guard) {
            slot=int16_t(which==0 ? 260 : 242);trigger=-423;animation=423;questCount=1;questItems=questSocket;
            *reinterpret_cast<int16_t*>(laraMemory+off::lara_interacted_item)=0;
            if(guard==0) slot=100;
            if(guard==1) trigger=0;
            if(guard==2) animation=424;
            if(guard==3) questCount=0;
            if(guard==4) *reinterpret_cast<int16_t*>(laraMemory+off::lara_interacted_item)=-1;
            if(guard==5) questItems=nullptr;
            Check(!QuestItemCameraOverride(itemMemory),"unrelated/stale controlled animation does not trigger quest camera");
        }
        for(int other:{0,1,2,39,40,41,71,89,94,98,103,105,125,127})
            Check(!firstperson::QuestItemUsesThirdPerson(which,other),"non-quest animation states keep their existing camera rules");
    }
    for(int state:{42,43,89}) Check(!firstperson::QuestItemUsesThirdPerson(2,state,260,-423,423),"TR6 state meanings are excluded");
    VR().m_system=nullptr;
    std::printf("PASS: %d focused quest-camera checks (TR4/5); no game or full self-test executed.\n",checks);
}
