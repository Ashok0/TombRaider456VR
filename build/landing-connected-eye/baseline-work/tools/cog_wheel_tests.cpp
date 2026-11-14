// Exercises the actual interaction/ownership code against a small native-engine
// stand-in. Uses installed animation angles; never loads or executes a game DLL.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include "../src/CogWheelMotion.h"
#include "../src/SwitchPosePath.h"
#include "../src/LocomotionMath.h"

namespace hook {
struct InlineHook {
    void* original=nullptr;
    template<class T>T Original() const{return reinterpret_cast<T>(original);}
    bool Install(void*,void*,size_t,const uint8_t*,size_t,const char*){return true;}
    void Remove(){}
};
}
namespace tr {
using namespace motiongun;
namespace firstperson {
inline float Length(Vec v){return std::sqrt(Dot(v,v));}
inline bool Finite(Frame f){return std::isfinite(f.origin.x)&&std::isfinite(f.origin.y)&&std::isfinite(f.origin.z);}
}
struct PHD_3DPOS {int32_t x_pos,y_pos,z_pos;int16_t x_rot,y_rot,z_rot,padding;};
struct ShotVector {int32_t x,y,z;int16_t room,padding;};
namespace off {constexpr int item_stride=9416,object_stride=3792,item_pos=96,item_hit_points=38,item_room=28,
    object_geom=112,lara_movement_flags=68;}
namespace drva::app_off {constexpr int level=0;}
struct GameDllLayout {uint32_t timestamp;int game;uint32_t objects,anims,laraItem,lara,input,getFloor;};
struct SwitchDll {uint32_t items,count;};
struct MotionDll {uint32_t lineOfSight;};
GameDllLayout layout{0x696B4999,0,0x10000,0x100,0x108,0x200,0x110,0};
GameDllLayout* g_boundDll=&layout;
SwitchDll switchLayout{0x118,0x120};SwitchDll* g_switchDll=&switchLayout;
MotionDll motionLayout{};MotionDll* g_motionDll=&motionLayout;
uint64_t g_boundBase=0;
alignas(16) uint8_t memory[2*1024*1024]{},items[off::item_stride*3]{};
template<class T>T* Ptr(uint32_t at){return reinterpret_cast<T*>(memory+at);}
int currentLevel=1;bool ready=true,visible=true;Vec trackedHand{};
PHD_3DPOS g_scenePose{};
GameDllLayout* GameDllBound(){return g_boundDll;}
uint64_t GameDllBase(){return g_boundBase;}
int AppState(int){return currentLevel;}
bool PhysicalSwitchReady(){return ready;}
float LiveWorldUnitsPerMetre(){return 1000;}
float Radians(int16_t a){return a*3.14159265358979323846f/32768;}
bool SwitchHand(Vec& hand,float& y,float& head){hand=trackedHand;y=head=1;return true;}
bool SwitchPalmLocal(Vec& palm){palm={62,40,0};return true;}
void Log(const char*){}
template<class... T>void LogF(const char*,T...){}
struct SwitchCandidate {int16_t index=-1,state=-1;int level=-1;uint8_t* items=nullptr;uint64_t seen=0;float distance=0;};
uint8_t* SwitchItem(const SwitchCandidate& c){return c.level==currentLevel && c.items==items && c.index>=0 && c.index<3 ? items+c.index*off::item_stride : nullptr;}
using Fn_SwitchCollision=void(__cdecl*)(int16_t,uint8_t*,void*);
using Fn_SwitchControl=void(__cdecl*)(int16_t);
uint8_t* g_switchCommit=nullptr;
firstperson::SwitchAnim animations[600]{};
float measuredAngles[54]{};
float fixtureDirection=1;
uint8_t* wheel=items+off::item_stride;
uint8_t* object=nullptr;
int nativeCalls=0,pulses=0,doorTicks=0,doorTimer=0,pushes=0,collisionCalls=0;
void SwitchPoseMatrices(uint8_t* item,firstperson::SwitchPose pose) {
    *reinterpret_cast<int16_t*>(item+24)=pose.animation;*reinterpret_cast<int16_t*>(item+26)=pose.frame;
    const int phase=pose.animation==501 ? std::clamp(int(pose.frame)-1,0,53) : 0;
    const float a=fixtureDirection*measuredAngles[phase],c=std::cos(a),s=std::sin(a);
    Frame frame{{{{c,0,-s},{0,1,0},{s,0,c}}},{0,-500,0}};
    for(int j=0;j<4;++j) WriteRows(frame,reinterpret_cast<float*>(item+7832)+j*12);
}
void* __cdecl Floor(int32_t,int32_t,int32_t,int16_t*){return nullptr;}
int32_t __cdecl Sight(ShotVector*,ShotVector*){return visible ? 1 : 0;}
void __cdecl NativeCollision(int16_t,uint8_t* lara,void* collision) {
    ++collisionCalls;
    if(collision && (*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(collision)+0x8c)&0x10)) ++pushes;
    if(!(*Ptr<uint64_t>(layout.input)&0x40)) return;
    if(g_switchCommit!=wheel || *reinterpret_cast<int16_t*>(lara+18)!=2 || *reinterpret_cast<int16_t*>(lara+24)!=103) return;
    *reinterpret_cast<uint32_t*>(wheel+0x1820)=2;*reinterpret_cast<int16_t*>(wheel+20)=1;
    *reinterpret_cast<int16_t*>(wheel+58)=2;
    *reinterpret_cast<int16_t*>(lara+24)=322;*reinterpret_cast<int16_t*>(lara+18)=96;
    *Ptr<int16_t>(layout.lara+2)=1;*Ptr<uint32_t>(layout.lara+68)=0;
}
void __cdecl NativeControl(int16_t) {
    ++nativeCalls;
    auto& state=*reinterpret_cast<int16_t*>(wheel+18);auto& goal=*reinterpret_cast<int16_t*>(wheel+20);
    auto& anim=*reinterpret_cast<int16_t*>(wheel+24);auto& frame=*reinterpret_cast<int16_t*>(wheel+26);
    ++frame;
    if(state==0 && goal==1){state=1;anim=501;frame=1;}
    else if(frame>54){if(goal==0){state=0;anim=500;frame=0;}else frame=1;}
    if(state==1) {
        if(goal==1 && !(*Ptr<uint64_t>(layout.input)&0x40)){goal=0;*reinterpret_cast<int16_t*>(items+20)=2;}
        if(*reinterpret_cast<int16_t*>(items+24)==321 && *reinterpret_cast<int16_t*>(items+26)==110){++pulses;doorTimer=40;}
    } else if(frame==0) {
        *reinterpret_cast<uint32_t*>(wheel+0x1820)=0;
        *reinterpret_cast<int16_t*>(items+24)=11;*reinterpret_cast<int16_t*>(items+18)=2;
        *Ptr<int16_t>(layout.lara+2)=0;
    }
}
#include "../src/PhysicalCogWheel.inl"
}

int main(int argc,char** argv) {
    using namespace tr;
    if(argc!=2)return 2;
    std::ifstream input(argv[1]);int checks=0,paths=0;
    auto check=[&](bool result,const char* label){++checks;if(!result){std::printf("FAIL: %s (%d)\n",label,checks);std::exit(1);}};
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    layout.getFloor=uint32_t(reinterpret_cast<uint64_t>(&Floor)-g_boundBase);
    motionLayout.lineOfSight=uint32_t(reinterpret_cast<uint64_t>(&Sight)-g_boundBase);
    *Ptr<uint8_t*>(layout.laraItem)=items;*Ptr<uint8_t*>(switchLayout.items)=items;*Ptr<int32_t>(switchLayout.count)=3;
    *Ptr<firstperson::SwitchAnim*>(layout.anims)=animations;
    int16_t dummy=0;animations[500].frames=animations[501].frames=animations[321].frames=&dummy;
    animations[501].state=1;animations[501].first=1;animations[501].last=54;
    animations[501].jump=501;animations[501].jumpFrame=1;animations[501].interpolation=1;
    animations[321].state=96;animations[321].first=100;animations[321].last=153;
    g_hCogControl.original=reinterpret_cast<void*>(&NativeControl);g_hCogCollision.original=reinterpret_cast<void*>(&NativeCollision);
    uint8_t collision[144]{};auto& collisionFlags=*reinterpret_cast<uint32_t*>(collision+0x8c);
    while(input>>measuredAngles[0]) {
        for(int i=1;i<54;++i) input>>measuredAngles[i];check(bool(input),"complete measured asset path");++paths;
        ResetPhysicalCogs();std::memset(items,0,sizeof(items));nativeCalls=pulses=doorTimer=pushes=0;
        layout.game=paths==5 ? 1 : 0;g_cogDll=&kCogDlls[layout.game];
        const int slot=layout.game ? 278 : 317;
        object=Ptr<uint8_t>(layout.objects)+slot*off::object_stride;
        *reinterpret_cast<int16_t*>(object)=paths==1 || paths==4 ? 4 : 3;
        *reinterpret_cast<int16_t*>(object+66)=500;
        *reinterpret_cast<uint64_t*>(object+24)=g_boundBase+g_cogDll->control;
        *reinterpret_cast<uint64_t*>(object+56)=g_boundBase+g_cogDll->collision;
        fixtureDirection=paths%2 ? 1.f : -1.f;
        *reinterpret_cast<int16_t*>(items+18)=2;*reinterpret_cast<int16_t*>(items+24)=103;*reinterpret_cast<int16_t*>(items+38)=1000;
        reinterpret_cast<PHD_3DPOS*>(items+96)->z_pos=-900;
        *reinterpret_cast<int16_t*>(wheel+16)=int16_t(slot);*reinterpret_cast<int16_t*>(wheel+24)=500;
        *Ptr<uint64_t>(layout.input)=0x800;*Ptr<uint32_t>(layout.lara+68)=0x8000;
        collisionFlags=0x91;ready=true;visible=true;
        Detour_CogCollision(1,items,collision);
        check(pushes==0 && collisionFlags==0x91,"wheel approach relieves only push flag and restores collision flags");
        ready=false;Detour_CogCollision(1,items,collision);check(pushes==1,"third-person collision unchanged");ready=true;
        auto setHand=[&](float angle){trackedHand={700*std::cos(angle),-500,700*std::sin(angle)};};
        float angle=-firstperson::CogPegAngle*4;setHand(angle);
        visible=false;check(!UpdatePhysicalCogGrip(true,true,1),"cannot grab through wall");UpdatePhysicalCogGrip(false,true,2);visible=true;
        check(UpdatePhysicalCogGrip(true,true,3),"grab visible radial peg without Action icon");
        const uint64_t start=10;
        for(uint64_t tick=start;tick<start+100;++tick){AdvancePhysicalCog(items,collision,tick);Detour_CogControl(1);}
        check(nativeCalls==0 && pulses==0,"stationary grip never starts wheel or opens door");
        for(int step=1;step<=160;++step) {
            angle+=fixtureDirection*firstperson::CogPegAngle/40;setHand(angle);
            AdvancePhysicalCog(items,collision,110+step);
            check(g_cogClaimed,"continuous hand remains attached across animation wrap");
            check(*reinterpret_cast<int16_t*>(items+24)==103 && *reinterpret_cast<int16_t*>(items+18)==2 &&
                  *Ptr<uint64_t>(layout.input)==0x800 && *Ptr<uint32_t>(layout.lara+68)==0x8000 && *Ptr<int16_t>(layout.lara+2)==0,
                  "native wheel control restores Lara/input/locomotion");
            const int before=nativeCalls;Detour_CogControl(1);check(nativeCalls==before,"normal tick cannot free-run physical wheel");
        }
        check(pulses==4,"four peg advances produce four native door pulses");
        const int before=pulses,callsBefore=nativeCalls;const int16_t parked=*reinterpret_cast<int16_t*>(wheel+26);
        UpdatePhysicalCogGrip(false,true,280);
        for(int i=0;i<60;++i){Detour_CogControl(1);if(doorTimer>0)--doorTimer;}
        check(pulses==before && nativeCalls==callsBefore && doorTimer==0,"release holds wheel pose while native door timer runs down");
        check(*reinterpret_cast<int16_t*>(wheel+26)==parked,"release retains wheel position");
        check(UpdatePhysicalCogGrip(true,true,350),"regrab current peg after release");
        AdvancePhysicalCog(items,collision,351);check(pulses==before,"regrab does not replay pulse");
        const float sameAngle=angle;
        for(int i=0;i<8;++i){angle-=fixtureDirection*.01f;setHand(angle);AdvancePhysicalCog(items,collision,352+i);}
        for(int i=0;i<8;++i){angle+=fixtureDirection*.01f;setHand(angle);AdvancePhysicalCog(items,collision,360+i);}
        check(std::fabs(angle-sameAngle)<.001 && pulses==before,"back-and-forth jitter cannot pump door");
        setHand(angle+1.5f);AdvancePhysicalCog(items,collision,368);
        check(!g_cogClaimed && pulses==before,"tracking teleport releases hand without pulse");
        check(*reinterpret_cast<int16_t*>(wheel+26)==parked,"tracking loss preserves wheel phase");
        ResetPhysicalCogs();check(!(*reinterpret_cast<uint32_t*>(wheel+0x1820)&6),"mode reset runs native wheel stop/removal");
        check(*reinterpret_cast<int16_t*>(items+24)==103 && *Ptr<uint64_t>(layout.input)==0x800,"native stop cannot overwrite Lara's actual pose");
        setHand(-firstperson::CogPegAngle*4);
        check(UpdatePhysicalCogGrip(true,true,400),"new grab after native cleanup");
        setHand(-firstperson::CogPegAngle*4+fixtureDirection*.1f);AdvancePhysicalCog(items,collision,401);
        const int afterStart=nativeCalls;
        *reinterpret_cast<int16_t*>(wheel+20)=0;
        check(!FindCog(1),"native/script goal change revokes physical ownership");
        ResetPhysicalCogs();check(nativeCalls==afterStart,"cleanup cannot overwrite a script-owned wheel");
        *reinterpret_cast<int16_t*>(wheel+18)=0;*reinterpret_cast<int16_t*>(wheel+24)=500;
        *reinterpret_cast<int16_t*>(wheel+26)=0;*reinterpret_cast<uint32_t*>(wheel+0x1820)=0;
        setHand(-firstperson::CogPegAngle*4);
        check(UpdatePhysicalCogGrip(true,true,450),"grab before level change");
        setHand(-firstperson::CogPegAngle*4+fixtureDirection*.1f);AdvancePhysicalCog(items,collision,451);
        const int beforeLevelChange=nativeCalls;
        ++currentLevel;ResetPhysicalCogs();check(nativeCalls==beforeLevelChange,"level transition discards stale wheel without native writes");
    }
    check(paths==5,"all installed cog variants checked");
    firstperson::CogDrive drive;drive.Grab(3.13f);float advance=0;
    check(drive.Track(-3.13f,advance) && advance>0 && advance<.03f,"hand angle crosses +/-pi without a jump");
    check(!drive.Track(NAN,advance),"invalid hand rotation rejected");
    std::printf("PASS: %d cog interaction checks across %d installed animation paths; no game DLL loaded\n",checks,paths);
}
