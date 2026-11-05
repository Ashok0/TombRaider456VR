// Linked with first_person_tests: use the actual first-person palette capture,
// dynamic-bone observer and solver, with only the render-state boundary stubbed.
#include <cstdlib>
#include "../src/FirstPerson.h"
#define private public
#include "../src/InlineHook.h"
#undef private
#include "../src/DynamicBones.cpp"

namespace {
tr::RenderState renderState{};
bool worldPass=true;
int observedDrawType=-1;
void* observedDrawItem=nullptr;
void __fastcall ObserveDrawArguments(void* item,int32_t drawType) {
    observedDrawItem=item;observedDrawType=drawType;
}
void __fastcall ObserveDrawArguments5(void* item) { observedDrawItem=item;observedDrawType=-5; }
void Require(bool ok,const char* message) {
    if (!ok) { std::printf("FAIL: jiggle integration: %s\n",message);std::exit(1); }
}
}

void TestDynamicBonesMirrorArguments() {
    using namespace tr;
    const auto saved=g_hDrawLaraHD.m_trampoline;
    const auto savedDll=g_boundDll;GameDllLayout layout{};g_boundDll=&layout;
    g_hDrawLaraHD.m_trampoline=reinterpret_cast<void*>(&ObserveDrawArguments);
    for(int drawType=0;drawType<3;++drawType) {
        Detour_DrawLaraHD(&renderState,drawType);
        Require(observedDrawItem==&renderState && observedDrawType==drawType,
                "HD scope forwards the native reflection/interpolation flag exactly");
        Require(!g_inLara,"HD scope ends after every native draw");
    }
    layout.game=1;g_hDrawLaraHD.m_trampoline=reinterpret_cast<void*>(&ObserveDrawArguments5);
    Detour_DrawLaraHD5(&renderState);
    Require(observedDrawItem==&renderState && observedDrawType==-5 && !g_inLara,
            "TR5 retains its distinct one-argument native draw entry");
    g_hDrawLaraHD.m_trampoline=saved;g_boundDll=savedDll;
}

void TestDynamicBonesMirrorIsolation(const float* palette) {
    using namespace tr;
    Require(FirstPersonNativeMirrorDrawing(),"fixture is inside actual native mirror scope");
    renderState.joints=const_cast<float*>(palette);renderState.num_joints=15;renderState.shader=20;
    const bool previous=g_inLara;g_inLara=true;worldPass=true;
    g_haveThis=g_haveBest=false;g_bestScore=-1;g_candCount=0;
    DynamicBonesObserveDraw();
    Require(!DynamicBonesRenderBody() && !g_haveThis && !g_haveBest && g_candCount==0,
            "reflected joints cannot drive or receive live-body spring deformation");
    g_inLara=previous;
}
namespace tr {
RenderState& VidState() { return renderState; }
bool IsWorldPass() { return worldPass; }
bool BoneSkinActive() { return true; }
}

void TestDynamicBonesFrameStability() {
    using namespace tr;
    auto& config=const_cast<Config&>(Cfg());const auto savedConfig=config;
    const auto savedTorso=g_torsoThis;
    const auto item=*reinterpret_cast<uint8_t**>(GameDllBase()+GameDllBound()->laraItem);
    auto& flags=*reinterpret_cast<uint32_t*>(item+kItemFlags);
    auto& fall=*reinterpret_cast<int16_t*>(item+kItemFallspeed);
    const auto savedFlags=flags;const auto savedFall=fall;
    config.dynamicBonesDriveMode=1;config.dynamicBonesAirGravity=1;
    config.dynamicBonesStiffness=630;config.dynamicBonesDamping=9.5f;
    config.dynamicBonesGravity=5400;config.dynamicBonesAxis=1;
    config.dynamicBonesMaxDisplace=40;config.dynamicBonesDebugScale=1;
    ResetSolver();g_haveThis=true;g_torsoThis={};
    g_torsoThis.m[0]=g_torsoThis.m[5]=g_torsoThis.m[10]=1;
    flags&=~(1u<<kGravityStatusBit);fall=0;
    for(int frame=0;frame<600;++frame) Solve(1.f/90);
    for(auto& bone:g_bone) bone.x[1]+=1;
    const float times[]={1.f/90,.09f,1.f/60,.075f,.095f};
    float offset[3]{};
    for(int frame=0;frame<200;++frame) {
        Solve(times[frame%5]);
        Require(DynamicBonesWorldOffset(offset),"irregular-frame solver remains live");
        Require(std::fabs(offset[1])<2 && std::fabs(offset[0])+std::fabs(offset[2])<.001f,
                "long frames must damp a small disturbance instead of amplifying it into clamp-sized bounce");
    }
    Require(std::fabs(offset[1])<.001f && g_jumps==0 && g_lands==0,
            "grounded chest settles without fabricated jump or landing events");
    ResetSolver();g_haveThis=true;Solve(1.f/90);
    Require(DynamicBonesWorldOffset(offset) && std::fabs(offset[1])<.001f,
            "first visible frame starts at authored rest without an upward chest pop");
    g_torsoThis.m[3]+=2000;Solve(1.f/90);
    Require(DynamicBonesWorldOffset(offset) && std::fabs(offset[1])<.001f,
            "camera rebase resets to equilibrium rather than restarting a bounce");
    config.dynamicBonesLandImpulse=.2f;
    float referencePeak=0;
    const int frameRates[]={120,90,60,20,12,10};
    for(int hz:frameRates) {
        ResetSolver();g_haveThis=true;g_torsoThis={};
        g_torsoThis.m[0]=g_torsoThis.m[5]=g_torsoThis.m[10]=1;
        flags&=~(1u<<kGravityStatusBit);fall=0;
        for(int frame=0;frame<hz;++frame) Solve(1.f/hz);
        flags|=1u<<kGravityStatusBit;fall=100;
        for(int frame=0;frame<hz;++frame) Solve(1.f/hz);
        flags&=~(1u<<kGravityStatusBit);fall=0;
        float peak=0;
        for(int frame=0;frame<3*hz;++frame) {
            Solve(1.f/hz);DynamicBonesWorldOffset(offset);
            peak=std::fmax(peak,offset[1]);
            // A 10 Hz display samples after the first crest. Compare the
            // physical response at the sample time, not an unseen peak.
            const float time=float(frame+1)/hz,decay=config.dynamicBonesDamping*.5f;
            const float frequency=std::sqrt(config.dynamicBonesStiffness-decay*decay);
            const float expected=600.f/frequency*std::exp(-decay*time)*std::sin(frequency*time);
            Require(std::fabs(offset[1]-expected)<1.f,
                    "landing follows the damped-spring response even when display frames miss its crest");
            Require(std::isfinite(offset[1]) && std::fabs(offset[1])<30,
                    "normal landing stays bounded at both low and headset frame rates");
        }
        if(hz==120) referencePeak=peak;

        Require(peak>8 && (hz<12 || std::fabs(peak-referencePeak)<referencePeak*.25f) && std::fabs(offset[1])<.01f,
                "normal landing retains its bounce strength and settles at 10 through 120 Hz");
        Require(g_jumps==1 && g_lands==1,"substeps apply each landing event exactly once");
    }
    flags=savedFlags;fall=savedFall;config=savedConfig;
    ResetSolver();g_torsoThis=savedTorso;
}

void TestDynamicBonesVisibility(const float* rendered) {
    using namespace tr;
    uint64_t mask=0;int count=0;
    const float* full=FirstPersonBodyPalette(mask,count);
    Require(full && (count==15 || count==33),"real first-person capture is available");
    float native[33*12]{};
    std::memcpy(native,rendered ? rendered : full,count*12*sizeof(float));
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

    TestDynamicBonesFrameStability();

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

// Project an actual FP torso capture plus the shader's chest displacement.
// Re-run identical spring history so a camera-space change cannot be mistaken
// for a different phase of jiggle. Caller compares clear and wall-blocked eyes.
void TestDynamicBonesChestPosition(float out[3]) {
    using namespace tr;
    uint64_t mask=0;int count=0;
    const float* full=FirstPersonBodyPalette(mask,count);
    Require(full && count==15,"wall fixture supplies production body capture");
    const auto oldBound=g_boundDll;g_boundDll=GameDllBound();
    renderState.joints=const_cast<float*>(full);renderState.num_joints=count;renderState.shader=20;
    g_inLara=true;worldPass=true;ResetSolver();
    for(int frame=0;frame<90;++frame) {
        g_haveBest=g_haveThis=false;g_bestScore=-1;g_candCount=0;
        DynamicBonesObserveDraw();
        Require(DynamicBonesRenderBody() && g_haveThis,"wall torso retains chest solver");
        Solve(1.f/90.f);
    }
    float displacement[3]{};
    Require(DynamicBonesWorldOffset(displacement),"wall torso retains shader displacement");
    const float* posed=FirstPersonRenderBodyPalette(mask,count);
    Require(posed!=nullptr,"chest rendering uses this draw's posed body palette");
    // Fully torso-weighted chest vertex. This is the same order as BoneSkin:
    // skin with posed joint 7, then add weighted uDynBone in that same space.
    const float* torso=posed+7*12;
    for(int axis=0;axis<3;++axis)
        out[axis]=torso[axis*4+1]*-170.f+torso[axis*4+2]*45.f+torso[axis*4+3]
            +displacement[axis]*Cfg().dynamicBonesChestStrength;
    g_inLara=false;ResetSolver();g_boundDll=oldBound;
}
