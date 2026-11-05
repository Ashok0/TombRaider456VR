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
int currentLevel=1;bool ready=true,visible=true,tracking=true;Vec trackedHand{};float trackedHeight=1.5f;
PHD_3DPOS g_scenePose{};
GameDllLayout* GameDllBound(){return g_boundDll;}
uint64_t GameDllBase(){return g_boundBase;}
int AppState(int){return currentLevel;}
bool PhysicalSwitchReady(){return ready;}
float fixtureScale=1000;
float LiveWorldUnitsPerMetre(){return fixtureScale;}
float Radians(int16_t a){return a*3.14159265358979323846f/32768;}
bool SwitchHand(Vec& hand,float& y,float& head){hand=trackedHand;y=trackedHeight;head=1.7f;return tracking;}
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
int16_t pulleyWords[1400]{};
int pulleyRate=1,pulleySize=11;
float bindShift=0;
bool onTrigger=true,nativeAccept=true;
int pullEvents=0;
struct RoomCollision { uint8_t bytes[144]{}; };
Frame IdentityFrame(){return {{{{1,0,0},{0,1,0},{0,0,1}}},{}};}
bool ReadIKBind(const uint8_t*,int,int,const int32_t*,const float*,int joint,Frame& bind) {
    bind=IdentityFrame();bind.origin={bindShift+joint*3.f,0,0};return true;
}
void SwitchPoseMatrices(uint8_t* item,firstperson::SwitchPose pose) {
    *reinterpret_cast<int16_t*>(item+24)=pose.animation;*reinterpret_cast<int16_t*>(item+26)=pose.frame;
    const int i=pose.frame/pulleyRate,n=std::min(i+1,animations[pose.animation].last/pulleyRate);
    const float f=float(pose.frame%pulleyRate)/pulleyRate;
    float y=pulleyWords[i*pulleySize+7]+f*(pulleyWords[n*pulleySize+7]-pulleyWords[i*pulleySize+7]);
    // Canonical Z-up mesh becomes game Y-down. Include nonidentity bind
    // offsets so forgetting inverse-bind multiplication breaks contact tests.
    Frame world{{{{1,0,0},{0,0,1},{0,-1,0}}},{-1,y-(layout.game==1 ? 510.f : 0.f),-1}};
    for(int j=0;j<9;++j) {
        Frame bind=IdentityFrame();bind.origin={bindShift+j*3.f,0,0};
        WriteRows(Multiply(world,bind),reinterpret_cast<float*>(item+7832)+j*12);
    }
}
void* __cdecl Floor(int32_t,int32_t,int32_t,int16_t*){return nullptr;}
int32_t __cdecl Sight(ShotVector*,ShotVector*){return visible ? 1 : 0;}
#include "../src/PhysicalCogWheel.inl"
#include "../src/PhysicalPulley.inl"
int32_t __cdecl PulleyQuery(uint8_t* probe,int16_t* links) {
    *reinterpret_cast<int16_t*>(probe+28)=99;
    if(!onTrigger)return 0;links[0]=1;return 1;
}
void __cdecl PulleyNativeCollision(int16_t index,uint8_t* lara,void*) {
    ++collisionCalls;
    if(!nativeAccept || !(*Ptr<uint64_t>(layout.input)&0x40))return;
    if(g_switchCommit!=wheel || *reinterpret_cast<int16_t*>(lara+18)!=2 || *reinterpret_cast<int16_t*>(lara+24)!=103)return;
    *reinterpret_cast<uint32_t*>(wheel+0x1820)=3;
    *reinterpret_cast<int16_t*>(lara+18)=104;*reinterpret_cast<int16_t*>(lara+24)=339;
    *Ptr<int16_t>(layout.lara+0x15A)=index;*Ptr<int16_t>(layout.lara+0x15C)=index;
    *Ptr<int16_t>(layout.lara+2)=1;*Ptr<uint32_t>(layout.lara+68)=0;
    std::memset(Ptr<uint8_t>(layout.lara+224),0,12);
}
void __cdecl PulleyNativeControl(int16_t index) {
    ++nativeCalls;auto* target=items+index*off::item_stride;
    auto& timer=*reinterpret_cast<int16_t*>(target+42);if(timer>0)--timer;
    auto& frame=*reinterpret_cast<int16_t*>(target+26);if(++frame>animations[500].last)frame=0;
}
void __cdecl PulleyLaraControl(uint8_t* lara,void* collision) {
    if(*Ptr<int16_t>(layout.lara+0x15A)!=1 || *reinterpret_cast<int16_t*>(lara+24)!=340 ||
       *reinterpret_cast<int16_t*>(lara+26)!=animations[340].first+44)std::abort();
    ++pullEvents;
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(collision)+140)&=~0x30u;
    *reinterpret_cast<int16_t*>(lara+20)=104;
    auto& count=*reinterpret_cast<int16_t*>(wheel+48);auto& latch=*reinterpret_cast<int16_t*>(wheel+60);
    if(--count==0) {
        *reinterpret_cast<uint32_t*>(wheel+0x1820)=5;latch=1;
        const int setting=*reinterpret_cast<int16_t*>(wheel+62);
        if(setting<0)*reinterpret_cast<int16_t*>(wheel+56)=1;else count=int16_t(setting);
    } else if(latch) { latch=0;*reinterpret_cast<uint32_t*>(wheel+0x1820)=5; }
}
}
int main(int argc,char** argv) {
    using namespace tr;
    if(argc!=2)return 2;std::ifstream input(argv[1]);int checks=0,paths=0;
    auto check=[&](bool ok,const char* label){++checks;if(!ok){std::printf("FAIL %s path=%d check=%d\n",label,paths,checks);std::exit(1);}};
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](auto fn){return uint32_t(reinterpret_cast<uint64_t>(fn)-g_boundBase);};
    layout.getFloor=rva(&Floor);motionLayout.lineOfSight=rva(&Sight);
    *Ptr<uint8_t*>(layout.laraItem)=items;*Ptr<uint8_t*>(switchLayout.items)=items;*Ptr<int32_t>(switchLayout.count)=3;
    *Ptr<firstperson::SwitchAnim*>(layout.anims)=animations;
    PulleyDll fixture{};g_pulleyDll=&fixture;
    fixture.collision=rva(&PulleyNativeCollision);fixture.control=rva(&PulleyNativeControl);
    fixture.laraControl=rva(&PulleyLaraControl);fixture.switchTrigger=rva(&PulleyQuery);
    g_hPulleyCollision.original=reinterpret_cast<void*>(&PulleyNativeCollision);
    g_hPulleyControl.original=reinterpret_cast<void*>(&PulleyNativeControl);
    int game=0,wordCount=0;
    while(input>>game>>pulleyRate>>pulleySize>>wordCount) {
        ++paths;ResetPhysicalPulleys();std::memset(items,0,sizeof(items));std::memset(animations,0,sizeof(animations));
        std::memset(Ptr<uint8_t>(layout.lara),0,512);
        for(int i=0;i<wordCount;++i)input>>pulleyWords[i];check(bool(input),"complete installed animation fixture");
        layout.game=game-4;fixtureScale=paths%2 ? 423.f : 1000.f;ready=visible=tracking=onTrigger=nativeAccept=true;pullEvents=nativeCalls=collisionCalls=0;bindShift=paths*17.f;
        object=Ptr<uint8_t>(layout.objects)+(game==4 ? 321 : 282)*off::object_stride;
        std::memset(object,0,off::object_stride);
        *reinterpret_cast<int16_t*>(object)=game==4 ? 9 : 1;
        *reinterpret_cast<int16_t*>(object+66)=500;
        *reinterpret_cast<uint64_t*>(object+24)=g_boundBase+fixture.control;
        *reinterpret_cast<uint64_t*>(object+56)=g_boundBase+fixture.collision;
        *reinterpret_cast<const char**>(object+off::object_geom)="PULLEY";
        animations[500].frames=pulleyWords;animations[500].interpolation=int16_t((pulleySize<<8)|pulleyRate);
        animations[500].last=game==4 ? 72 : 58;animations[500].jump=500;animations[500].jumpFrame=game==4 ? 15 : 0;
        animations[339].frames=animations[340].frames=pulleyWords;animations[339].state=animations[340].state=104;
        animations[340].first=100;animations[340].last=158;
        auto& remaining=*reinterpret_cast<int16_t*>(wheel+48);auto& setting=*reinterpret_cast<int16_t*>(wheel+62);
        auto& status=*reinterpret_cast<uint32_t*>(wheel+0x1820);
        auto resetItem=[&](int count) {
            ResetPhysicalPulleys();std::memset(wheel,0,off::item_stride);
            *reinterpret_cast<int16_t*>(wheel+16)=int16_t(game==4 ? 321 : 282);*reinterpret_cast<int16_t*>(wheel+24)=500;
            remaining=int16_t(std::abs(count));setting=int16_t(count);
            *reinterpret_cast<int16_t*>(items+18)=*reinterpret_cast<int16_t*>(items+20)=2;
            *reinterpret_cast<int16_t*>(items+24)=103;*reinterpret_cast<int16_t*>(items+38)=1000;
            reinterpret_cast<PHD_3DPOS*>(items+96)->z_pos=-200;
            *Ptr<uint64_t>(layout.input)=0x800;*Ptr<uint32_t>(layout.lara+68)=0x8000;
            *Ptr<int32_t>(layout.lara+0x15A)=0x000b000a;
            std::memset(Ptr<uint8_t>(layout.lara+224),0x35,12);
            trackedHand={20,-650,0};trackedHeight=1.5f;pullEvents=nativeCalls=collisionCalls=0;
        };
        uint64_t now=100;
        auto grab=[&](){UpdatePhysicalPulleyGrip(false,true,++now);return UpdatePhysicalPulleyGrip(true,true,++now);};
        auto step=[&](float delta){trackedHand.y+=delta*fixtureScale/1000;trackedHeight-=delta/1000;AdvancePhysicalPulley(items,nullptr,++now);};
        auto finishReturn=[&]() {
            UpdatePhysicalPulleyGrip(false,true,++now);
            for(int n=0;n<12;++n){now+=50;AdvancePhysicalPulley(items,nullptr,now);}
            trackedHand={20,-650,0};trackedHeight=1.5f;
        };
        resetItem(3);
        check(grab(),"RG grabs actual chain/rope without Action input/icon");
        for(int n=0;n<80;++n){step(n%2 ? -2.f : 2.f);check(pullEvents==0,"jitter cannot count as pull");}
        step(100);check(g_pulleyClaimed && pullEvents==0,"partial pull keeps hand and does not trigger");
        const auto parked=g_physicalPulleys[0].progress;
        UpdatePhysicalPulleyGrip(false,true,++now);AdvancePhysicalPulley(items,nullptr,++now);
        check(g_physicalPulleys[0].progress==parked,"release retains partial progress");
        trackedHand.y=-700;check(grab(),"regrip partial pull at a new reachable height");
        step(-40);check(std::fabs(g_physicalPulleys[0].progress-(parked-40*fixtureScale/1000))<.01,"upward motion reverses only current pull");
        for(int n=0;n<29;++n)step(10);
        check(pullEvents==1 && remaining==2,"one complete stroke invokes exactly one native pull");
        check(*reinterpret_cast<int16_t*>(items+18)==2 && *reinterpret_cast<int16_t*>(items+20)==2 &&
              *reinterpret_cast<int16_t*>(items+24)==103 && *Ptr<uint64_t>(layout.input)==0x800 &&
              *Ptr<uint32_t>(layout.lara+68)==0x8000 && *Ptr<int16_t>(layout.lara+2)==0 &&
              *Ptr<int32_t>(layout.lara+0x15A)==0x000b000a && *Ptr<uint8_t>(layout.lara+224)==0x35 &&
              *reinterpret_cast<int16_t*>(items+28)==0,"native call restores Lara, room, angles, interaction indices and input");
        for(int n=0;n<100;++n)step(n%2 ? -1.f : 1.f);
        check(pullEvents==1,"held grip cannot repeat the completed event");
        *reinterpret_cast<int16_t*>(wheel+42)=10;const auto pose=*reinterpret_cast<int16_t*>(wheel+26);
        for(int n=0;n<5;++n)Detour_PulleyControl(1);
        check(nativeCalls==5 && *reinterpret_cast<int16_t*>(wheel+42)==5 && *reinterpret_cast<int16_t*>(wheel+26)==pose,
              "native timers tick normally while only visual frame is held");
        Frame wrist=IdentityFrame();AttachPhysicalPulleyHand(1,wrist,trackedHand);
        check(firstperson::Length(Sub(motiongun::Transform(wrist,{62,40,0}),g_pulleyGrip))<.01,"visible palm meets rendered rope with inverse-bind offsets");
        for(int stroke=0;stroke<2;++stroke) {
            finishReturn();check(grab(),"fresh grip rearms after completed pull returns");
            for(int n=0;n<35;++n)step(10);
        }
        check(pullEvents==3 && remaining==3 && *reinterpret_cast<int16_t*>(wheel+60)==1 && (status&6)==4,
              "three pulls preserve resettable native count and queue native floor trigger");
        finishReturn();check(!grab(),"pending native trigger cannot be double-committed");
        status=0;check(grab(),"native floor consumption permits another cycle");
        for(int n=0;n<35;++n)step(10);
        check(remaining==2 && *reinterpret_cast<int16_t*>(wheel+60)==0 && (status&6)==4,"native latch-off event is preserved");
        resetItem(-1);check(grab(),"one-use pulley is grabbable");for(int n=0;n<35;++n)step(10);
        check(remaining==0 && *reinterpret_cast<int16_t*>(wheel+56)==1 && (status&6)==4,"negative original count remains native one-use");
        finishReturn();check(!grab(),"exhausted one-use cannot be pulled again");
        resetItem(1);*reinterpret_cast<int16_t*>(wheel+56)=1;check(grab(),"floor one-use marker does not block first pull");
        resetItem(2);visible=false;check(!grab(),"wall occlusion blocks grabbing");visible=true;
        onTrigger=false;check(!grab(),"wrong floor trigger blocks grabbing");onTrigger=true;
        *reinterpret_cast<int16_t*>(wheel+58)=1;check(!grab(),"locked or special-case pulley remains native");*reinterpret_cast<int16_t*>(wheel+58)=0;
        *reinterpret_cast<uint16_t*>(wheel+44)=0x100;check(!grab(),"used pulley remains unavailable");*reinterpret_cast<uint16_t*>(wheel+44)=0;
        *reinterpret_cast<int16_t*>(wheel+16)=int16_t(game==4 ? 135 : 126);check(!grab(),"trap chains and climbing ropes excluded");
        *reinterpret_cast<int16_t*>(wheel+16)=int16_t(game==4 ? 321 : 282);
        ready=false;check(!grab(),"third person unchanged");ready=true;
        check(grab(),"reacquire after guards");step(30);onTrigger=false;for(int n=0;n<32;++n)step(10);
        check(!g_pulleyClaimed && pullEvents==0 && remaining==2,"moving off real trigger cancels commit");onTrigger=true;
        resetItem(2);check(grab(),"grab before tracking jump");step(500);check(!g_pulleyClaimed && !pullEvents,"tracking teleport cannot activate pulley");
        resetItem(2);check(grab(),"grab before virtual height change");trackedHand.y+=100;step(0);
        check(g_pulleyClaimed && !pullEvents && g_physicalPulleys[0].progress==0,"virtual movement alone never counts as a physical pull");
        resetItem(2);check(grab(),"grab before loss of tracking");tracking=false;step(10);
        check(!g_pulleyClaimed && !pullEvents,"lost pose cannot complete a pull");tracking=true;
        resetItem(2);check(grab(),"grab before invalid pose");trackedHand.y=NAN;step(0);
        check(!g_pulleyClaimed && !pullEvents && g_physicalPulleys[0].progress==0,"nonfinite pose cannot poison retained progress");
        resetItem(2);check(grab(),"grab before clock reversal");now-=20;step(10);
        check(!g_pulleyClaimed && !pullEvents,"reversed clock releases safely");
        resetItem(2);nativeAccept=false;check(grab(),"grab before native refusal");for(int n=0;n<35;++n)step(10);
        check(!g_pulleyClaimed && !pullEvents && remaining==2 && *Ptr<int32_t>(layout.lara+0x15A)==0x000b000a,"native refusal does not write a counter or leave Lara locked");nativeAccept=true;
        resetItem(2);animations[500].commandCount=1;check(!grab(),"animation with unverified commands stays native");animations[500].commandCount=0;
        if(game==4) {
            resetItem(2);*reinterpret_cast<int16_t*>(wheel+26)=72;
            check(!grab(),"saved native mid-pull pose first returns without a physical event");
            UpdatePhysicalPulleyGrip(false,true,++now);
            for(int n=0;n<36;++n){now+=50;AdvancePhysicalPulley(items,nullptr,now);}
            check(grab() && !pullEvents && remaining==2,"saved pose returns safely and rearms");
        }
        resetItem(2);check(grab(),"grab before timeout");now+=251;step(1);check(!g_pulleyClaimed,"stale tracking releases");
        resetItem(2);check(grab(),"grab before lateral escape");trackedHand.x+=250*fixtureScale/1000;step(0);check(!g_pulleyClaimed,"sideways departure releases");
        resetItem(2);check(grab(),"grab before competing owner");check(!UpdatePhysicalPulleyGrip(true,false,++now),"other interaction owns grip exclusively");
        resetItem(2);check(grab(),"grab before level change");++currentLevel;step(20);check(!g_pulleyClaimed && g_physicalPulleys.empty(),"level change discards stale item");
        resetItem(2);check(grab(),"grab before native state change");*reinterpret_cast<int16_t*>(wheel+48)=1;step(20);
        check(!g_pulleyClaimed && remaining==1,"external mechanism state is never rolled back");
        resetItem(2);check(grab(),"grab before native takeover");step(50);UpdatePhysicalPulleyGrip(false,true,++now);
        *Ptr<uint64_t>(layout.input)|=0x40;Detour_PulleyCollision(1,items,nullptr);
        check(!FindPulley(1) && remaining==2,"native Action releases preview without restoring old mechanism flags");
        resetItem(2);check(grab(),"grab before mode change");step(50);ready=false;AdvancePhysicalPulley(items,nullptr,++now);
        check(!g_pulleyClaimed && g_physicalPulleys.empty() && remaining==2 && *reinterpret_cast<int16_t*>(wheel+26)==0,
              "mode change restores only visual pose");ready=true;
        *reinterpret_cast<int16_t*>(items+2*off::item_stride+42)=3;const int before=nativeCalls;Detour_PulleyControl(2);
        check(nativeCalls==before+1 && *reinterpret_cast<int16_t*>(items+2*off::item_stride+42)==2,"shared control passes unrelated switches through once");
    }
    check(paths==12,"all installed pulley animation paths tested");
    std::printf("PASS: %d focused pulley checks across %d installed paths\n",checks,paths);
}
