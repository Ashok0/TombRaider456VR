// Reuse native/VR fixtures without executing the full first-person suite.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p) {
        const auto d=reinterpret_cast<uint64_t>(p)-g_boundBase;
        Check(d<=UINT32_MAX,"test addresses fit module RVA");return uint32_t(d);
    };
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);
    dll.camera=rva(cameraMemory);dll.input=rva(&actionInput);
    g_boundDll=&dll;
    const auto pose=[](float yaw,float right,float forward,float up) {
        Head(yaw);
        VR().m_rawHeadPose.m[0][3]+=right;
        VR().m_rawHeadPose.m[1][3]+=up;
        VR().m_rawHeadPose.m[2][3]-=forward;
        VR().m_headFromTracking=InvertRigid(FromHmd(VR().m_rawHeadPose));
    };
    const auto reset=[]() {
        config=Config{};
        config.firstPerson=true;config.enabled=true;
        config.positionalTracking=config.firstPersonHeadTranslation=true;
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));
        VR().m_poseValid=true;
        g_active=g_runtimeEnabled=g_haveHeading=true;
        g_headingBase=g_lastHeadWorld=0;g_headingItem=itemMemory;
        std::memset(itemMemory,0,sizeof(itemMemory));
        std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(cameraMemory,0,sizeof(cameraMemory));
        std::memset(appMemory,0,sizeof(appMemory));
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=dll.game==0 ? -1 : 0;
        water=0;actionInput=0;Head(0);
    };
    TestPhysicalSwitch(reset,rva,pose);
    std::printf("PASS: %d physical-switch checks (TR4/5)\n",checks);
}
