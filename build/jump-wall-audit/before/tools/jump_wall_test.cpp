#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
int wallKind=0;float wallX=0,wallZ=1;
void __cdecl AuditWall(tr::RoomCollision* c,int32_t x,int32_t y,int32_t z,int16_t,int32_t height) {
    Check(c->radius==64,"rendered-eye sweep uses eye clearance radius");
    for(int i=0;i<18;i+=3) {
        c->floorSamples[i]=-y;c->floorSamples[i+1]=-4096-y+height;
    }
    const float at=x*wallX+z*wallZ;
    if(at+64>=100) {
        if(wallKind==1)c->type=8;
        if(wallKind==2)c->floorSamples[3]=-768-y;
    }
}
}
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p){return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.frameFrac=rva(&fraction);dll.getCollisionInfo=rva(reinterpret_cast<void*>(&AuditWall));
    dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));g_boundDll=&dll;
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
    auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
    config=Config{};config.enabled=config.firstPerson=true;
    config.positionalTracking=config.firstPersonHeadTranslation=true;
    config.firstPersonMovementStabilization=true;
    config.firstPersonRoomscaleMove=config.firstPersonBodyFollowsHead=false;
    config.firstPersonFullBodyIK=false;
    VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;
    for(int which:{0,1}) for(bool collide:{false,true}) for(float heading:{0.f,.8f,-1.9f}) {
        game=dll.game=which;water=0;std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0?-1:0;
        g_active=g_haveHeading=g_scenePoseValid=true;g_headingItem=itemMemory;g_headingBase=heading;
        config.firstPersonWallCollision=collide;Head(0);VR().RecenterFirstPersonHead();
        float neutral[3];std::copy(VR().m_firstPersonNeutral,VR().m_firstPersonNeutral+3,neutral);
        wallX=std::sin(heading);wallZ=std::cos(heading);
        for(int s:{0,1,2,3,9,12,15,25,26,27,28,29,56,57,58,59,60,61})
        for(int f:{0,64,128,192,256}) for(float lean:{0.f,.08f,.4f}) for(int kind:{0,1,2}) {
            state=int16_t(s);fraction=f;wallKind=kind;pos=prev={};
            pos.y_pos=-1024;pos.y_rot=prev.y_rot=Angle(heading);
            Head(.5f);VR().m_rawHeadPose.m[2][3]=neutral[2]-lean;
            const auto desired=locomotion::Rotate({0,200},heading);
            PHD_3DPOS eye{int32_t(std::lround(desired.x)),-700-4*f,int32_t(std::lround(desired.z)),0,0,0,0};
            const auto before=eye;const auto native=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            ClampRenderedHeadToCollision(itemMemory,eye);
            const auto tracked=locomotion::Rotate({0,collide?lean*1000:0},heading);
            const float along=(eye.x_pos+tracked.x)*wallX+(eye.z_pos+tracked.z)*wallZ;
            const bool blocked=kind==1 || (kind==2 && before.y_pos+64>=-768);
            if(blocked && along>=36.5f)std::printf("wall case game%d state%d frac%d tracked%d lean%.2f kind%d along%.2f\n",which,s,f,int(collide),lean,kind,along);
            if(blocked)Check(along<36.5f,"standing, jumping and ladder eye cannot pass a wall at eye height");
            else {
                Check(along>190,"open space and jumping above crate tops retain forward eye clearance");
                if(!collide || lean<.3f)Check(eye.x_pos==before.x_pos && eye.z_pos==before.z_pos,"safe jump eye is not retracted");
            }
            Check(eye.y_pos==before.y_pos,"wall sweep preserves jump height");
            Check(native==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),"rendered clearance cannot change native jump/collision state");
            Check(!std::memcmp(neutral,VR().m_firstPersonNeutral,sizeof(neutral)),"wall contact never changes tracking calibration");
        }
    }
    VR().m_system=nullptr;
    std::printf("PASS: %d jump and wall clearance checks\n",checks);
}
