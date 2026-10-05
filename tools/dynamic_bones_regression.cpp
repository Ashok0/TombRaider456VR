// Linked with first_person_tests: use the actual first-person palette capture,
// dynamic-bone observer and solver, with only the render-state boundary stubbed.
#include <cstdlib>
#include "../src/FirstPerson.h"
#include "../src/DynamicBones.cpp"

namespace {
tr::RenderState renderState{};
bool worldPass=true;
void Require(bool ok,const char* message) {
    if (!ok) { std::printf("FAIL: jiggle integration: %s\n",message);std::exit(1); }
}
}
namespace tr {
RenderState& VidState() { return renderState; }
bool IsWorldPass() { return worldPass; }
bool BoneSkinActive() { return true; }
}

void TestDynamicBonesVisibility() {
    using namespace tr;
    uint64_t mask=0;int count=0;
    const float* full=FirstPersonBodyPalette(mask,count);
    Require(full && (count==15 || count==33),"real first-person capture is available");
    float native[33*12]{};
    std::memcpy(native,full,count*12*sizeof(float));
    for(int j=0;j<count;++j)
        if (!(mask&(uint64_t(1)<<j))) std::memset(native+j*12,0,12*sizeof(float));
    const auto oldBound=g_boundDll;
    g_boundDll=GameDllBound();
    renderState.joints=native;renderState.num_joints=count;renderState.shader=20;
    g_inLara=true;g_lockShader=20;worldPass=true;
    auto observe=[]() {
        g_haveBest=g_haveThis=g_lockSeen=false;
        g_bestScore=-1;g_candCount=0;
        DynamicBonesObserveDraw();
    };
    observe();
    Require(DynamicBonesRenderBody(),"hidden arms must not disable chest deformation");
    Require(g_haveThis && g_lockSeen,"hidden arms must keep the solver and its shader lock alive");
    Require(std::memcmp(g_best.joints,full,count*12*sizeof(float))==0,
            "physics uses complete captured skeleton, not zeroed hidden joints");
    Require(std::memcmp(g_torsoThis.m,full+Cfg().dynamicBonesTorsoJoint*12,12*sizeof(float))==0,
            "torso anchor matches rendered full palette");

    // Identical animation/engine input must produce identical physics whether
    // native joints are visible or masked. Exercise actual spring integration.
    const auto item=*reinterpret_cast<uint8_t**>(GameDllBase()+GameDllBound()->laraItem);
    auto& flags=*reinterpret_cast<uint32_t*>(item+kItemFlags);
    auto& fall=*reinterpret_cast<int16_t*>(item+kItemFallspeed);
    const auto savedFlags=flags;const auto savedFall=fall;
    Bone baseline[90][2]{};
    for(int run=0;run<2;++run) {
        ResetSolver();g_lockShader=20;
        renderState.joints=run==0 ? const_cast<float*>(full) : native;
        for(int frame=0;frame<90;++frame) {
            const bool airborne=frame>=20 && frame<50;
            flags=(savedFlags & ~(1u<<kGravityStatusBit)) |
                  (airborne ? 1u<<kGravityStatusBit : 0);
            fall=airborne ? int16_t((frame-30)*3) : 0;
            observe();
            Require(DynamicBonesRenderBody() && g_haveThis,"body remains eligible every physics frame");
            Solve(1.f/90.f);
            if(run==0) std::memcpy(baseline[frame],g_bone,sizeof(g_bone));
            else Require(std::memcmp(baseline[frame],g_bone,sizeof(g_bone))==0,
                         "arm visibility cannot alter any jump/landing physics frame");
        }
        Require(g_jumps==1 && g_lands==1,"takeoff and landing still drive the spring");
        Require(g_settled,"solver stays live");
        float offset[3]{};
        Require(DynamicBonesWorldOffset(offset),"shader receives a physics offset");
        for(float value:offset) Require(std::isfinite(value),"physics offset stays finite");

    }
    flags=savedFlags;fall=savedFall;
    renderState.joints=native;
    worldPass=false;observe();
    Require(!DynamicBonesRenderBody() && !g_haveThis,"shadow/UI draws remain excluded");
    worldPass=true;g_inLara=false;observe();
    Require(!DynamicBonesRenderBody(),"draws outside Lara remain excluded");
    g_inLara=true;g_lockShader=12;observe();
    Require(DynamicBonesRenderBody() && !g_haveThis,"other body materials render without driving locked solver");
    // A stale or incompatible capture must never be read for another palette.
    renderState.num_joints=count-1;
    std::memset(native,0,sizeof(native));
    observe();
    Require(!DynamicBonesRenderBody(),"mismatched palette count falls back to native attachment classification");
    g_inLara=false;ResetSolver();g_boundDll=oldBound;
    std::printf("PASS: jiggle integration, %d joints, mask %llx\n",count,
                static_cast<unsigned long long>(mask));
}

// Called after leaving the first-person body scope, and during tracked hands.
void TestDynamicBonesNativeDraws() {
    using namespace tr;
    uint64_t mask=0;int count=0;
    Require(!FirstPersonBodyPalette(mask,count),"body capture cannot leak into other draws");
    const auto oldBound=g_boundDll;g_boundDll=GameDllBound();
    float native[15*12]{};
    for(int j=0;j<15;++j) {
        native[j*12]=native[j*12+5]=native[j*12+10]=1.f;
        native[j*12+3]=float(100+j*30);
    }
    renderState.joints=native;renderState.num_joints=15;renderState.shader=20;
    g_inLara=true;worldPass=true;ResetSolver();
    DynamicBonesObserveDraw();
    Require(DynamicBonesRenderBody() && g_haveThis,"native body physics still works without FP capture");
    Require(std::memcmp(g_best.joints,native,sizeof(native))==0,"native draws retain their own skeleton");
    for(int j=0;j<15;++j) native[j*12+3]=float((j%2)*30);
    DynamicBonesObserveDraw();
    Require(!DynamicBonesRenderBody(),"two-joint attachments must not receive chest physics");
    g_inLara=false;ResetSolver();g_boundDll=oldBound;
}
