// Exercise the actual simulation hook, including turns AFTER AnimateLara.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
int hangDelta=32768,hangState=10;
void __cdecl HangJoint(uint8_t* item,tr::PHD_VECTOR* point,int,int) {
    const auto& pos=*reinterpret_cast<const tr::PHD_3DPOS*>(item+tr::off::item_pos);
    *point={pos.x_pos,pos.y_pos-700,pos.z_pos};
}
void __cdecl FakeStoppedHang(uint8_t* item,void*) {
    tr::Detour_AnimateLara(item);
    auto& pos=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    pos.y_rot=int16_t(int(pos.y_rot)+hangDelta);
    *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=int16_t(hangState);
    *reinterpret_cast<int16_t*>(item+tr::off::item_anim_number)=hangState==10 ? 96 : 43;
}
}
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p) {return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.frameFrac=rva(&fraction);dll.analogInput=rva(analogMemory);dll.input=rva(&actionInput);
    dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&HangJoint));g_boundDll=&dll;
    g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeAnimate);
    g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeStoppedHang);
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& animation=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
    auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
    auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
    for(int which:{0,1}) for(bool stable:{false,true}) for(int degrees:{-170,0,90,170})
    for(int endState:{10,139}) for(int snap:{-364,0,364}) {
        config=Config{};config.enabled=config.firstPerson=config.positionalTracking=true;
        config.firstPersonHeadTranslation=true;config.firstPersonMovementStabilization=stable;
        config.firstPersonMotionGuns=config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=false;
        config.firstPersonRoomscaleMove=config.firstPersonWallCollision=config.firstPersonBodyFollowsHead=false;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(cameraMemory,0,sizeof(cameraMemory));std::memset(appMemory,0,sizeof(appMemory));
        std::memset(analogMemory,0,sizeof(analogMemory));actionInput=0;
        game=dll.game=which;water=0;state=2;animation=103;fraction=256;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x7fff;
        g_runtimeInitialized=g_runtimeEnabled=true;g_active=g_haveHeading=g_scenePoseValid=false;
        g_headingItem=nullptr;ResetMovementStabilization();g_groundEye.Reset();g_autoBodyCalibration={};
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(.4f);
        pos.y_rot=Angle(degrees*kPi/180);prev=pos;VR().RecenterFirstPersonHead();
        PHD_3DPOS camera{};UpdateSceneCamera(camera);Check(g_active,"first person fixture active");
        state=139;animation=48;hangState=endState;hangDelta=32768+snap;
        g_manualWorld={.2f,.8f};g_haveManualInput=false;
        VR().m_rawHeadPose.m[0][3]+=.04f;VR().m_rawHeadPose.m[2][3]+=.03f;
        const float before=g_headingBase,neutralY=VR().m_firstPersonNeutral[1];
        const auto beforePos=pos;
        locomotion::Vec lean;VR().FirstPersonViewOffset(lean.x,lean.z);lean=locomotion::Rotate(lean,before);
        const int ticks=animationTicks;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(std::fabs(std::fabs(Wrap(g_headingBase-before))-kPi)<.0001f,"collision half-turn flips view toward ledge");
        Check(animationTicks==ticks+1 && pos.x_pos==beforePos.x_pos+3 && pos.y_pos==beforePos.y_pos+7 &&
            pos.z_pos==beforePos.z_pos+5,"native animation and collision displacement preserved");
        Check(Near(g_manualWorld.x,-.2f) && Near(g_manualWorld.z,-.8f),"pending movement follows reversed view");
        Check(analogMemory[2]==Angle(g_lastHeadWorld) && analogMemory[3]==analogMemory[2],"native input uses corrected view");
        locomotion::Vec afterLean;VR().FirstPersonViewOffset(afterLean.x,afterLean.z);afterLean=locomotion::Rotate(afterLean,g_headingBase);
        Check(Near(lean.x,afterLean.x) && Near(lean.z,afterLean.z) && neutralY==VR().m_firstPersonNeutral[1],"physical lean and height calibration preserved");
        const float turned=g_headingBase;hangDelta=0;hangState=10;
        for(int frame=0;frame<3;++frame) {
            Detour_LaraAboveWater(itemMemory,nullptr);prev=pos;
            for(fraction=0;fraction<=256;fraction+=64) {
                UpdateSceneCamera(camera);
                Check(Near(g_headingBase,turned) && camera.y_rot==Angle(turned),"hang and repeated stereo/interpolated views never flip again");
            }
        }
        const float headBefore=g_lastHeadWorld;Head(.7f);UpdateSceneCamera(camera);
        Check(Near(Wrap(g_lastHeadWorld-headBefore),.3f),"headset can still look around while hanging");
        for(int excluded:{2,10,30,31,75,56,19}) {
            state=int16_t(excluded);hangState=10;hangDelta=32768;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(Near(g_headingBase,turned),"other catches, shimmies, ladders and mounts keep view");
        }
        state=139;g_active=false;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(Near(g_headingBase,turned),"third person never rotates VR frame");
    }
    for(int delta:{0,364,-364,16384,32768-365,32768+365})
        Check(locomotion::NativeStoppedHangTurn(139,10,0,int16_t(delta))==0,"ordinary alignment and backward drop never flip view");
    std::printf("Ledge hang camera: %d checks passed\n",checks);
}
