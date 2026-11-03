// Exercise the actual simulation hook, including turns AFTER AnimateLara.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
int hangDelta=32768,hangState=10;
bool hangPoseAvailable=false;
float hangPoseYaw=0;
void __cdecl HangJoint(uint8_t* item,tr::PHD_VECTOR* point,int joint,int frac) {
    const auto& pos=*reinterpret_cast<const tr::PHD_3DPOS*>(item+tr::off::item_pos);
    if(joint==0 && hangPoseAvailable) {
        const auto& prev=*reinterpret_cast<const tr::PHD_3DPOS*>(item+tr::off::item_pos_prev);
        const float yaw=tr::Radians(prev.y_rot)+tr::Wrap(tr::Radians(pos.y_rot)-tr::Radians(prev.y_rot))*(frac/256.f)+hangPoseYaw;
        const auto right=tr::locomotion::Rotate({float(point->x),float(point->z)},yaw);
        *point={pos.x_pos+int32_t(std::lround(right.x)),pos.y_pos,pos.z_pos+int32_t(std::lround(right.z))};
        return;
    }
    *point={pos.x_pos,pos.y_pos-700,pos.z_pos};
}
void __cdecl FakeStoppedHang(uint8_t* item,void*) {
    tr::Detour_AnimateLara(item);
    auto& pos=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    pos.y_rot=int16_t(int(pos.y_rot)+hangDelta);
    *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=int16_t(hangState);
    if(hangState==10) *reinterpret_cast<int16_t*>(item+tr::off::item_anim_number)=96;
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
        // Native pelvis rotation drives the camera before item yaw changes.
        // Run both turn directions, every render fraction and duplicate views.
        if(snap==0 && endState==10) for(int clip:{43,48,49}) {
            const float base=g_headingBase;const auto bodyBefore=pos;
            state=139;animation=int16_t(clip);hangPoseAvailable=true;
            g_renderTurn.Sample(0,TurnTime());
            const float direction=clip==49 ? 1.f : -1.f;
            for(float progress:{0.f,.15f,.5f,1.1f,1.9f,2.6f,3.05f,3.18f,3.14f}) {
                hangPoseYaw=Wrap(direction*progress);
                for(fraction=0;fraction<=256;fraction+=32) for(int eye=0;eye<2;++eye) {
                    UpdateSceneCamera(camera);
                    if(!Near(Wrap(g_headingBase-base-direction*std::min(progress,kPi)),0))
                        std::printf("turn mismatch game=%d stable=%d clip=%d progress=%f fraction=%d actual=%f expected=%f active=%d applied=%f\n",which,stable,clip,progress,fraction,Wrap(g_headingBase-base),Wrap(direction*std::min(progress,kPi)),g_active,g_ledgeViewTurn.applied);
                    Check(Near(Wrap(g_headingBase-base-direction*std::min(progress,kPi)),0),"view follows animated turn including 180-degree wrap and duplicate views");
                    Check(pos.y_rot==bodyBefore.y_rot,"following animation never rotates native Lara");
                    Check(g_renderTurn.valid,"animated camera follow preserves stick-turn sampling");
                }
            }
            const float preFinish=g_headingBase;hangDelta=32768;hangState=clip==48 ? 10 : 139;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(std::fabs(Wrap(g_headingBase-preFinish))<.01f,"native half-turn commits only the remainder, with no final snap");
            Check(Near(std::fabs(Wrap(g_headingBase-base)),kPi),"animated turn ends exactly 180 degrees from its start");
            if(hangState==139) {
                g_runtimeEnabled=false;UpdateSceneCamera(camera);
                g_runtimeEnabled=true;UpdateSceneCamera(camera);
                Check(Near(std::fabs(Wrap(g_headingBase-base)),kPi),"suspending a completed turn cannot repeat it on resume");
            }
            state=10;animation=96;
            hangPoseAvailable=false;fraction=256;prev=pos;UpdateSceneCamera(camera);
            Check(Near(std::fabs(Wrap(g_headingBase-base)),kPi),"hang loop keeps completed turn");
            // Interrupted animation must remove only its uncommitted preview.
            const float resumeBase=g_headingBase;state=139;animation=int16_t(clip);
            hangPoseAvailable=true;hangPoseYaw=direction;
            UpdateSceneCamera(camera);Check(Near(Wrap(g_headingBase-resumeBase),direction),"new drop starts a fresh animated turn");
            g_runtimeEnabled=false;UpdateSceneCamera(camera);
            Check(Near(g_headingBase,resumeBase) && g_ledgeViewTurn.animation==-1,"camera suspension cancels partial preview");
            g_runtimeEnabled=true;state=2;animation=103;hangPoseAvailable=false;UpdateSceneCamera(camera);
        }
        hangPoseAvailable=false;
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
        g_active=true;state=139;water=1;Detour_LaraAboveWater(itemMemory,nullptr);water=0;
        Check(Near(g_headingBase,turned),"water transition never rotates VR frame");
        state=139;g_runtimeEnabled=false;Detour_LaraAboveWater(itemMemory,nullptr);g_runtimeEnabled=true;
        Check(Near(g_headingBase,turned),"disabled first person never rotates VR frame");
        // The shared turn helper must retain the existing roll/air-flip path.
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeRollAnimate);
        for(int clip:{146,207,210,212}) {
            state=clip==146 ? 45 : clip==212 ? 25 : 3;animation=int16_t(clip);
            rollNextState=clip==146 ? 23 : state;rollNextAnimation=clip;rollYawDelta=32768;
            const float base=g_headingBase;
            AdvanceLaraAnimation(itemMemory);
            Check(std::fabs(std::fabs(Wrap(g_headingBase-base))-kPi)<.0001f,"ground rolls and airborne flips retain their camera half-turn");
            rollYawDelta=0;AdvanceLaraAnimation(itemMemory);
            Check(std::fabs(std::fabs(Wrap(g_headingBase-base))-kPi)<.0001f,"animation half-turn remains one-shot");
        }
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeAnimate);
    }
    for(int delta:{0,364,-364,16384,32768-365,32768+365})
        Check(locomotion::NativeStoppedHangTurn(139,10,0,int16_t(delta))==0,"ordinary alignment and backward drop never flip view");
    std::printf("Ledge hang camera: %d checks passed\n",checks);
}

