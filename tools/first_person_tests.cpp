// Compile with MSVC /std:c++17 /O2 /Gy /EHsc and /link /OPT:REF.
// Exercises the actual implementation with engine memory and native calls
// stubbed. No game process, OpenVR runtime, or live code patching is involved.
// From a VS x64 developer prompt at the repo root:
// cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX
//    /Ithird_party\openvr\headers /Isrc tools\first_person_tests.cpp
//    tools\dynamic_bones_regression.cpp
//    /Febuild\first_person_tests.exe /Fobuild\ /link /OPT:REF user32.lib
// build\first_person_tests.exe
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <openvr.h>
#include "../src/StereoMath.h"
#define private public
#include "../src/VRSystem.h"
#include "../src/InlineHook.h"
#undef private
#include "../src/FirstPerson.cpp"
#include "../src/VRSystem.cpp"
#include "../src/Gamepad.cpp"

// Separate translation unit executes the real dynamic-bone observer/solver.
void TestDynamicBonesVisibility();
void TestDynamicBonesNativeDraws();

namespace {
tr::Config config;
tr::Layout layout{};
tr::GameDllLayout dll{};
alignas(16) uint8_t appMemory[4096]{};
alignas(16) uint8_t itemMemory[9416]{};
alignas(16) uint8_t laraMemory[448]{};
alignas(16) uint8_t cameraMemory[112]{};
int16_t analogMemory[28]{};
uint64_t actionInput = 0, seenInput = 0;
int16_t seenAnalog[4]{}, seenYaw = 0, seenMove = 0, seenTurn = 0;
int animationTicks = 0, simulationTicks = 0;
int collisionMode = 0, collisionCalls = 0, floorCalls = 0, roomChanges = 0;
int cameraCollisionCalls = 0;
float impactNormalX=0,impactNormalZ=1;
int cutseq = 0, floorToken = 0, draws = 0, hairs = 0;
tr::PHD_VECTOR iconPoints[20]{}, seenIconPoints[20]{};
int iconCount=0, iconDraws=0, iconPerspective=1000, iconCenterX=960, iconCenterY=540;
float iconMatrix[12]{}, iconNear=16, iconFar=32768;
void __cdecl FakeActionIcons() {
    ++iconDraws;
    std::memcpy(seenIconPoints,iconPoints,sizeof(iconPoints));
}
int cutseqNumber=0, cutseqTransition=0, spotCamera=0;
uint8_t vonCroyScene=0;
bool nativeMoves = true, jointFollowsBody = false;
bool worldCameraValid = false, opticsZoomed=false;
float worldCameraRot[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
float worldCameraPos[3] = {};
uint8_t* laraItem = itemMemory;
uint8_t* motionApp = appMemory;
int fraction = 128, game = 0, water = 0, checks = 0;
bool jointCalled = false;
void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) { std::printf("FAIL: %s\n", label); std::exit(1); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.002f; }
int nativeShotCalls=0, nativeShotHand=-1, handDrawCalls=0;
uint32_t handDrawMask=0;
void __cdecl FakeHandJoint(uint8_t* item,tr::PHD_VECTOR* p,int joint,int) {
    Check(item==itemMemory && (joint==10 || joint==13),"tracked shot queries correct native wrist");
    const auto& body=*reinterpret_cast<const tr::PHD_3DPOS*>(item+tr::off::item_pos);
    *p={body.x_pos,body.y_pos,body.z_pos};
}
int32_t __cdecl FakeFire(int32_t,void*,void*,const int16_t*) {
    ++nativeShotCalls; nativeShotHand=tr::g_firingHand;
    return -1; // A native miss is still a successfully fired shot.
}
int longCalls=0, pelletChecks=0, initCalls=0, opticMode=-1;
tr::PHD_3DPOS crossbowArgument{};
bool crossbowWasNull=false, sawOpticScope=false;
alignas(16) uint8_t projectileMemory[9416]{};
uint8_t* projectileItems=projectileMemory;
void* __cdecl FakeProjectileFloor(int32_t,int32_t,int32_t,int16_t* room) {
    *room=9; return nullptr;
}
void __cdecl FakeProjectileInit(int16_t index) {
    Check(index==0,"only newly created player projectile is initialized");
    ++initCalls;
    std::memcpy(projectileMemory+tr::off::item_pos_prev,
        projectileMemory+tr::off::item_pos,sizeof(tr::PHD_3DPOS));
}
void __cdecl FakeShotgun() {
    ++longCalls;
    if (!tr::g_longShot) return;
    for (int pellet=0;pellet<6;++pellet) {
        const int16_t aim[]={int16_t(tr::g_longShot->baseAim[0]+(pellet-3)*120),
                             int16_t(tr::g_longShot->baseAim[1]+(pellet-2)*80)};
        Check(tr::Detour_FireWeapon(4,nullptr,nullptr,aim)==-1,"all six native pellets fire");
        Check(tr::g_firingAim[0]==tr::g_longShot->baseAim[0] &&
              tr::g_firingAim[1]==tr::g_longShot->baseAim[1],
              "pellet spread is measured from volley base, not erased per pellet");
        ++pelletChecks;
    }
}
void __cdecl FakeHK(int32_t running) {
    Check(running==7,"HK native argument preserved");
    ++longCalls;
    if (tr::g_longShot)
        tr::Detour_FireWeapon(5,nullptr,nullptr,tr::g_longShot->baseAim);
}
void __cdecl FakeGrenade() {
    ++longCalls;
    if (!tr::g_longShot) return;
    const auto& gun=tr::g_longShot->gun;
    const auto& body=*reinterpret_cast<tr::PHD_3DPOS*>(itemMemory+tr::off::item_pos);
    Check(int16_t(*reinterpret_cast<int16_t*>(laraMemory+tr::off::lara_left_arm+16)+body.x_rot)==gun.pitch &&
          int16_t(*reinterpret_cast<int16_t*>(laraMemory+tr::off::lara_left_arm+14)+body.y_rot)==gun.yaw,
          "launcher native angles include body-relative pitch and yaw exactly once");
    tr::PHD_VECTOR point{0,276,80};
    tr::Detour_GunJoint(itemMemory,&point,10);
    Check(std::abs(point.x-gun.muzzle.x)<=0.51f && std::abs(point.y-gun.muzzle.y)<=0.51f &&
          std::abs(point.z-gun.muzzle.z)<=0.51f,"grenade native joint starts at tracked muzzle");
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(projectileMemory+tr::off::item_pos);
    p={point.x,point.y,point.z,gun.pitch,gun.yaw,0,0};
    *reinterpret_cast<int16_t*>(projectileMemory+tr::off::item_object)=0x16d;
    tr::Detour_InitialiseProjectile(0);
}
void __cdecl FakeCrossbow(tr::PHD_3DPOS* pose) {
    ++longCalls; crossbowWasNull=pose==nullptr;
    if (pose) crossbowArgument=*pose;
    sawOpticScope=tr::g_opticShot;
    if (pose && tr::g_longShot) {
        *reinterpret_cast<tr::PHD_3DPOS*>(projectileMemory+tr::off::item_pos)=*pose;
        *reinterpret_cast<int16_t*>(projectileMemory+tr::off::item_object)=0x158;
        tr::Detour_InitialiseProjectile(0);
    }
}
void __cdecl FakeRifle(int32_t) {
    for (unsigned offset=tr::off::lara_head_y_rot;offset<=tr::off::lara_torso_z_rot;offset+=2)
        *reinterpret_cast<int16_t*>(laraMemory+offset)=800;
}
int32_t __cdecl FakeOptic(tr::PHD_VECTOR* start,tr::PHD_VECTOR* end,int32_t,int32_t mode) {
    opticMode=mode;
    Check(start->x!=999 && end->x!=999,"optic receives private tracked vectors, not camera aliases");
    if (mode && tr::CurrentMotionWeapon()==6) {
        tr::PHD_3DPOS p{start->x,start->y,start->z,1234,2345,0,0};
        tr::Detour_FireCrossbow(&p);
    }
    return 42;
}
void __cdecl FakeHandDraw(uint8_t* item,int32_t useBits,int32_t) {
    Check(tr::FirstPersonTrackedHandJoint()==-1,"each hand draw starts without a stale wrist shader index");
    ++handDrawCalls;
    handDrawMask=*reinterpret_cast<uint32_t*>(item+tr::off::item_mesh_bits);
    Check(useBits==1 && ((tr::g_renderArm==1 && handDrawMask==0x400) ||
          (tr::g_renderArm==0 && handDrawMask==0x2000)),
          "actual draw retains only the tracked hand and gun");
}
void __cdecl FakeUziHandler(int32_t weapon) {
    Check(weapon==3,"native Uzi ID is 3");
    const int16_t aim[2]={};
    if (tr::FireWeaponForCaller(tr::g_motionDll->rightFireReturn,weapon,nullptr,nullptr,aim))
        *reinterpret_cast<int16_t*>(laraMemory+tr::off::lara_right_arm+20)=3;
    if (tr::FireWeaponForCaller(tr::g_motionDll->leftFireReturn,weapon,nullptr,nullptr,aim))
        *reinterpret_cast<int16_t*>(laraMemory+tr::off::lara_left_arm+20)=3;
}
bool shotVisible=true;
alignas(16) uint8_t targetItems[3][9416]{};
uint8_t* targetItemsPtr=targetItems[0];
int16_t activeTarget=0;
uint8_t targetObjects[3792]{};
struct TestSphere { int32_t x,y,z,r; };
int __cdecl FakeTargetSpheres(uint8_t* item,TestSphere* out,int flags) {
    Check(flags==1,"controller selection requests world-space hit spheres");
    const auto& pos=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    out[0]={pos.x_pos,pos.y_pos,pos.z_pos,80}; return 1;
}
void __cdecl FakeCandidatePoint(uint8_t* item,tr::ShotVector* out) {
    const auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    *out={p.x_pos,p.y_pos,p.z_pos,1,0};
}
void* __cdecl FakeCandidateFloor(int32_t,int32_t,int32_t,int16_t*) { return nullptr; }
int __cdecl FakeCandidateLOS(tr::ShotVector*,tr::ShotVector* end) { return end->y>-500; }
void* seenAnimationTarget=nullptr;
void __cdecl FakePistolAnimation(int32_t) {
    seenAnimationTarget=*reinterpret_cast<void**>(laraMemory+tr::off::lara_target);
}
int shotSightCalls=0, shotTargetCalls=0;
tr::ShotVector shotTarget{100,100,1300,9,0};
void __cdecl FakeShotTarget(void*, tr::ShotVector* end) {
    ++shotTargetCalls; *end=shotTarget;
}
void* __cdecl FakeShotFloor(int32_t x,int32_t y,int32_t z,int16_t* room) {
    Check(x==100 && y==200 && z==300 && *room==7,"assist resolves muzzle room from Lara room");
    *room=8; return nullptr;
}
int32_t __cdecl FakeShotSight(tr::ShotVector* start,tr::ShotVector* end) {
    ++shotSightCalls;
    Check(start->room==8 && end->room==9,"assist uses full room-aware vectors");
    Check(start->x==100 && start->y==200 && start->z==300,"assist visibility starts at muzzle");
    end->z=500; // Native LOS is allowed to clip its private endpoint.
    return shotVisible ? 1 : 0;
}
void __cdecl FakeJoint(uint8_t* item, tr::PHD_VECTOR* v, int joint, int frac) {
    Check(item == itemMemory && joint == 14 && frac == fraction, "native joint arguments");
    const int state = *reinterpret_cast<const int16_t*>(item + tr::off::item_anim_state);
    const int anchorZ = tr::locomotion::FirstPersonAnchorZ(
        state, config.firstPersonAnchorZ, config.firstPersonInteractionAnchorZ);
    Check(v->x == 0 && v->y == -32 && v->z == anchorZ,
          "state-specific local eye offset passed to engine");
    *v = { 0, -700, anchorZ };
    if (jointFollowsBody) {
        const auto& pos = *reinterpret_cast<tr::PHD_3DPOS*>(item + tr::off::item_pos);
        const auto& prev = *reinterpret_cast<tr::PHD_3DPOS*>(item + tr::off::item_pos_prev);
        const float t = fraction / 256.0f;
        v->x += static_cast<int>(prev.x_pos + (pos.x_pos - prev.x_pos) * t);
        v->z += static_cast<int>(prev.z_pos + (pos.z_pos - prev.z_pos) * t);
    }
    jointCalled = true;
}
int shadowJointCalls=0, shadowJointFraction=0;
uint8_t* shadowJointItem=nullptr;
tr::PHD_VECTOR shadowNativePoint{1000,-400,2000};
void __cdecl FakeShadowJoint(uint8_t* item,tr::PHD_VECTOR* point,int32_t,int32_t frac) {
    ++shadowJointCalls;shadowJointFraction=frac;shadowJointItem=item;
    *point=shadowNativePoint;
}
int16_t nativeAimLock=0;
const float* skinFixture=nullptr;
int skinFixtureCount=0;
int32_t __cdecl FakeSkinJoints(uint8_t*,float* joints,int32_t) {
    if (skinFixture) {
        std::memcpy(joints,skinFixture,skinFixtureCount*12*sizeof(float));
        return skinFixtureCount;
    }
    std::memset(joints,0,15*12*sizeof(float));
    for (int i=0;i<15;++i) joints[i*12]=joints[i*12+5]=joints[i*12+10]=1.f;
    return 15;
}
void __cdecl FakeAim(void*, uint8_t* arm) {
    *reinterpret_cast<int16_t*>(arm + 12) = nativeAimLock;
    *reinterpret_cast<int16_t*>(arm + 14) = 123;
    *reinterpret_cast<int16_t*>(arm + 16) = 456;
}
void __cdecl FakeAnimate(uint8_t* item) {
    ++animationTicks;
    auto& pos = *reinterpret_cast<tr::PHD_3DPOS*>(item + tr::off::item_pos);
    pos.x_pos += 3; pos.y_pos += 7; pos.z_pos += 5;
    *reinterpret_cast<int16_t*>(item + tr::off::item_speed) = 10;
}
int rollNextState=23,rollYawDelta=32768,rollNextAnimation=-1;
void __cdecl FakeRollAnimate(uint8_t* item) {
    FakeAnimate(item);
    auto& pos=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    pos.y_rot=int16_t(int(pos.y_rot)+rollYawDelta);
    *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=int16_t(rollNextState);
    if (rollNextAnimation>=0)
        *reinterpret_cast<int16_t*>(item+tr::off::item_anim_number)=int16_t(rollNextAnimation);
    // turn180_effect's native analog transition fields must remain untouched.
    analogMemory[4]=45;analogMemory[5]=pos.y_rot;
}
tr::locomotion::Vec unstableStep{};
int unstableHeight=0, nativeCollisionPasses=0;
bool blockStableMotion=false, startAirborne=false;
int animationNextState=-1, animationNextGoal=-1;
bool animationStartsGravity=false;
bool animationRetainsSpeed=false;
bool collisionStartsFall=false;
tr::locomotion::Vec collisionPush{};
tr::locomotion::Vec beforeNativeCollision{};
tr::PHD_VECTOR gaitSway{};
void __cdecl FakeUnstableAnimate(uint8_t* item) {
    ++animationTicks;
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    p.x_pos+=int32_t(unstableStep.x); p.z_pos+=int32_t(unstableStep.z);
    p.y_pos+=unstableHeight;
    auto& speed=*reinterpret_cast<int16_t*>(item+tr::off::item_speed);
    if (animationRetainsSpeed) {
        // Native gravity animation inherits speed instead of overwriting it
        // from the grounded animation table every tick.
        p.x_pos-=int32_t(unstableStep.x); p.z_pos-=int32_t(unstableStep.z);
        const float angle=tr::Radians(*reinterpret_cast<int16_t*>(laraMemory+tr::off::lara_move_angle));
        const auto step=tr::locomotion::Rotate({0,float(speed)},angle);
        p.x_pos+=int32_t(std::round(step.x)); p.z_pos+=int32_t(std::round(step.z));
    } else speed=int16_t(tr::locomotion::Length(unstableStep));
    if (startAirborne) *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=3;
    if (animationNextState>=0) *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=int16_t(animationNextState);
    if (animationNextGoal>=0) *reinterpret_cast<int16_t*>(item+tr::off::item_goal_state)=int16_t(animationNextGoal);
    if (animationStartsGravity) *reinterpret_cast<uint32_t*>(item+0x1820)|=8;
}
void __cdecl FakeUnstableAboveWater(uint8_t* item,void*) {
    ++simulationTicks;
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    const auto previous=p;
    tr::Detour_AnimateLara(item);
    beforeNativeCollision={float(p.x_pos-previous.x_pos),float(p.z_pos-previous.z_pos)};
    ++nativeCollisionPasses;
    if (blockStableMotion) { p.x_pos=previous.x_pos; p.z_pos=previous.z_pos; }
    p.x_pos+=int32_t(collisionPush.x); p.z_pos+=int32_t(collisionPush.z);
    if (collisionStartsFall) {
        *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=3;
        *reinterpret_cast<int16_t*>(item+tr::off::item_goal_state)=3;
        *reinterpret_cast<uint32_t*>(item+0x1820)|=8;
    }
}
void __cdecl FakeGaitTransitionAnimate(uint8_t* item) {
    ++animationTicks;
    auto& state=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_state);
    auto& frame=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_number+2);
    if (animationNextState>=0) state=int16_t(animationNextState);
    ++frame;
    // Model the native ordering: select/advance animation, read its velocity,
    // then translate along lara.move_angle. The outgoing run has NOT become
    // a slow sidestep just because this tick's input asks for one.
    const int16_t speed=state==1 ? 70 : state==0 ? 24 : state==2 ? 32 : 12;
    *reinterpret_cast<int16_t*>(item+tr::off::item_speed)=speed;
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    const float angle=tr::Radians(*reinterpret_cast<int16_t*>(laraMemory+tr::off::lara_move_angle));
    p.x_pos+=int32_t(std::round(std::sin(angle)*speed));
    p.z_pos+=int32_t(std::round(std::cos(angle)*speed));
    p.y_pos+=7;
}
alignas(8) uint8_t immediateGaitAnimations[42*48]{};
const uint8_t* immediateGaitTable=immediateGaitAnimations;
void __cdecl FakeBackpedalAnimate(uint8_t* item) {
    ++animationTicks;
    auto& anim=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_number);
    auto& frame=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_number+2);
    ++frame;
    if (anim==41 && frame>759) { anim=40; frame=684; }
    // Actual stock TR4/5 startup/loop speeds from the installed PDP tables.
    const int16_t speed=anim==41 ? 2 : 10;
    *reinterpret_cast<int16_t*>(item+tr::off::item_speed)=speed;
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    p.z_pos-=speed; ++p.y_pos;
}
bool blockGaitStart=false;
int standingGaitChecks=0;
void __cdecl FakeImmediateGaitAboveWater(uint8_t* item,void* collision) {
    auto& state=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_state);
    auto& anim=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_number);
    if (state==2 && anim==11) {
        ++standingGaitChecks; // Native lara_as_stop checks destination terrain.
        const auto action=actionInput&tr::locomotion::Directions;
        animationNextState=blockGaitStart ? 2 : action==tr::locomotion::StepLeft ? 22 :
            action==tr::locomotion::StepRight ? 21 : action==tr::locomotion::Forward ? 1 : 16;
        *reinterpret_cast<int16_t*>(item+tr::off::item_goal_state)=int16_t(animationNextState);
    } else animationNextState=-1;
    FakeUnstableAboveWater(item,collision);
    if (state==1) anim=0;
    if (state==22) anim=65;
    if (state==21) anim=67;
    if (state==16) anim=41;
}
void __cdecl FakeGaitJoint(uint8_t* item,tr::PHD_VECTOR* v,int joint,int frac) {
    Check(joint==14,"stabilized eye still queries native head joint");
    const auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    const auto& old=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos_prev);
    const float t=frac/256.f;
    const float yaw=tr::Radians(old.y_rot)+tr::Wrap(tr::Radians(p.y_rot)-tr::Radians(old.y_rot))*t;
    const auto flat=tr::locomotion::Rotate({float(v->x+gaitSway.x),float(v->z+gaitSway.z)},yaw);
    *v={int32_t(std::lround(old.x_pos+(p.x_pos-old.x_pos)*t+flat.x)),
        int32_t(std::lround(old.y_pos+(p.y_pos-old.y_pos)*t-700+gaitSway.y)),
        int32_t(std::lround(old.z_pos+(p.z_pos-old.z_pos)*t+flat.z))};
}
void __cdecl FakeAboveWater(uint8_t* item, void*) {
    ++simulationTicks;
    seenInput = actionInput;
    std::copy(analogMemory, analogMemory + 4, seenAnalog);
    seenYaw = reinterpret_cast<tr::PHD_3DPOS*>(item + tr::off::item_pos)->y_rot;
    seenMove = *reinterpret_cast<int16_t*>(laraMemory + tr::off::lara_move_angle);
    seenTurn = *reinterpret_cast<int16_t*>(laraMemory + tr::off::lara_turn_rate);
    if (nativeMoves) tr::Detour_AnimateLara(item);
}
void __cdecl FakeCollision(tr::RoomCollision* c, int32_t x, int32_t y, int32_t z,
                          int16_t room, int32_t height) {
    if (c->radius == 64) {
        ++cameraCollisionCalls;
        const bool airborne = c->badPos == 4096 && c->badNeg == -4096;
        Check((airborne || (c->badPos == 384 && c->badNeg == -384)) &&
              c->badCeiling == 0 && c->flags == (airborne ? 0 : 5) && height == 762,
              "camera uses native ground or airborne collision parameters");
        Check(std::hypot(float(x - c->old[0]), float(z - c->old[2])) <= 16+std::sqrt(2.f),
              "rendered eye sweep advances at most 16 units plus two-axis integer rounding");
        Check(room == *reinterpret_cast<int16_t*>(itemMemory + tr::off::item_room),
              "camera sweep starts in Lara's room");
        for (int sample=0;sample<18;sample+=3) {
            c->floorSamples[sample]=0; c->floorSamples[sample+1]=-1024;
        }
        c->floorSamples[0] = collisionMode == 12 ? 1000 : 0;
        if (collisionMode==14 && x*impactNormalX+z*impactNormalZ>=100)
            c->floorSamples[0]=-768; // raised crate, no synthetic shift needed
        c->floorSamples[1] = -1024;
        if (collisionMode == 11 && x >= 100) c->shift[0] = 99 - x;
        if (collisionMode == 13) c->type = 8;
        if (collisionMode==15) {
            for (int sample=0;sample<18;sample+=3) {
                c->floorSamples[sample]=-y;
                c->floorSamples[sample+1]=-4096-(y-height);
            }
            if (x*impactNormalX+z*impactNormalZ+64>=100)
                c->floorSamples[3]=-768-y; // front radius sample hits crate before eye centre
        }
        if (collisionMode==16 || collisionMode==17 || collisionMode==18) {
            for (int sample=0;sample<18;sample+=3) {
                c->floorSamples[sample]=collisionMode==17 ? -32512 : int32_t(x/4)-y;
                c->floorSamples[sample+1]=(collisionMode==18 ? -600 : -1600)-(y-height);
            }
        }
        (void)y;
        return;
    }
    ++collisionCalls;
    Check(c->radius == 100 && c->badPos == 384 && c->badNeg == -384 &&
          c->badCeiling == 0 && c->flags == 5 && height == 762,
          "native room-scale query uses verified standing collision parameters");
    Check(std::hypot(float(x - c->old[0]), float(z - c->old[2])) <= 32+std::sqrt(2.f),
          "physical displacement split into short collision sweeps");
    Check(room == *reinterpret_cast<int16_t*>(itemMemory + tr::off::item_room),
          "each sweep uses current Lara room");
    c->floorSamples[0] = 0; c->floorSamples[1] = -1024;
    if (collisionMode == 1) { c->shift[0] = c->old[0] - x; c->shift[2] = c->old[2] - z; }
    if (collisionMode == 2) c->floorSamples[0] = 385;
    if (collisionMode == 3) c->floorSamples[1] = 0;
    if (collisionMode >= 4 && collisionMode <= 6) c->type = int16_t(8 << (collisionMode - 4));
    if (collisionMode == 7) c->floorSamples[0] = -385;
    if (collisionMode == 8) c->shift[0] = 1000;
    if (collisionMode == 9 && x > 50) c->shift[0] = 50 - x;
    (void)y;
}
void* __cdecl FakeFloor(int32_t x, int32_t y, int32_t z, int16_t* room) {
    ++floorCalls;
    const auto& pos = *reinterpret_cast<tr::PHD_3DPOS*>(itemMemory + tr::off::item_pos);
    Check(x == pos.x_pos && y == pos.y_pos - 381 && z == pos.z_pos,
          "room update samples Lara's accepted position with native height offset");
    if (collisionMode == 10 && x >= 64) *room = 2;
    return &floorToken;
}
int32_t __cdecl FakeHeight(void* floor, int32_t, int32_t, int32_t) {
    Check(floor == &floorToken, "floor result passed to native height query");
    return 123;
}
void __cdecl FakeNewRoom(int16_t item, int16_t room) {
    Check(item == *reinterpret_cast<int16_t*>(laraMemory), "room update uses Lara item index");
    ++roomChanges;
    *reinterpret_cast<int16_t*>(itemMemory + tr::off::item_room) = room;
}
uint32_t drawnBodyBits=0;
int drawnUseBits=-1, drawnPass=-1;
void __cdecl FakeDraw(uint8_t* item, int32_t useBits, int32_t pass) {
    ++draws;
    drawnBodyBits=*reinterpret_cast<uint32_t*>(item+tr::off::item_mesh_bits);
    drawnUseBits=useBits; drawnPass=pass;
}
void __cdecl FakeHair(int32_t) { ++hairs; }
void Head(float yaw, float pitch = 0) {
    auto& vr = tr::VR();
    const float c = std::cos(yaw), s = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    vr.m_rawHeadPose = { { {c, -s*sp, -s*cp, 1},
                          {0, cp, -sp, 1.7f},
                          {s, c*sp, c*cp, -2} } };
    vr.m_headFromTracking = tr::InvertRigid(tr::FromHmd(vr.m_rawHeadPose));
}
}
void Log(const char*) {}
void LogF(const char*, ...) {}
namespace hook {
InlineHook::~InlineHook() {}
void InlineHook::Remove() {}
bool InlineHook::Install(void*, void*, size_t, const uint8_t*, size_t,
                         const char*, const int*, size_t) { return false; }
}
namespace tr {
const Config& Cfg() { return config; }
const motiongun::Calibration& LiveMotionGunCalibration() {
    static motiongun::Calibration calibration; return calibration;
}
void AdjustMotionGunCalibration(int) {}
bool SaveMotionGunCalibration() { return false; }
void RestoreMotionGunCalibration() {}
float LiveWorldUnitsPerMetre() { return 1000.0f; }
float LiveIpdScale() { return 1.0f; }
uint64_t Base() { return reinterpret_cast<uint64_t>(appMemory); }
const Layout& L() { return layout; }
int CurrentGame() { return game; }
void* XInputGetStateSlot() { return nullptr; }
bool IsOpticsZoomed() { return opticsZoomed; }
bool CameraHeadroom(float&) { return false; }
bool CameraViewFrame(float rot[3][3], float pos[3]) {
    if (!worldCameraValid) return false;
    std::memcpy(rot, worldCameraRot, sizeof(worldCameraRot));
    std::memcpy(pos, worldCameraPos, sizeof(worldCameraPos));
    return true;
}
int LaraWaterStatus() { return water; }
const GameDllLayout* GameDllBound() { return g_boundDll; }
uint64_t GameDllBase() { return g_boundBase; }
}

int main() {
    using namespace tr;
    // Optical axis follows gun yaw/pitch/roll, not the headset; projected
    // barrel-ray points remain under the reticle at every distance.
    for (int yaw=-180;yaw<=180;yaw+=30) for (int pitch=-60;pitch<=60;pitch+=30)
    for (int roll:{-45,0,45}) {
        motiongun::Calibration c{};c.yawDegrees=float(yaw);c.pitchDegrees=float(pitch);c.rollDegrees=float(roll);
        const motiongun::Basis identity{{{1,0,0},{0,1,0},{0,0,1}}};
        const auto basis=motiongun::GunBasis(motiongun::CalibratedController(identity,c));
        const motiongun::Frame optic{basis,{100,-200,300}};
        // Use a translated, rotated and scaled inverse bind: identity-only
        // fixtures conceal the original mesh-space vs wrist-space mistake.
        auto inv=motiongun::Frame{motiongun::CalibratedController(identity,c),{-63,20,-10}};
        for (auto& row:inv.basis.r) for (float& v:row) v*=2.f;
        motiongun::Frame attached{};float radius=0;
        Check(hkscope::MeshLens(optic,inv,attached,radius),"mesh eyepiece attachment is valid across wrist rotations");
        const auto expected=motiongun::Transform(motiongun::Multiply(optic,inv),{63.45f,39.8f,80.5f});
        const auto error=motiongun::Sub(attached.origin,expected);
        Check(motiongun::Dot(error,error)<.0001f && std::fabs(radius-10.8f)<.001f,
              "lens follows the same full bind and wrist transform as the HK aperture");
        const auto lensEye=motiongun::Sub(attached.origin,motiongun::Transform(attached.basis,{0,50.76f,0}));
        Check(hkscope::EyeBox(attached,lensEye,423),"eye aligned with actual mesh activates lens at user's world scale");
        const auto lensProjection=hkscope::Projection(1,32768,radius/423.f);
        Check(std::fabs(lensProjection.m[0]*radius/423.f/.12f-3.f)<.001f,
              "scope magnification stays 3x when measured aperture size changes");
        const auto view=hkscope::View(optic);
        for (float distance:{100.f,1000.f,20000.f}) {
            const auto point=motiongun::Add(optic.origin,motiongun::Transform(basis,{0,distance,0}));
            const float p[]={point.x,point.y,point.z};float v[3]{};
            for (int r=0;r<3;++r) {v[r]=view.r[r][3];for(int i=0;i<3;++i)v[r]+=view.r[r][i]*p[i];}
            Check(std::fabs(v[0])<0.01f && std::fabs(v[1])<0.01f && std::fabs(v[2]+distance)<0.01f,
                  "scope camera centers the muzzle ray across wrist rotations and distances");
        }
        auto eye=motiongun::Sub(optic.origin,motiongun::Transform(basis,{0,120,0}));
        Check(hkscope::EyeBox(optic,eye,1000),"scope accepts aligned eye behind lens");
        eye=motiongun::Add(eye,motiongun::Transform(basis,{30,0,0}));
        Check(!hkscope::EyeBox(optic,eye,1000),"scope rejects eye outside lateral eye box");
        Check(!hkscope::EyeBox(optic,motiongun::Add(optic.origin,motiongun::Transform(basis,{0,120,0})),1000),
              "scope rejects looking through objective from wrong side");
        Check(!hkscope::EyeBox(optic,motiongun::Sub(optic.origin,motiongun::Transform(basis,{0,400,0})),1000),
              "scope rejects rifle held beyond eye relief");
    }
    // Off-axis close-eye placement must match the physical aperture exactly.
    for (float scale:{423.f,1000.f}) {
        const motiongun::Basis identity{{{1,0,0},{0,1,0},{0,0,1}}};
        const motiongun::Frame lens{motiongun::GunBasis(identity),{.005f*scale,-.003f*scale,.02f*scale}};
        const float rot[3][3]={{1,0,0},{0,1,0},{0,0,1}},pos[3]={};
        const auto view=hkscope::WorldView(rot,pos);
        const auto model=hkscope::LensModel(lens,scale,.013f);
        mat4 projection{};BuildEyeProjection(projection,-.9f,1.1f,-1.1f,.9f,16,32768,true);
        const auto physical=Mul4(projection,Mul4(AffineToMat4(view),model));
        mat4 display{};
        Check(hkscope::LensProjection(projection,view,model,scale,16,display),"off-axis close-eye projection succeeds");
        for(int col=0;col<4;++col) for(int row:{0,1,3})
            Check(display.m[col*4+row]==physical.m[col*4+row],
                  "close-eye lens preserves physical mesh screen position, size and perspective");
    }
    // Close-eye comfort must remain local to the lens at both real-world scales.
    for (float scale:{423.f,1000.f}) for (float relief:{.12f,.039f,.02f,.0011f}) {
        const motiongun::Basis identity{{{1,0,0},{0,1,0},{0,0,1}}};
        const motiongun::Frame lens{motiongun::GunBasis(identity),{0,0,relief*scale}};
        const float rot[3][3]={{1,0,0},{0,1,0},{0,0,1}},pos[3]={};
        const auto view=hkscope::WorldView(rot,pos);
        const auto model=hkscope::LensModel(lens,scale,.013f);
        mat4 projection{};BuildEyeProjection(projection,-1,1,-1,1,16,32768,true);
        const auto savedProjection=projection,savedModel=model;
        mat4 result{};
        Check(hkscope::EyeBox(lens,{0,0,0},scale),"close eye remains aligned while in front of lens");
        Check(hkscope::LensProjection(projection,view,model,scale,16,result),"close lens projection is valid");
        Check(!std::memcmp(&projection,&savedProjection,sizeof(projection)) &&
              !std::memcmp(&model,&savedModel,sizeof(model)),"comfort does not mutate world projection or physical lens");
        for (float value:result.m) Check(std::isfinite(value),"close-eye lens matrix remains finite");
        for (float x:{-1.f,1.f}) for (float y:{-1.f,1.f}) {
            const float z=result.m[2]*x+result.m[6]*y+result.m[14];
            const float w=result.m[3]*x+result.m[7]*y+result.m[15];
            Check(w>0 && z>=-w && z<=w,"lens corners survive near clipping at close positive relief");
        }
        if (relief==.12f) {
            const auto native=Mul4(projection,Mul4(AffineToMat4(view),model));
            Check(!std::memcmp(&native,&result,sizeof(result)),"normal-distance lens transform and depth are unchanged");
        } else {
            auto reversed=view;for(int col=0;col<3;++col) reversed.r[2][col]*=-1;
            Check(!hkscope::LensProjection(projection,reversed,model,scale,16,result),
                  "close-eye comfort does not display lens when looking away");
        }
    }
    config.enabled = config.firstPerson = true;
    // Original behavior remains explicitly covered with the opt-out, then
    // new stabilized fixtures below exercise the enabled default separately.
    Check(config.firstPersonMovementStabilization,"movement stabilization defaults on for existing INIs");
    config.firstPersonMovementStabilization=false;
    config.positionalTracking = true;
    VR().m_system = reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));
    VR().m_poseValid = true;
    Head(0);
    g_boundBase = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    auto rva = [](const void* p) {
        const uint64_t d = reinterpret_cast<uint64_t>(p) - g_boundBase;
        Check(d <= UINT32_MAX, "test addresses fit a module RVA");
        return static_cast<uint32_t>(d);
    };
    dll.lara = rva(laraMemory);
    dll.laraItem = rva(&laraItem);
    dll.camera = rva(cameraMemory);
    dll.frameFrac = rva(&fraction);
    dll.analogInput = rva(analogMemory);
    dll.input = rva(&actionInput);
    dll.getJointAbsPositionLerp = rva(reinterpret_cast<void*>(&FakeJoint));
    *reinterpret_cast<int16_t*>(laraMemory + off::lara_vehicle) = -1;
    g_boundDll = &dll;
    auto& pos = *reinterpret_cast<PHD_3DPOS*>(itemMemory + off::item_pos);
    auto& state = *reinterpret_cast<int16_t*>(itemMemory + off::item_anim_state);
    *reinterpret_cast<int16_t*>(itemMemory + off::item_hit_points) = 1000;
    state = 2;

    {
        ActionIconDll icons{};
        icons.points=rva(iconPoints); icons.count=rva(&iconCount); icons.matrix=rva(iconMatrix);
        icons.perspective=rva(&iconPerspective); icons.centerX=rva(&iconCenterX);
        icons.centerY=rva(&iconCenterY); icons.nearZ=rva(&iconNear); icons.farZ=rva(&iconFar);
        g_actionIconDll=&icons;
        g_hDrawActionIndicators.m_trampoline=reinterpret_cast<void*>(&FakeActionIcons);
        for (int which:{0,1}) for (int degrees=0;degrees<360;degrees+=15) {
            dll.game=which;
            const float yaw=degrees*kPi/180;
            const float m[]={std::cos(yaw),0,-std::sin(yaw),12000,
                             0,1,0,-4000,std::sin(yaw),0,std::cos(yaw),28000};
            std::memcpy(iconMatrix,m,sizeof(m));
            auto world=[&](float x,float y,float z) {
                return PHD_VECTOR{int32_t(std::lround(m[3]+m[0]*x+m[8]*z)),
                                  int32_t(std::lround(m[7]+y)),
                                  int32_t(std::lround(m[11]+m[2]*x+m[10]*z))};
            };
            for (auto local:{PHD_VECTOR{0,750,80},PHD_VECTOR{500,100,100},PHD_VECTOR{0,0,0},
                             PHD_VECTOR{0,700,-144},PHD_VECTOR{0,-700,140}}) {
                iconPoints[0]=world(float(local.x),float(local.y),float(local.z));
                const auto saved=iconPoints[0]; iconCount=1;
                for (int guard=0;guard<3;++guard) {
                    g_active=guard!=1; VR().m_poseValid=guard!=2;
                    Detour_DrawActionIndicators();
                    Check(!std::memcmp(iconPoints,&saved,sizeof(saved)),"Action icon world data restored after draw");
                    if (guard) Check(!std::memcmp(seenIconPoints,&saved,sizeof(saved)),
                        "third person and invalid tracking preserve native Action prompt");
                    else {
                        const auto& p=seenIconPoints[0];
                        const float dx=p.x-m[3],dy=p.y-m[7],dz=p.z-m[11];
                        const float vx=m[0]*dx+m[2]*dz,vz=m[8]*dx+m[10]*dz;
                        Check(vz>iconNear && vz<iconFar,"nearby FP Action prompt clears native depth clipping");
                        Check(std::fabs(vx/vz*iconPerspective)<=iconCenterX*.705f &&
                              std::fabs(dy/vz*iconPerspective)<=iconCenterY*.705f,
                              "floor/switch prompt stays inside HUD safe rectangle at every heading");
                    }
                }
            }
            for (auto local:{PHD_VECTOR{0,0,600},PHD_VECTOR{0,0,-500},PHD_VECTOR{0,2000,100}}) {
                auto p=world(float(local.x),float(local.y),float(local.z)); const auto saved=p;
                Check(!PlaceActionIcon(p,m,1000,960,540,16,32768) && !std::memcmp(&p,&saved,sizeof(p)),
                      "visible, distant and behind-player prompts are not moved");
            }
        }
        std::fill(std::begin(iconMatrix),std::end(iconMatrix),0.0f);
        iconMatrix[0]=iconMatrix[5]=iconMatrix[10]=1;
        for (int n:{0,1,20,21,-1}) {
            iconCount=n; g_active=true; VR().m_poseValid=true;
            for (auto& p:iconPoints) p={0,700,0};
            const int before=iconDraws; Detour_DrawActionIndicators();
            Check(iconDraws==before+1,"native Action icon drawer invoked exactly once");
            Check(iconPoints[0].y==700 && iconPoints[19].z==0,"all prompt data restored including full-capacity list");
            Check((seenIconPoints[0].z>0)==(n>0 && n<=20),"empty/corrupt prompt counts never generate icons");
        }
        PHD_VECTOR p{0,700,0};
        Check(!PlaceActionIcon(p,iconMatrix,0,960,540,16,32768),"uninitialized prompt projection is not divided by zero");
        Check(!PlaceActionIcon(p,iconMatrix,1000,960,540,400,420),"invalid prompt clipping interval preserves native path");
        iconMatrix[0]=2;
        Check(!PlaceActionIcon(p,iconMatrix,1000,960,540,16,32768),"non-rigid native camera effects preserve original prompt");
        iconMatrix[0]=NAN;
        Check(!PlaceActionIcon(p,iconMatrix,1000,960,540,16,32768),"nonfinite prompt matrix cannot corrupt native data");
        g_actionIconDll=nullptr; g_active=false; dll.game=0;
    }

    {
        MotionDll motion{};
        motion.findTargetPoint=rva(reinterpret_cast<void*>(&FakeShotTarget));
        motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeShotSight));
        g_motionDll=&motion; g_headingItem=itemMemory;
        dll.getFloor=rva(reinterpret_cast<void*>(&FakeShotFloor));
        *reinterpret_cast<int16_t*>(itemMemory+off::item_room)=7;
        GunPose gun{}; gun.muzzle={100,200,300}; gun.direction={0,0,1};
        motiongun::Vec direction=gun.direction;
        Check(!AssistGunShot(gun,nullptr,direction) && shotTargetCalls==0,"no native target means no assist");
        Check(AssistGunShot(gun,itemMemory,direction),"actual shot assist accepts visible selected target");
        Check(direction.y<0 && direction.z>0 && Near(direction.x,0),"actual assist uses unclipped target direction");
        shotVisible=false; direction=gun.direction;
        Check(!AssistGunShot(gun,itemMemory,direction) && direction.z==1 && direction.y==0,
              "occluded target never redirects shot");
        shotTarget.x=1000; shotVisible=true;
        const int calls=shotSightCalls;
        Check(!AssistGunShot(gun,itemMemory,direction) && shotSightCalls==calls,
              "outside-cone target does not invoke native visibility");
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=0;
        const int targetCalls=shotTargetCalls;
        Check(!AssistGunShot(gun,itemMemory,direction) && shotTargetCalls==targetCalls,
              "dead target never invokes native aim helpers");
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_room)=0;
        g_motionDll=nullptr; g_headingItem=nullptr; dll.getFloor=0;
    }

    {
        MotionDll motion{};
        motion.getSpheres=rva(reinterpret_cast<void*>(&FakeTargetSpheres));
        motion.findTargetPoint=rva(reinterpret_cast<void*>(&FakeCandidatePoint));
        motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeCandidateLOS));
        motion.items=rva(&targetItemsPtr); motion.nextItemActive=rva(&activeTarget);
        g_motionDll=&motion;
        const auto savedObjects=dll.objects,savedFloor=dll.getFloor;
        dll.objects=rva(targetObjects); dll.getFloor=rva(reinterpret_cast<void*>(&FakeCandidateFloor));
        *reinterpret_cast<int16_t*>(targetObjects)=1;
        for (int i=0;i<3;++i) {
            *reinterpret_cast<int16_t*>(targetItems[i]+off::item_next_active)=i==2 ? -1 : int16_t(i+1);
            *reinterpret_cast<int16_t*>(targetItems[i]+off::item_hit_points)=100;
            *reinterpret_cast<PHD_3DPOS*>(targetItems[i]+off::item_pos)={i==1 ? 1000 : 0,0,i==2 ? 2000 : 1000};
        }
        GunPose gun{}; gun.direction={0,0,1}; motiongun::Vec ray=gun.direction;
        Check(SelectGunTarget(gun,ray)==targetItems[0],"barrel picks nearest direct living target without native lock");
        gun.muzzle.x=1000; ray=gun.direction;
        Check(SelectGunTarget(gun,ray)==targetItems[1],"other controller independently picks another enemy");
        gun.muzzle.x=0; *reinterpret_cast<int16_t*>(targetItems[0]+off::item_hit_points)=0;
        Check(SelectGunTarget(gun,ray)==targetItems[2],"dead near enemy cannot steal direct target");
        auto& farTarget=*reinterpret_cast<PHD_3DPOS*>(targetItems[2]+off::item_pos);
        farTarget.x_pos=200; ray=gun.direction;
        Check(SelectGunTarget(gun,ray)==targetItems[2] && ray.x>0,"off-axis living sphere receives bounded assist");
        farTarget={0,-999,1000}; ray={0,-.70675f,.70746f}; gun.direction=ray;
        Check(!SelectGunTarget(gun,ray),"occluded sphere is not acquired");
        activeTarget=-1; gun.direction={0,0,1}; ray=gun.direction;
        Check(!SelectGunTarget(gun,ray) && ray.z==1,"empty active list leaves unassisted shot intact");
        activeTarget=0; dll.objects=savedObjects; dll.getFloor=savedFloor; g_motionDll=nullptr;
    }

    // Menu state cannot change the meaning of either chord. Repeated polls
    // must not turn a held view chord into repeated toggles or native START.
    for (int which : {-1, 0, 1}) for (int menu : {0, 1, 2}) {
        game = which;
        *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InTitle) = menu == 1;
        *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InventoryActive) = menu == 2;
        g_runtimeEnabled = g_viewToggleHeld = false;
        for (int poll = 0; poll < 5; ++poll) {
            XGamepad p{}; p.wButtons = XB_Y; p.bLeftTrigger = 255;
            ApplyViewChords(p);
            Check(g_runtimeEnabled && !p.wButtons && !p.bLeftTrigger, "Y+LT owns menus and holds");
        }
        XGamepad p{}; ApplyViewChords(p);
        p.wButtons = XB_Y; p.bRightTrigger = 255; ApplyViewChords(p);
        Check(p.wButtons == XB_START && !p.bRightTrigger && g_runtimeEnabled, "Y+RT is graphics only");
        p = {}; p.wButtons = XB_Y; p.bLeftTrigger = p.bRightTrigger = 255;
        ApplyViewChords(p);
        Check(!g_runtimeEnabled && !p.wButtons && !p.bLeftTrigger && !p.bRightTrigger, "view chord wins both triggers");
    }
    game = 2;
    XGamepad p{}; p.wButtons = XB_Y; p.bLeftTrigger = 255; ApplyViewChords(p);
    Check(p.wButtons == XB_Y && p.bLeftTrigger == 255, "TR6 retains native controls");
    std::memset(appMemory, 0, sizeof(appMemory));
    for (int which : {0, 1, 2}) {
        game = which;
        Check(GameplayInputActive(), "TR4/5/6 gameplay accepts dual-grip Action");
        VRSystem::HandState hands[2]{};
        hands[0].grip = hands[1].grip = 1.0f;
        XState touch{}; bool shifted = false, gripAction = false;
        BuildStateFromHands(hands, touch, shifted, true, gripAction);
        Check(gripAction && !(touch.Gamepad.wButtons &
              (XB_LEFT_SHOULDER | XB_RIGHT_SHOULDER | XB_X)),
              "both Touch grips suppress Duck, Sneak and Walk");
        ApplyMergedChords(touch.Gamepad, true, gripAction, shifted);
        Check(touch.Gamepad.wButtons == XB_Y,
              "both Touch grips hold native Action in TR4/5/6");
        hands[0].btnUpper = true;
        BuildStateFromHands(hands, touch, shifted, true, gripAction);
        ApplyMergedChords(touch.Gamepad, true, gripAction, shifted);
        Check(touch.Gamepad.wButtons == XB_Y,
              "both grips take priority over right-grip plus Y Sneak");
        hands[0].grip = 0;
        BuildStateFromHands(hands, touch, shifted, true, gripAction);
        Check(!gripAction && touch.Gamepad.wButtons == XB_RIGHT_SHOULDER,
              "right-grip plus Y still emits Sneak alone");

        XGamepad physical{}; physical.wButtons =
            XB_LEFT_SHOULDER | XB_RIGHT_SHOULDER | XB_X;
        ApplyMergedChords(physical, true, false, shifted);
        Check(physical.wButtons == XB_Y,
              "merged physical LB+RB also holds Action without Duck/Walk");
        for (int trigger : {0, 1}) {
            XGamepad withTrigger{};
            if (trigger) withTrigger.bRightTrigger = 255;
            else withTrigger.bLeftTrigger = 255;
            ApplyMergedChords(withTrigger, true, true, shifted);
            Check(withTrigger.wButtons == XB_Y &&
                  (trigger ? withTrigger.bRightTrigger : withTrigger.bLeftTrigger) == 255,
                  "grip Action with a trigger never invokes a view/graphics chord");
        }

        hands[0] = {}; hands[1] = {};
        hands[0].stickClick = hands[1].stickClick = true;
        hands[0].stickX = 0.8f; hands[1].stickY = 0.7f;
        BuildStateFromHands(hands, touch, shifted, true, gripAction);
        ApplyMergedChords(touch.Gamepad, true, gripAction, shifted);
        Check(!shifted && touch.Gamepad.wButtons ==
              (XB_LEFT_THUMB | XB_RIGHT_THUMB) &&
              !touch.Gamepad.sThumbLX && !touch.Gamepad.sThumbRY,
              "native L3+R3 Photo Mode survives D-pad shift and stick drift");
        for (uint32_t ui : {drva::app_off::InventoryActive,
                            drva::app_off::InTitle, drva::app_off::InFMV}) {
            *reinterpret_cast<int32_t*>(appMemory + ui) = 1;
            Check(!GameplayInputActive(), "UI scene disables dual-grip Action");
            hands[0] = {}; hands[1] = {};
            hands[0].grip = hands[1].grip = 1.0f;
            BuildStateFromHands(hands, touch, shifted, false, gripAction);
            ApplyMergedChords(touch.Gamepad, false, gripAction, shifted);
            Check(!gripAction && !(touch.Gamepad.wButtons & XB_Y),
                  "UI scene retains native grip controls without synthetic Action");
            *reinterpret_cast<int32_t*>(appMemory + ui) = 0;
        }
    }
    game = 0; g_viewToggleHeld = true;
    XGamepad viewWithGrips{}; viewWithGrips.wButtons = XB_Y;
    viewWithGrips.bLeftTrigger = 255; bool shifted = false;
    ApplyMergedChords(viewWithGrips, true, true, shifted);
    Check(!viewWithGrips.wButtons && !viewWithGrips.bLeftTrigger,
          "real Y+LT view toggle cannot leak grip Action");
    XGamepad graphicsWithGrips{}; graphicsWithGrips.wButtons = XB_Y;
    graphicsWithGrips.bRightTrigger = 255;
    ApplyMergedChords(graphicsWithGrips, true, true, shifted);
    Check(graphicsWithGrips.wButtons == XB_START && !graphicsWithGrips.bRightTrigger,
          "real Y+RT graphics toggle cannot leak grip Action");
    game = 0; std::memset(appMemory, 0, sizeof(appMemory));
    g_runtimeEnabled = true; g_haveHeading = false;
    PHD_3DPOS camera{}; camera.y_rot = Angle(1.0f);
    Check(Anchor(camera) && jointCalled, "camera invokes native joint query");
    Check(camera.y_pos == -700 && camera.z_pos == 144 && camera.x_rot == 0,
          "native head location becomes camera location");
    Check(Near(g_headingBase, 0), "first-person starts facing Lara, not chase-camera orbit");
    g_active = true;

    // A headset standing 1.7m above its origin contributes zero at entry.
    // Both eye transforms and the culling head transform share that neutral.
    auto head = VR().HeadView();
    Check(Near(head.r[0][3], 0) && Near(head.r[1][3], 0) && Near(head.r[2][3], 0), "standing height neutralized");
    VR().m_eyeFromHead[0].r[0][3] = 0.032f;
    VR().m_eyeFromHead[1].r[0][3] = -0.032f;
    Check(Near(VR().EyeView(Eye::Left).r[0][3], 32) &&
          Near(VR().EyeView(Eye::Right).r[0][3], -32), "neutral preserves stereo IPD");
    VR().m_rawHeadPose.m[1][3] += 0.1f;
    Check(Near(InvertRigid(VR().HeadView()).r[1][3], -100), "physical height delta stays tracked");
    VR().RecenterFirstPersonHead();
    Check(Near(VR().HeadView().r[1][3], 0), "recenter takes effect immediately");
    g_active = false;
    Check(Near(VR().TrackedHeadView().r[1][3], VR().m_headFromTracking.r[1][3]), "third person retains original tracking");
    g_active = true;

    // Physical turns, wraparound, and stick turns must all reach body yaw.
    Head(kPi / 4); pos.y_rot = 0; g_headingBase = 0;
    TurnBodyToHead(itemMemory, 1.0f / 60);
    Check(Near(Radians(pos.y_rot), 4 * kPi / 180), "body follows physical yaw at configured speed");
    for (int i = 0; i < 30; ++i) TurnBodyToHead(itemMemory, 1.0f / 60);
    Check(Near(Radians(pos.y_rot), kPi / 4), "body reaches physical heading");
    state = 23; const auto locked = pos.y_rot; Head(-1);
    TurnBodyToHead(itemMemory, 1);
    Check(pos.y_rot == locked, "scripted/roll state retains native facing");
    state = 2; water = 1; TurnBodyToHead(itemMemory, 1);
    Check(pos.y_rot == locked, "swimming retains native facing"); water = 0;
    Head(-179 * kPi / 180); pos.y_rot = Angle(179 * kPi / 180);
    TurnBodyToHead(itemMemory, 1.0f / 60);
    Check(Near(Wrap(Radians(pos.y_rot) + 179 * kPi / 180), 0), "body turns short way across yaw wrap");
    Head(0); pos.y_rot = 0; g_headingBase = 0;
    const auto renderStickFrame=[]() {
        g_renderTurn.frameTime=TurnTime()-1.0/60;
        AdvanceStickTurn(TurnTime());
    };
    float lx = 0, ly = 0, rx = 1;
    FirstPersonInput(lx, ly, rx, false);
    Check(rx==0 && g_headingBase==0,"input latches turn intent without stepping the camera at poll rate");
    renderStickFrame();
    Check(g_headingBase>0,"render update advances shared first-person heading");
    TurnBodyToHead(itemMemory, 1.0f / 60);
    Check(pos.y_rot > 0, "body follows stick heading");
    g_renderTurn.Reset();

    // Test the real AimWeapon detour with a native stub that first writes
    // different values. Both arm paths must receive relative HMD yaw/pitch.
    g_hAimWeapon.m_trampoline = reinterpret_cast<void*>(&FakeAim);
    g_headingBase = 0.25f; pos.y_rot = Angle(0.5f); Head(0.75f, 0.3f);
    Check(Near(VR().HeadYawRadians(), 0.75f) && Near(VR().HeadPitchRadians(), 0.3f), "yaw/pitch convention");
    Detour_AimWeapon(nullptr, laraMemory + off::lara_left_arm);
    for (uint32_t offset : {off::lara_left_arm, off::lara_right_arm}) {
        Check(Near(Radians(*reinterpret_cast<int16_t*>(laraMemory + offset + off::arm_y_rot)), 0.5f), "arm yaw is head minus body");
        Check(Near(Radians(*reinterpret_cast<int16_t*>(laraMemory + offset + off::arm_x_rot)), 0.3f), "arm pitch follows look-up");
        Check(*reinterpret_cast<int16_t*>(laraMemory + offset + off::arm_lock) == 1, "gun avoids torso double rotation");
    }
    Head(0.75f, -0.3f); Detour_AimWeapon(nullptr, laraMemory + off::lara_right_arm);
    Check(Near(Radians(*reinterpret_cast<int16_t*>(laraMemory + off::lara_right_arm + off::arm_x_rot)), -0.3f), "gun follows look-down");
    {
        const auto savedConfig=config;
        const bool savedScene=g_scenePoseValid;
        MotionDll motion{}; motion.app=rva(&motionApp);
        g_motionDll=&motion; g_motionHooksReady=true; g_scenePoseValid=true;
        config.firstPersonMotionGuns=true; config.firstPersonHeadTranslation=true;
        appMemory[0x9e4]=1;
        auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
        auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
        const auto oldStatus=status,oldGun=gun;
        status=4; gun=1;
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        Check(MotionReady(),"motion readiness accepts HD pistols and both controller poses");
        g_hAnimatePistols.m_trampoline=reinterpret_cast<void*>(&FakePistolAnimation);
        auto& nativeTarget=*reinterpret_cast<void**>(laraMemory+off::lara_target);
        nativeTarget=targetItems[0];
        g_gunTriggers.active=true; g_gunTriggers.pending[1]=true;
        Detour_AnimatePistols(1);
        Check(!seenAnimationTarget && nativeTarget==targetItems[0],"controller animation bypasses obsolete arm lock and restores target");
        g_gunTriggers.Reset(); Detour_AnimatePistols(1);
        Check(seenAnimationTarget==targetItems[0],"non-firing animation preserves native target");
        nativeTarget=nullptr;

        auto blocked=[](const char* expected) {
            const char* reason=MotionBlockedReason();
            Check(reason && !std::strcmp(reason,expected),"motion fallback diagnostic identifies failing gate");
        };
        VR().m_controllerPoseValid[0]=false; blocked("left-controller-pose-missing");
        VR().m_controllerPoseValid[0]=true;
        VR().m_controllerPoseValid[1]=false; blocked("right-controller-pose-missing");
        VR().m_controllerPoseValid[1]=true;
        appMemory[0x9e4]=0; blocked("classic-graphics-or-no-app"); appMemory[0x9e4]=1;
        gun=2; blocked("weapon-not-supported"); gun=3;
        Check(MotionReady(),"motion readiness accepts Uzis");
        status=0; blocked("guns-not-ready"); status=4;
        g_scenePoseValid=false; blocked("no-scene-camera"); g_scenePoseValid=true;
        config.positionalTracking=false; blocked("positional-tracking-disabled"); config.positionalTracking=true;
        config.firstPersonHeadTranslation=false; blocked("head-translation-disabled"); config.firstPersonHeadTranslation=true;
        g_motionHooksReady=false; blocked("motion-hooks-unavailable");
        g_motionHooksReady=true;
        status=0;
        Check(MotionTriggerMode(),"holstered pistols/Uzis retain long-hold equip controls");
        status=4;
        gun=2; Check(!MotionTriggerMode(),"single weapons need the additional hooks"); gun=1;
        g_hFireWeapon.m_trampoline=reinterpret_cast<void*>(&FakeFire);
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeHandJoint));
        const bool neutral=VR().m_firstPersonNeutralValid;
        VR().m_firstPersonNeutralValid=true;
        for (auto& controller:VR().m_rawControllerPose) {
            controller=VR().m_rawHeadPose;
        }
        const int16_t aim[2]={};
        g_gunTriggers.Reset(); g_gunTriggers.Update(true,true,false,false,0);
        g_gunTriggers.Update(true,true,true,false,1);
        g_gunTriggers.Update(true,true,false,false,2);
        Check(FireWeaponForHand(1,1,nullptr,nullptr,aim)==0 && nativeShotCalls==0,
              "right native firing skipped for left-only tap");
        Check(FireWeaponForHand(0,1,nullptr,nullptr,aim)==-1 && nativeShotCalls==1 &&
              nativeShotHand==0,"left tap fires exactly the left native shot");
        Check(FireWeaponForHand(0,1,nullptr,nullptr,aim)==0 && nativeShotCalls==1,
              "repeated simulation calls cannot duplicate a consumed tap");
        g_gunTriggers.Update(true,true,false,true,3);
        Check(FireWeaponForHand(0,1,nullptr,nullptr,aim)==0 &&
              FireWeaponForHand(1,1,nullptr,nullptr,aim)==-1 &&
              nativeShotCalls==2 && nativeShotHand==1,"right tap fires only right native shot");
        g_gunTriggers.Update(true,true,true,false,4);
        g_gunTriggers.Update(true,true,false,false,5);
        VR().m_controllerPoseValid[0]=false;
        Check(FireWeaponForHand(0,1,nullptr,nullptr,aim)==0 && nativeShotCalls==2,
              "lost tracking cannot convert queued shot to head aiming");
        VR().m_controllerPoseValid[0]=true;
        Check(FireWeaponForHand(-1,1,nullptr,nullptr,aim)==-1 && nativeShotCalls==3,
              "unrecognized native firing caller is not suppressed");
        g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&FakeHandDraw);
        auto& meshBits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
        const uint32_t savedBits=meshBits;
        meshBits=0x7fff; Detour_DrawCreatureHD(itemMemory,0,0);
        Check(handDrawCalls==0 && meshBits==0x7fff,"body/upper-arm pass completely skipped");
        for (uint32_t mask:{0x600u,0x3000u}) {
            meshBits=mask; Detour_DrawCreatureHD(itemMemory,1,0);
            Check(meshBits==mask && g_renderArm==-1,"native mesh mask and arm scope restored after draw");
        }
        Check(handDrawCalls==2,"exactly two hand-only draws");
        g_crouchHidden=true;
        const int beforeCrouchedHands=handDrawCalls;
        meshBits=0; Detour_DrawCreatureHD(itemMemory,0,0);
        Check(handDrawCalls==beforeCrouchedHands,"crouched body stays hidden with motion guns");
        for (uint32_t mask:{0x600u,0x3000u}) {
            meshBits=mask; Detour_DrawCreatureHD(itemMemory,1,0);
            Check(meshBits==mask,"crouched hand draw restores native gun mask");
        }
        Check(handDrawCalls==beforeCrouchedHands+2,"crouched motion guns retain both hands");
        g_rollHidden=true; Detour_DrawCreatureHD(itemMemory,1,0);
        Check(handDrawCalls==beforeCrouchedHands+2,"roll still hides hands as well as body");
        g_rollHidden=g_crouchHidden=false;
        g_underwaterHidden=true;
        const int beforeSubmergedHands=handDrawCalls;
        meshBits=0; Detour_DrawCreatureHD(itemMemory,0,0);
        Check(handDrawCalls==beforeSubmergedHands,"submerged body pass stays hidden with motion guns ready");
        for (uint32_t mask:{0x600u,0x3000u}) {
            meshBits=mask; Detour_DrawCreatureHD(itemMemory,1,0);
            Check(meshBits==mask,"underwater hiding preserves hand-pass native mask");
        }
        Check(handDrawCalls==beforeSubmergedHands+2,"underwater body hiding does not disable tracked-hand passes");
        g_underwaterHidden=false;
        meshBits=savedBits;
        auto& leftFlash=*reinterpret_cast<int16_t*>(laraMemory+off::lara_left_arm+20);
        auto& rightFlash=*reinterpret_cast<int16_t*>(laraMemory+off::lara_right_arm+20);
        leftFlash=rightFlash=0; gun=3;
        motion.rightFireReturn=0x111; motion.leftFireReturn=0x222;
        g_gunTriggers.pending[0]=true; g_gunTriggers.pending[1]=false;
        g_hPistolHandler.m_trampoline=reinterpret_cast<void*>(&FakeUziHandler);
        Detour_PistolHandler(3);
        Check(leftFlash==3 && rightFlash==0,"left-only Uzi shot and flash stay on the left");
        leftFlash=rightFlash=0;
        g_gunTriggers.pending[1]=true; Detour_PistolHandler(3);
        Check(leftFlash==0 && rightFlash==3,"right-only Uzi shot and flash stay on the right");
        leftFlash=rightFlash=0;
        // Additional weapon hooks are independently gated: failures must
        // leave the previously working pistols/Uzis usable.
        LongGunDll longDll{}; longDll.items=rva(&projectileItems);
        longDll.opticReturns[0]=0x12345;
        g_longGunDll=&longDll; g_longGunHooksReady=true;
        g_hFireShotgun.m_trampoline=reinterpret_cast<void*>(&FakeShotgun);
        g_hFireSpecial.m_trampoline=reinterpret_cast<void*>(&FakeGrenade);
        g_hFireCrossbow.m_trampoline=reinterpret_cast<void*>(&FakeCrossbow);
        g_hInitialiseProjectile.m_trampoline=reinterpret_cast<void*>(&FakeProjectileInit);
        g_hRifleHandler.m_trampoline=reinterpret_cast<void*>(&FakeRifle);
        g_hGetTargetOnLOS.m_trampoline=reinterpret_cast<void*>(&FakeOptic);
        const auto savedFloor=dll.getFloor;
        dll.getFloor=rva(reinterpret_cast<void*>(&FakeProjectileFloor));
        const int savedGame=dll.game;
        {
            const auto settings=config;
            config.firstPersonHKScope=true;config.eyeOffsetMode=3;
            alignas(16) static uint8_t scopeObject[off::object_stride]{};
            const auto oldObjects=dll.objects;
            const auto oldObject=*reinterpret_cast<int16_t*>(itemMemory+off::item_object);
            dll.objects=rva(scopeObject);*reinterpret_cast<int16_t*>(itemMemory+off::item_object)=0;
            auto* scopeBind=reinterpret_cast<float*>(scopeObject+1676)+10*16;
            scopeBind[0]=scopeBind[5]=scopeBind[10]=scopeBind[15]=1.f;
            config.perEyeView=true;config.perEyeProjection=1;config.duplicateDraws=true;
            motiongun::Frame lens{},optic{};
            const auto savedLara=std::string(reinterpret_cast<const char*>(laraMemory),sizeof(laraMemory));
            dll.game=1;gun=5;
            g_hkScopeBindItem=nullptr;
            Check(!FirstPersonHKScopePose(lens,optic),"scope waits for a real HK wrist binding instead of reading unrelated live geometry");
            const auto oldGetJoints=g_hGetJoints.m_trampoline;
            g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
            float hkPalette[15*12]{};
            g_renderArm=1;
            Check(Detour_GetJoints(itemMemory,hkPalette,0)==15,"scope sees the production right-hand gun draw");
            Check(FirstPersonTrackedHandJoint()==10,"corrected right wrist enables hand-only shader repair");
            g_renderArm=0;
            auto* leftBind=reinterpret_cast<float*>(scopeObject+1676)+13*16;
            leftBind[0]=leftBind[5]=leftBind[10]=leftBind[15]=1.f;
            Check(Detour_GetJoints(itemMemory,hkPalette,0)==15 && FirstPersonTrackedHandJoint()==13,
                  "left hand uses its own corrected wrist palette index");
            g_renderArm=-1;
            Check(FirstPersonTrackedHandJoint()==-1,"non-hand draws cannot inherit wrist shader repair");
            g_renderArm=1;
            Detour_GetJoints(laraMemory,hkPalette,0);
            Check(FirstPersonTrackedHandJoint()==-1,"rejected non-Lara palette clears stale wrist shader index");
            g_renderArm=-1;g_hGetJoints.m_trampoline=oldGetJoints;
            for (int which:{0,1,2}) for (int weapon=1;weapon<=6;++weapon) {
                dll.game=which;gun=int16_t(weapon);
                Check(FirstPersonHKScopePose(lens,optic)==(which==1 && weapon==5),
                      "physical scope available only for TR5 HK");
            }
            dll.game=1;gun=5;
            GunPose gunPose{};Check(BuildGunPose(1,gunPose),"scope fixture has controller barrel pose");
            Check(FirstPersonHKScopePose(lens,optic),"HK exposes scope pose without native zoom");
            const auto boundLens=lens;
            // DrawLaraHD copies each body/hand/face geometry into objects[item].
            // Outside that draw the final face descriptor has no joint 10.
            int32_t headOnlyMapping=14;float headOnlyPose[12]{};
            auto* liveGeom=scopeObject+off::object_geom;
            *reinterpret_cast<int32_t*>(liveGeom+28)=1;
            *reinterpret_cast<int32_t**>(liveGeom+48)=&headOnlyMapping;
            *reinterpret_cast<float**>(liveGeom+72)=headOnlyPose;
            Check(FirstPersonHKScopePose(lens,optic),"scope survives native replacement of gun descriptor with face-only geometry");
            Check(std::memcmp(&lens,&boundLens,sizeof(lens))==0,"face geometry cannot move or disable the HK lens");
            const auto savedController=VR().m_rawControllerPose[1];
            VR().m_rawControllerPose[1].m[0][3]+=.05f;
            Check(FirstPersonHKScopePose(lens,optic),"cached binding accepts a new controller pose");
            const auto trackedDelta=motiongun::Sub(lens.origin,boundLens.origin);
            Check(std::fabs(std::sqrt(motiongun::Dot(trackedDelta,trackedDelta))-50.f)<.02f,
                  "scope follows controller translation immediately, not the prior draw pose");
            VR().m_rawControllerPose[1]=savedController;
            g_hkScopeBindItem=laraMemory;
            Check(!FirstPersonHKScopePose(lens,optic),"binding from another Lara item cannot be reused");
            g_hkScopeBindItem=itemMemory;
            Check(FirstPersonHKScopePose(lens,optic),"matching Lara binding restores the lens");
            config.firstPersonHKScopeForwardMetres=.8f;config.firstPersonHKScopeUpMetres=-.5f;
            Check(FirstPersonHKScopePose(lens,optic) &&
                  motiongun::Dot(motiongun::Sub(lens.origin,boundLens.origin),motiongun::Sub(lens.origin,boundLens.origin))<.0001f,
                  "mesh attachment does not use obsolete guessed wrist offsets");
            config.firstPersonHKScopeForwardMetres=settings.firstPersonHKScopeForwardMetres;
            config.firstPersonHKScopeUpMetres=settings.firstPersonHKScopeUpMetres;
            const auto muzzleDelta=motiongun::Sub(optic.origin,gunPose.muzzle);
            Check(motiongun::Dot(muzzleDelta,muzzleDelta)<0.0001f,
                  "scope optical camera uses actual muzzle origin");
            const bool cameraWasValid=worldCameraValid;
            float savedCameraRot[3][3],savedCameraPos[3];
            std::memcpy(savedCameraRot,worldCameraRot,sizeof(savedCameraRot));
            std::memcpy(savedCameraPos,worldCameraPos,sizeof(savedCameraPos));
            std::memset(worldCameraRot,0,sizeof(worldCameraRot));
            worldCameraRot[0][0]=worldCameraRot[1][1]=worldCameraRot[2][2]=1;
            const auto desiredEye=motiongun::Sub(lens.origin,motiongun::Transform(lens.basis,{0,120,0}));
            const auto eyeOffset=InvertRigid(VR().EyeView(Eye::Left));
            worldCameraPos[0]=desiredEye.x-eyeOffset.r[0][3];
            worldCameraPos[1]=desiredEye.y-eyeOffset.r[1][3];
            worldCameraPos[2]=desiredEye.z+eyeOffset.r[2][3];worldCameraValid=true;
            Check(FirstPersonHKScopeAiming(),"aligned physical eye activates scoped precision aiming");
            auto shotDirection=gunPose.direction;
            Check(!AssistGunShot(gunPose,itemMemory,shotDirection) &&
                  shotDirection.x==gunPose.direction.x && shotDirection.y==gunPose.direction.y && shotDirection.z==gunPose.direction.z,
                  "scoped HK shot cannot be pulled away from reticle by aim assist");
            worldCameraPos[0]+=1000;
            Check(!FirstPersonHKScopeAiming(),"moving away from scope releases precision-aim gate");
            worldCameraValid=cameraWasValid;
            std::memcpy(worldCameraRot,savedCameraRot,sizeof(savedCameraRot));
            std::memcpy(worldCameraPos,savedCameraPos,sizeof(savedCameraPos));
            opticsZoomed=true;Check(!FirstPersonHKScopePose(lens,optic),"native full-screen zoom and physical lens are mutually exclusive");opticsZoomed=false;
            appMemory[0x9e4]=0;Check(!FirstPersonHKScopePose(lens,optic),"classic graphics retain original optics");appMemory[0x9e4]=1;
            const auto snapshot=std::string(reinterpret_cast<const char*>(laraMemory),sizeof(laraMemory));
            FirstPersonHKScopePose(lens,optic);
            Check(snapshot==std::string(reinterpret_cast<const char*>(laraMemory),sizeof(laraMemory)),
                  "scope pose query cannot change native weapon or animation state");
            status=3;Check(!FirstPersonHKScopePose(lens,optic),"holstering immediately removes scope");status=4;
            water=1;Check(!FirstPersonHKScopePose(lens,optic),"swimming cannot retain scope");water=0;
            config.firstPersonHKScope=false;Check(!FirstPersonHKScopePose(lens,optic),"scope INI opt-out works");config.firstPersonHKScope=true;
            config.firstPersonMotionGuns=false;Check(!FirstPersonHKScopePose(lens,optic),"head-aim-only mode cannot enable physical scope");config.firstPersonMotionGuns=true;
            VR().m_controllerPoseValid[1]=false;Check(!FirstPersonHKScopePose(lens,optic),"tracking loss removes scope");VR().m_controllerPoseValid[1]=true;
            const bool wasActive=g_active;g_active=false;
            Check(!FirstPersonHKScopePose(lens,optic),"third person never enables scope");g_active=wasActive;
            dll.objects=oldObjects;*reinterpret_cast<int16_t*>(itemMemory+off::item_object)=oldObject;
            g_hkScopeBindItem=nullptr;
            config=settings;dll.game=savedGame;
            std::memcpy(laraMemory,savedLara.data(),sizeof(laraMemory));
        }
        // RICH3's grapple raised animation loops with no state-change exits.
        // Forcing arm.lock sends native AnimateShotgun there without firing.
        // Both tracked and head-aim paths must preserve its native lock.
        for (int which:{0,1}) for (int weapon=1;weapon<=6;++weapon)
        for (bool tracked:{false,true}) for (int16_t lock:{int16_t(0),int16_t(1)}) {
            dll.game=which; gun=int16_t(weapon); nativeAimLock=lock;
            config.firstPersonMotionGuns=tracked; config.firstPersonHeadAim=true;
            *reinterpret_cast<int16_t*>(laraMemory+off::lara_right_arm+off::arm_lock)=lock;
            GunPose controller{};
            if (tracked) Check(BuildGunPose(1,controller),"lock regression retains a valid tracked gun pose");
            Detour_AimWeapon(nullptr,laraMemory+off::lara_left_arm);
            Check(*reinterpret_cast<int16_t*>(laraMemory+off::lara_left_arm+off::arm_lock)==
                  (which==1 && weapon==6 ? lock : 1),
                  "TR5 grapple keeps native lock; all other guns retain forced FP aim lock");
            if (!tracked && which==1 && weapon==6)
                Check(*reinterpret_cast<int16_t*>(laraMemory+off::lara_right_arm+off::arm_lock)==lock,
                      "grapple head-aim fallback cannot force the other arm lock");
            if (tracked && weapon>=4)
                Check(*reinterpret_cast<int16_t*>(laraMemory+off::lara_left_arm+off::arm_x_rot)==controller.pitch,
                      "preserving native lock does not revert rifle/grapple controller pitch to head aim");
        }
        nativeAimLock=0; dll.game=savedGame; config.firstPersonMotionGuns=true;
        // Exercise the production caller dispatcher, not just FireWeaponForHand.
        // All four native binaries use ID 2 for revolver/Desert Eagle, 3 for Uzis.
        for (const auto& native:kMotionDlls) {
            motion.rightFireReturn=native.rightFireReturn;
            motion.leftFireReturn=native.leftFireReturn;
            for (int weapon:{1,3,2}) {
                gun=int16_t(weapon);
                const bool dual=weapon!=2;
                const auto leftCaller=motion.leftFireReturn;
                const auto rightCaller=dual ? motion.rightFireReturn : leftCaller;
                auto fire=[&](uint64_t caller) {
                    return FireWeaponForCaller(caller,weapon,nullptr,nullptr,aim);
                };
                g_gunTriggers.Reset(); g_gunTriggers.Update(true,true,false,false,0);
                g_gunTriggers.Update(true,true,true,false,1);
                const int before=nativeShotCalls;
                Check(fire(rightCaller)==0,"LT cannot fire the right pistol/Uzi or Desert Eagle");
                if (dual) {
                    Check(fire(leftCaller)==-1 && nativeShotHand==0 && nativeShotCalls==before+1,
                          "LT press fires native left pistol/Uzi before release");
                    Check(fire(leftCaller)==0,"native left call cannot repeat a consumed tap");
                    g_gunTriggers.Update(true,true,true,false,2);
                    g_gunTriggers.Update(true,true,false,false,3);
                    Check(fire(leftCaller)==0,"LT release discards repeat queued during native recoil");
                }
                g_gunTriggers.Clear(0);
                g_gunTriggers.Update(true,true,false,true,3);
                const int beforeRight=nativeShotCalls;
                if (dual) Check(fire(leftCaller)==0,"RT cannot fire left pistol/Uzi");
                Check(fire(rightCaller)==-1 && nativeShotHand==1 && nativeShotCalls==beforeRight+1,
                      "RT fires right pistol/Uzi or shared revolver branch from right controller");
                Check(!g_gunTriggers.WantsShot() && fire(rightCaller)==0,
                      "one RT request is consumed exactly once through native caller routing");
                g_gunTriggers.Update(true,true,false,true,4);
                g_gunTriggers.Update(true,true,false,false,5);
                Check(fire(rightCaller)==0 && nativeShotCalls==beforeRight+1,
                      "RT tap cannot fire a second native shot from a queued repeat after release");
                for (int repeat=0;repeat<8;++repeat) {
                    g_gunTriggers.Update(true,true,false,true,10+repeat);
                    Check(fire(rightCaller)==-1 && fire(rightCaller)==0,
                          "held RT continues native shots with one consume per request");
                }
                g_gunTriggers.Update(true,true,false,false,20);
                Check(fire(rightCaller)==0,"held RT stops on release through native caller routing");
                g_gunTriggers.pending[1]=true; VR().m_controllerPoseValid[1]=false;
                Check(fire(rightCaller)==0,"missing right pose cannot produce a head-aimed shot");
                VR().m_controllerPoseValid[1]=true;
                Check(fire(0xdeadbeef)==-1 && nativeShotHand==-1,
                      "unrecognized caller remains native for every handgun");
                const bool wasActive=g_active; g_active=false;
                Check(fire(rightCaller)==-1 && nativeShotHand==-1,
                      "third-person firing bypasses motion routing for every handgun");
                g_active=wasActive;
            }
        }
        g_gunTriggers.Reset(); g_gunTriggers.Update(true,true,false,false,0);
        VR().m_controllerPoseValid[0]=false;
        for (int weapon:{2,4,5,6}) {
            gun=int16_t(weapon);
            Check(MotionReady() && MotionTriggerMode(),"single weapons need only right-controller tracking");
            GunPose rightGun{};
            Check(BuildGunPose(1,rightGun),"all remaining weapons build calibrated wrist and muzzle");
            Detour_AimWeapon(nullptr,laraMemory+off::lara_left_arm);
            Check(*reinterpret_cast<int16_t*>(laraMemory+off::lara_left_arm+16)==rightGun.pitch,
                  "native shared rifle arm follows right-controller pitch");
            Detour_RifleHandler(gun);
            for (unsigned offset=off::lara_head_y_rot;offset<=off::lara_torso_z_rot;offset+=2)
                Check(*reinterpret_cast<int16_t*>(laraMemory+offset)==0,"rifle aiming cannot sway head/torso camera");
        }
        VR().m_controllerPoseValid[0]=true;
        for (gun=7;gun<=9;++gun) Check(!MotionTriggerMode(),"flares/torch/non-guns remain native");
        gun=4; g_gunTriggers.pending[0]=true; g_gunTriggers.pending[1]=false;
        Detour_FireShotgun(); Check(longCalls==0,"LT tap cannot discharge a single weapon");
        const auto savedBody=pos;
        int16_t savedArm[3]; std::memcpy(savedArm,laraMemory+off::lara_left_arm+12,sizeof(savedArm));
        const int beforeShots=nativeShotCalls;
        g_gunTriggers.pending[1]=true; Detour_FireShotgun();
        Check(pelletChecks==6 && nativeShotCalls==beforeShots+6 && !g_gunTriggers.pending[1],
              "one RT request produces exactly six pellets and consumes one request");
        Detour_FireShotgun(); Check(longCalls==1,"no repeated volley after request consumed");
        Check(!g_longShot && g_firingHand==-1 &&
              !std::memcmp(savedArm,laraMemory+off::lara_left_arm+12,sizeof(savedArm)) &&
              !std::memcmp(&savedBody,&pos,sizeof(pos)),"shot restores scoped arm and leaves body pose untouched");
        VR().m_controllerPoseValid[1]=false; g_gunTriggers.pending[1]=true;
        Detour_FireShotgun(); Check(longCalls==1,"lost right tracking blocks shotgun, not head fallback");
        VR().m_controllerPoseValid[1]=true;
        dll.game=0; gun=5; Detour_FireGrenade();
        Check(initCalls==1 && *reinterpret_cast<int16_t*>(projectileMemory+off::item_room)==9,
              "grenade initialization keeps native physics and resolves muzzle room");
        Check(!std::memcmp(projectileMemory+off::item_pos,projectileMemory+off::item_pos_prev,sizeof(PHD_3DPOS)),
              "projectile previous position starts at muzzle, not Lara's body");
        gun=6; g_gunTriggers.pending[1]=true; Detour_FireCrossbow(nullptr);
        Check(crossbowWasNull,"TR4 crossbow retains null-pose ammo-consuming branch");
        dll.game=1; gun=5; g_gunTriggers.pending[1]=true;
        g_hFireSpecial.m_trampoline=reinterpret_cast<void*>(&FakeHK);
        const int beforeHK=nativeShotCalls; Detour_FireHK(7);
        Check(nativeShotCalls==beforeHK+1 && nativeShotHand==1,"HK fires from right controller once");
        gun=6; g_gunTriggers.pending[1]=true;
        const int beforeGrapple=longCalls; Detour_FireCrossbow(nullptr);
        Check(longCalls==beforeGrapple && !g_gunTriggers.pending[1],
              "TR5 invalid non-optic grapple attempt cannot stick RT or fire on later mode entry");
        g_gunTriggers.pending[1]=true; // A new press in native laser-sight mode.
        PHD_VECTOR cameraStart{999,999,999},cameraEnd=cameraStart;
        Check(GetTargetOnLOSForCaller(0x12345,&cameraStart,&cameraEnd,1,1)==42 &&
              opticMode==1 && sawOpticScope && !g_opticShot && !g_gunTriggers.pending[1],
              "one queued grapple shot passes through native target validation exactly once");
        Check(crossbowArgument.x_rot==1234 && crossbowArgument.y_rot==2345,
              "native grapple attachment angles preserved after controller target selection");
        Check(cameraStart.x==999 && cameraEnd.x==999,"optic shot cannot overwrite camera vectors");
        GetTargetOnLOSForCaller(0x12345,&cameraStart,&cameraEnd,1,1);
        Check(opticMode==0 && longCalls==beforeGrapple+1,"repeated optic poll cannot duplicate shot");
        gun=2; g_gunTriggers.pending[1]=true;
        GetTargetOnLOSForCaller(0x12345,&cameraStart,&cameraEnd,1,1);
        Check(opticMode==1 && !g_gunTriggers.pending[1],
              "revolver/Desert Eagle optic shot uses right controller and consumes RT");
        GetTargetOnLOSForCaller(0x12345,&cameraStart,&cameraEnd,1,1);
        Check(opticMode==0 && cameraStart.x==999 && cameraEnd.x==999,
              "revolver optic polling cannot duplicate shots or overwrite the camera");
        g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&FakeHandDraw);
        const int beforeCombined=handDrawCalls;
        meshBits=0x3600; Detour_DrawCreatureHD(itemMemory,1,0);
        Check(handDrawCalls==beforeCombined+2 && meshBits==0x3600 && g_renderArm==-1,
              "combined long-gun pass splits into two hand-only tracked draws and restores state");
        meshBits=savedBits;
        const auto triggerState=state;
        for (int which:{0,1}) for (int weapon=1;weapon<=6;++weapon)
        for (int gunState:{0,1,2,3,5}) for (int actionState:{2,3,10,19,28,56,57}) {
            dll.game=which; gun=int16_t(weapon); status=int16_t(gunState); state=int16_t(actionState);
            g_gunTriggers.Reset(); g_gunEquip.Reset(); g_triggerWeapon=weapon;
            Check(MotionTriggerMode(),"FP equip handler remains available during native grab/action states");
            for (uint8_t raw:{uint8_t(255),uint8_t(255),uint8_t(128),uint8_t(0),uint8_t(31),uint8_t(0)}) {
                uint8_t l=0,r=raw;
                UpdateGunTriggers(l,r,MotionTriggerMode(),100);
                Check(r==raw && !g_gunTriggers.WantsShot(),
                      "non-ready RT preserves analog value, first press, hold and release for ledge grab");
            }
        }
        state=triggerState; status=4; gun=1;
        g_gunTriggers.Reset(); g_gunEquip.Reset(); g_triggerWeapon=1;
        uint8_t lt=0,rt=0; UpdateGunTriggers(lt,rt,true,0);
        lt=0; rt=255; UpdateGunTriggers(lt,rt,true,10);
        Check(rt==255 && g_gunTriggers.Consume(1),"ready RT still queues exactly one right-hand shot");
        lt=0; rt=255; UpdateGunTriggers(lt,rt,true,20);
        Check(rt==255 && g_gunTriggers.Consume(1),"holding ready RT sustains native fire");
        status=1; lt=0; rt=255; UpdateGunTriggers(lt,rt,true,30);
        Check(rt==255 && !g_gunTriggers.WantsShot(),"ready-to-hands-busy transition immediately restores held grab");
        status=4; lt=0; rt=255; UpdateGunTriggers(lt,rt,true,40);
        Check(rt==255 && g_gunTriggers.Consume(1),"held RT resumes when guns become ready");
        lt=0; rt=0; UpdateGunTriggers(lt,rt,true,50);
        lt=255; rt=0; UpdateGunTriggers(lt,rt,true,60);
        Check(rt==255 && g_gunTriggers.Consume(0) && !g_gunTriggers.Consume(1),
              "production LT press immediately fires only the left gun");
        lt=255; rt=0; UpdateGunTriggers(lt,rt,true,65);
        lt=0; rt=0; UpdateGunTriggers(lt,rt,true,70);
        Check(rt==0 && !g_gunTriggers.WantsShot(),
              "production LT release cancels the held repeat instead of firing");
        auto& lastGun=*reinterpret_cast<int16_t*>(laraMemory+8);
        const auto savedLastGun=lastGun;
        for (int which:{0,1}) for (int scheme:{0,1})
        for (uint8_t style:{uint8_t(0),uint8_t(1)}) for (int target=1;target<=6;++target) {
            dll.game=which; appMemory[0x9e4]=uint8_t(1 | (scheme<<1));
            appMemory[0x9ec+scheme]=style;
            gun=lastGun=6; status=0;
            g_gunTriggers.Reset(); g_gunEquip.Reset(); g_triggerWeapon=6;
            lt=255; rt=200; UpdateGunTriggers(lt,rt,true,0);
            Check(lt==255 && rt==200,"first LT poll draws immediately while preserving native grab RT");
            gun=lastGun=int16_t(target);
            lt=rt=0; UpdateGunTriggers(lt,rt,true,10);
            Check(lt==255 && !g_gunTriggers.WantsShot(),"tap survives weapon handoff before native draw acknowledgment");
            status=2; lt=rt=0; UpdateGunTriggers(lt,rt,true,20);
            Check(lt==(style==0 ? 255 : 0),"drawing acknowledges toggle request and retains native hold");
            status=4; lt=rt=0; UpdateGunTriggers(lt,rt,true,30);
            Check(lt==(style==0 ? 255 : 0) && !g_gunTriggers.WantsShot(),"released draw tap cannot holster or shoot");
            for (uint64_t now:{40ull,540ull,10000ull}) {
                lt=255; rt=0; UpdateGunTriggers(lt,rt,true,now);
                Check(lt==(style==0 ? 255 : 0),"repeated or held LT never holsters ready guns");
            }
            lt=rt=0;
            Check(UpdateGunTriggers(lt,rt,true,10010,true),"Y claims native Action when guns are held");
            Check(lt==(style==0 ? 0 : 255) && !g_gunTriggers.WantsShot(),"Y immediately holsters without a queued LT shot");
            lt=0; rt=255; UpdateGunTriggers(lt,rt,true,10020,false);
            Check(lt==(style==0 ? 0 : 255) && rt==0 && !g_gunTriggers.WantsShot(),
                  "brief Y survives input polls before native holster and suppresses firing");
            status=3; lt=rt=0; UpdateGunTriggers(lt,rt,true,10030);
            status=0; gun=0; lt=rt=0; UpdateGunTriggers(lt,rt,true,10040);
            Check(lt==0,"completed holster remains unarmed");
            lt=255; rt=0; UpdateGunTriggers(lt,rt,true,10050);
            Check(lt==255,"LT immediately redraws from empty current gun using last weapon");
            gun=int16_t(target); status=2; lt=255; rt=0; UpdateGunTriggers(lt,rt,true,10060);
            status=4; lt=255; rt=0; UpdateGunTriggers(lt,rt,true,10070);
            lt=rt=0; UpdateGunTriggers(lt,rt,true,10080);
            Check(!g_gunTriggers.WantsShot(),"draw squeeze held through readiness never fires on release");
        }
        // Equip/holster remains available without tracked guns or HD graphics.
        for (bool tracked:{false,true}) for (uint8_t hd:{uint8_t(0),uint8_t(1)}) {
            config.firstPersonMotionGuns=tracked; appMemory[0x9e4]=hd; appMemory[0x9ec]=1;
            gun=lastGun=1; status=0; g_gunEquip.Reset(); g_gunTriggers.Reset();
            Check(WeaponControlMode(),"first-person weapon bindings do not require motion guns or HD");
            lt=255; rt=150; UpdateGunTriggers(lt,rt,true,0);
            Check(lt==255 && rt==150,"fallback draws immediately and preserves grab input");
            status=4; lt=rt=0; UpdateGunTriggers(lt,rt,true,10);
            Check(lt==0,"toggle fallback stays armed after release");
            Check(UpdateGunTriggers(lt,rt,true,20,true) && lt==255,"fallback Y holsters");
        }
        config.firstPersonMotionGuns=true;
        lastGun=savedLastGun;
        {
            VRSystem::HandState hands[2]{};
            hands[0].btnUpper=true; hands[1].grip=1;
            XState pad{}; bool shifted=false,gripAction=false;
            BuildStateFromHands(hands,pad,shifted,true,gripAction);
            Check((pad.Gamepad.wButtons & XB_Y) && (pad.Gamepad.wButtons & XB_X) &&
                  !(pad.Gamepad.wButtons & XB_RIGHT_SHOULDER),
                  "right grip cannot steal Y holster in first person");
        }
        // A ready-weapon handoff clears old shots and a pending LT tap without
        // resetting held edges (which would otherwise create an RT shot).
        gun=6; status=4; appMemory[0x9e4]=1; appMemory[0x9ec]=0;
        g_gunTriggers.Reset(); g_gunEquip.Reset(); g_triggerWeapon=6;
        lt=rt=0; UpdateGunTriggers(lt,rt,true,3000);
        lt=rt=255; UpdateGunTriggers(lt,rt,true,3010);
        g_gunTriggers.pending[0]=g_gunTriggers.pending[1]=true;
        gun=1; lt=rt=255; UpdateGunTriggers(lt,rt,true,3020);
        Check(lt==255 && rt==255 && g_gunTriggers.Consume(0) && g_gunTriggers.Consume(1),
              "ready gun handoff sustains held dual firing without an equip gesture");
        lt=rt=0; UpdateGunTriggers(lt,rt,true,3030);
        Check(!g_gunTriggers.WantsShot(),"old LT tap cannot fire new weapon on release");
        lastGun=savedLastGun;
        appMemory[0x9ed]=0;
        lt=180; rt=210; UpdateGunTriggers(lt,rt,false,2000);
        Check(lt==180 && rt==210 && !g_gunTriggers.active,
              "third-person/disabled/chord-owned trigger input is untouched");
        appMemory[0x9ec]=0;
        dll.game=savedGame; dll.getFloor=savedFloor;
        g_longGunHooksReady=false; g_longGunDll=nullptr; g_longShot=nullptr;
        g_gunTriggers.Reset();
        g_handFired[0]=g_handFired[1]=false;
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeJoint));
        VR().m_firstPersonNeutralValid=neutral;
        g_motionHooksReady=false;
        config=savedConfig; status=oldStatus; gun=oldGun; appMemory[0x9e4]=0;
        g_scenePoseValid=savedScene; g_motionDll=nullptr;
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=false;
    }
    g_runtimeEnabled = false; Detour_AimWeapon(nullptr, laraMemory + off::lara_left_arm);
    Check(*reinterpret_cast<int16_t*>(laraMemory + off::lara_left_arm + off::arm_y_rot) == 123, "third person uses native gun aim");
    // The simulation hook must see the original stick intent even after the
    // native modern-controls converter has replaced it with Forward. Exercise
    // tank and modern modes with deliberately unrelated chase-camera headings.
    g_hLaraAboveWater.m_trampoline = reinterpret_cast<void*>(&FakeAboveWater);
    g_hAnimateLara.m_trampoline = reinterpret_cast<void*>(&FakeAnimate);
    g_runtimeEnabled = g_active = g_haveHeading = true;
    g_headingItem = itemMemory; g_headingBase = 0.4f; Head(0.6f);
    constexpr uint64_t preserved = (uint64_t(1) << 40) | 0x40; // high bits + Action
    struct Direction { float x, z; uint64_t action; float offset; int scale, gait; };
    const Direction dirs[] = {
        {0, 1, locomotion::Forward, 0, 1, 1},
        {-1, 0, locomotion::StepLeft, -kPi/2, 3, 22},
        {1, 0, locomotion::StepRight, kPi/2, 3, 21},
        {0, -1, locomotion::Back | locomotion::Walk, kPi, 3, 16},
        {0.2f, 1, locomotion::Forward, 0, 1, 1}
    };
    for (int modern : {0, 1}) for (float cam : {-2.4f, 0.0f, 2.1f}) for (const auto& d : dirs) {
        *reinterpret_cast<int32_t*>(appMemory + drva::app_off::cfgFlags) = modern ? 2 : 0;
        state = int16_t(d.gait); pos = {}; analogMemory[2] = Angle(cam);
        float x = d.x, z = d.z, turn = 0;
        FirstPersonInput(x, z, turn, false);
        Check(g_haveManualInput && Near(g_manualLocal.x, d.x) && Near(g_manualLocal.z, d.z), "simulation retains raw directional intent");
        analogMemory[0] = 6000; analogMemory[1] = 8000; // native axis deadzone result
        analogMemory[2] = Angle(cam); analogMemory[3] = Angle(cam - 0.5f);
        actionInput = preserved | locomotion::Forward | locomotion::Right;
        *reinterpret_cast<int16_t*>(laraMemory + off::lara_turn_rate) = 900;
        const int ticks = animationTicks;
        Detour_LaraAboveWater(itemMemory, nullptr);
        Check(seenInput == (preserved | d.action), "native gait selected; unrelated 64-bit input preserved");
        Check(Near(Radians(seenYaw), 1.0f) && Near(Radians(seenAnalog[2]), 1.0f) && seenAnalog[2] == seenAnalog[3], "body and native input share stable head heading");
        Check(Near(Wrap(Radians(seenMove) - 1.0f - d.offset), 0) && seenTurn == 0, "movement direction set before animation; native turn rate cleared");
        const float decodedAngle = std::atan2(float(seenAnalog[0]), float(seenAnalog[1]));
        Check(Near(Wrap(decodedAngle - d.offset), 0), "native steering is cardinal and independent of chase camera");
        Check(animationTicks == ticks + 1 && pos.x_pos == 3*d.scale && pos.z_pos == 5*d.scale && pos.y_pos == 7, "one animation tick; only horizontal root motion scaled");
        Check(*reinterpret_cast<int16_t*>(itemMemory + off::item_speed) == 10 && g_groundMoveAction == 0, "native animation speed preserved and action scope restored");
    }
    // Side-jump compression uses turn-left/right actions, never sidestep gait.
    for (int s : {2, 15}) {
        state = static_cast<int16_t>(s); pos = {}; float x=-1, z=0, r=0;
        FirstPersonInput(x, z, r, false, true);
        actionInput = preserved | 0x10; analogMemory[0] = 10000; analogMemory[1] = 0;
        Detour_LaraAboveWater(itemMemory, nullptr);
        Check(seenInput == (preserved | 0x10 | locomotion::Left) && pos.x_pos == 3, "side jump keeps native action and unscaled animation");
    }
    // Third person, menu, water and authored animation states pass through.
    for (int guard = 0; guard < 4; ++guard) {
        g_runtimeEnabled = guard != 0; water = guard == 2 ? 1 : 0;
        *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InventoryActive) = guard == 1;
        state = guard == 3 ? 23 : 2; pos = {}; actionInput = preserved | 8;
        analogMemory[2] = Angle(-2.0f);
        Detour_LaraAboveWater(itemMemory, nullptr);
        Check(seenInput == actionInput && seenAnalog[2] == Angle(-2.0f) && pos.x_pos == 3, "non-FP/scripted contexts pass through unchanged");
    }
    // A physical turn moves the headset in an arc around a stationary neck.
    // Use independent sin/cos samples, including nonzero entry headings, to
    // catch the orbit that cancels only after 360 degrees in the old port.
    g_runtimeEnabled = g_active = g_haveHeading = true;
    state = 2; water = 0; g_headingItem = itemMemory;
    config.firstPersonRoomscaleNeckMetres = 0.15f;
    auto physicalPose = [](float yaw, float right = 0, float forward = 0,
                           float up = 0) {
        Head(yaw);
        VR().m_rawHeadPose.m[0][3] = 1 + 0.15f * std::sin(yaw) + right;
        VR().m_rawHeadPose.m[1][3] = 1.7f + up;
        VR().m_rawHeadPose.m[2][3] = -2 - 0.15f * std::cos(yaw) - forward;
        VR().m_headFromTracking = InvertRigid(FromHmd(VR().m_rawHeadPose));
    };
    auto checkViews = [](float right, float forward, float up) {
        float x, z; VR().HeadFloorOffset(x, z);
        Check(Near(x, right) && Near(z, forward), "neck displacement excludes physical eye arc");
        const auto tracked = InvertRigid(VR().TrackedHeadView());
        Check(Near(tracked.r[0][3], right) && Near(tracked.r[1][3], up) &&
              Near(tracked.r[2][3], -forward), "tracked pose keeps actual lean and duck only");
        const auto centre = InvertRigid(VR().HeadView());
        const auto left = InvertRigid(VR().EyeView(Eye::Left));
        const auto rightEye = InvertRigid(VR().EyeView(Eye::Right));
        float ipdSquared = 0;
        for (int i = 0; i < 3; ++i) {
            Check(Near((left.r[i][3] + rightEye.r[i][3]) * 0.5f, centre.r[i][3]),
                  "culling and stereo eyes share corrected origin in same frame");
            const float gap = left.r[i][3] - rightEye.r[i][3];
            ipdSquared += gap * gap;
        }
        Check(Near(std::sqrt(ipdSquared), 64), "physical turns preserve full stereo IPD");
        Check(Near(centre.r[0][3], right * 1000) &&
              Near(centre.r[1][3], (config.flipViewY ? -up : up) * 1000) &&
              Near(centre.r[2][3], -forward * 1000), "rendered displacement matches physical neck motion");
    };
    for (float initial : {0.0f, 0.73f, -2.4f}) {
        physicalPose(initial); VR().RecenterFirstPersonHead();
        for (int step = -72; step <= 72; ++step) {
            const float yaw = initial + step * kPi / 36;
            physicalPose(yaw); checkViews(0, 0, 0);
            physicalPose(yaw, 0.08f, 0.12f, -0.2f);
            checkViews(0.08f, 0.12f, -0.2f);
        }
    }
    // Artificial turns pivot genuine translation around the current neck,
    // without rotating the physical eye arc into a second world-space orbit.
    for (float turn : {-1.2f, 0.5f, kPi / 2}) {
        physicalPose(0.4f); VR().RecenterFirstPersonHead();
        physicalPose(1.1f, 0.08f, 0.12f, -0.2f);
        VR().PivotHeadFloorOffset(turn);
        const float x = std::cos(turn) * 0.08f - std::sin(turn) * 0.12f;
        const float z = std::sin(turn) * 0.08f + std::cos(turn) * 0.12f;
        checkViews(x, z, -0.2f);
        physicalPose(-0.9f, 0.08f, 0.12f, -0.2f);
        checkViews(x, z, -0.2f);
        VR().RecenterFirstPersonHead(); checkViews(0, 0, 0);
    }
    // Exercise the real input call as well, not just the pivot helper.
    physicalPose(0.4f); VR().RecenterFirstPersonHead();
    physicalPose(1.1f, 0.08f, 0.12f);
    g_headingBase = 0.3f;
    lx = ly = 0; rx = 1;
    FirstPersonInput(lx, ly, rx, false);
    renderStickFrame();
    const float turn = Wrap(g_headingBase - 0.3f);
    Check(turn > 0 && rx == 0, "real input plus render update applies artificial turn");
    checkViews(std::cos(turn) * 0.08f - std::sin(turn) * 0.12f,
               std::sin(turn) * 0.08f + std::cos(turn) * 0.12f, 0);
    g_renderTurn.Reset();
    config.firstPersonRoomscaleNeckMetres = 0;
    physicalPose(0); VR().RecenterFirstPersonHead(); physicalPose(kPi / 2);
    checkViews(0.15f, -0.15f, 0);
    g_active = false;
    const auto native = VR().TrackedHeadView();
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j)
        Check(Near(native.r[i][j], VR().m_headFromTracking.r[i][j]),
              "third-person pose remains untouched by neck correction");
    // Native room-scale calls are stubs, but the sweep/consumption/render code
    // is the actual implementation. Exercise both TR4 and TR5 runtime paths.
    dll.getCollisionInfo = rva(reinterpret_cast<void*>(&FakeCollision));
    dll.getFloor = rva(reinterpret_cast<void*>(&FakeFloor));
    dll.getHeight = rva(reinterpret_cast<void*>(&FakeHeight));
    dll.itemNewRoom = rva(reinterpret_cast<void*>(&FakeNewRoom));
    dll.playingCutseq = rva(&cutseq);
    dll.cutseqNum = rva(&cutseqNumber);
    dll.cutseqTrigger = rva(&cutseqTransition);
    dll.useSpotCam = rva(&spotCamera);
    g_hDrawCreatureHD.m_trampoline = reinterpret_cast<void*>(&FakeDraw);
    g_hDrawHair.m_trampoline = reinterpret_cast<void*>(&FakeHair);
    nativeMoves = false; jointFollowsBody = true;
    auto& prev = *reinterpret_cast<PHD_3DPOS*>(itemMemory + off::item_pos_prev);
    auto& vehicle = *reinterpret_cast<int16_t*>(laraMemory + off::lara_vehicle);
    auto& hp = *reinterpret_cast<int16_t*>(itemMemory + off::item_hit_points);
    auto& room = *reinterpret_cast<int16_t*>(itemMemory + off::item_room);
    auto resetRoomscale = [&]() {
        RestoreHeadMesh();
        config.firstPersonRoomscaleMove = config.positionalTracking = true;
        config.firstPersonHeadTranslation = config.firstPersonHideHead = true;
        config.firstPersonRoomscaleDeadzoneMetres = 0.02f;
        config.firstPersonRoomscaleNeckMetres = 0.15f;
        g_active = g_runtimeEnabled = g_haveHeading = true;
        g_headingItem = itemMemory; g_headingBase = g_lastHeadWorld = 0;
        g_haveManualInput = g_shifted = g_jumpPressed = false;
        g_previousBody = {}; pos = {}; prev = {}; state = 2; room = 1;
        // TR5 InitialiseLara zeroes this unused field; only TR4 has the
        // -1/no-vehicle contract consumed by native LaraAboveWater.
        vehicle = dll.game == 0 ? -1 : 0; hp = 1000; water = 0; cutseq = 0;
        cutseqNumber=cutseqTransition=spotCamera=0; vonCroyScene=0;
        dll.vonCroyCutscene=dll.game==0 ? rva(&vonCroyScene) : 0;
        std::memset(appMemory, 0, sizeof(appMemory));
        std::memset(cameraMemory, 0, sizeof(cameraMemory));
        fraction = 0; collisionCalls = floorCalls = roomChanges = 0;
        physicalPose(0); FirstPersonRecenter();
        physicalPose(0, 0.2f, 0, -0.1f);
    };
    for (int which : {0, 1}) {
        game = dll.game = which;
        resetRoomscale();
        UpdateSceneCamera(camera); hp=0;
        const PHD_3DPOS deathCamera{300,-100,400,20,30,40,0}; camera=deathCamera;
        UpdateSceneCamera(camera);
        Check(!g_active && !g_scenePoseValid && g_runtimeEnabled &&
              !std::memcmp(&camera,&deathCamera,sizeof(camera)),"death immediately retains native camera and selected view preference");
        Check(!g_headHidden && !g_ledgeArmsOnly,"death restores native body visibility");
        hp=1000; UpdateSceneCamera(camera);
        Check(g_active && Gate(),"loading a living Lara restores first-person eligibility");
        // Reproduce normal engine initialization per game, then exercise the
        // actual input -> simulation -> animation path for every ground gait.
        // Previously these tests forced TR4's -1 sentinel even for TR5.
        for (int modern : {0, 1}) for (const auto& d : dirs) {
            resetRoomscale(); config.firstPersonRoomscaleMove = false;
            state=int16_t(d.gait);
            nativeMoves = true;
            *reinterpret_cast<int32_t*>(appMemory + drva::app_off::cfgFlags) = modern ? 2 : 0;
            physicalPose(0.6f); g_headingBase = 0.4f;
            Check(CanTurnBody(itemMemory), "native per-game initialization permits ground FP movement");
            TurnBodyToHead(itemMemory, 1.0f / 60);
            Check(pos.y_rot > 0, "physical turn reaches body with native per-game vehicle value");
            analogMemory[2] = Angle(-1.7f);
            float x=d.x, z=d.z, r=0;
            FirstPersonInput(x, z, r, false);
            Check(g_haveManualInput && Near(g_manualLocal.x, d.x) && Near(g_manualLocal.z, d.z),
                  "all directional intents survive native per-game initialization");
            analogMemory[0] = 6000; analogMemory[1] = 8000;
            actionInput = preserved | locomotion::Forward;
            Detour_LaraAboveWater(itemMemory, nullptr);
            Check(seenInput == (preserved | d.action) && Near(Radians(seenYaw), 1.0f),
                  "sidestep/backpedal/forward gait and stable heading work in both games");
            Check(pos.x_pos == 3*d.scale && pos.z_pos == 5*d.scale && pos.y_pos == 7,
                  "directional root scaling remains intact in both games");
            const float oldBase = g_headingBase;
            x=z=0; r=1; FirstPersonInput(x, z, r, false);
            renderStickFrame();
            Check(r == 0 && g_headingBase > oldBase,
                  "stick turn consumes native camera orbit in both games");
            TurnBodyToHead(itemMemory, 1.0f / 60);
            Check(Radians(pos.y_rot) > 1.0f, "body follows stick turn in both games");
            g_renderTurn.Reset();
        }
        nativeMoves = false;
        for (collisionMode = 0; collisionMode <= 10; ++collisionMode) {
            resetRoomscale();
            const int ticks = simulationTicks;
            Detour_LaraAboveWater(itemMemory, nullptr);
            const int expected = collisionMode == 0 || collisionMode == 10 ? 180 :
                                 collisionMode == 9 ? 50 : 0;
            Check(pos.x_pos == expected && pos.z_pos == 0 && pos.y_pos == 0,
                  "room-scale collision accepts only safe horizontal displacement");
            Check(Near(g_dragCurrent.x, expected / 1000.0f) && Near(g_dragCurrent.z, 0),
                  "only collision-accepted motion is recorded for consumption");
            Check(simulationTicks == ticks + 1, "native simulation still runs once after physical steps");
            Check(collisionMode != 10 || (room == 2 && roomChanges == 1),
                  "physical portal crossing updates Lara's room once");
            for (int f : {0, 64, 128, 192, 256}) {
                fraction = f;
                UpdateSceneCamera(camera);
                const float remaining = 0.2f - expected / 1000.0f * f / 256.0f;
                checkViews(remaining, 0, -0.1f);
                if (collisionMode == 0 || collisionMode == 10)
                    Check(std::fabs(camera.x_pos + InvertRigid(VR().HeadView()).r[0][3] - 200) < 1.01f,
                          "clear rendered camera does not double-count interpolated body drag");
            }
        }
        // The real scene path sweeps the final rendered eye (animated anchor
        // plus physical lean), not Lara's body or the untracked camera pose.
        resetRoomscale();
        collisionMode = 0; cameraCollisionCalls = 0;
        UpdateSceneCamera(camera);
        auto eyeX = [&]() {
            return camera.x_pos + InvertRigid(VR().HeadView()).r[0][3];
        };
        Check(std::fabs(eyeX() - 200) < 1.01f && cameraCollisionCalls > 0,
              "clear first-person eye preserves physical lean");
        collisionMode = 11; cameraCollisionCalls = 0;
        UpdateSceneCamera(camera);
        Check(eyeX() >= 80 && eyeX() < 100 && cameraCollisionCalls > 1,
              "wall stops rendered eye with stereo clearance during ground movement");
        collisionMode = 12; state = 2;
        UpdateSceneCamera(camera);
        Check(camera.z_pos == 144, "grounded camera permits looking beyond a ledge");
        state = 3; UpdateSceneCamera(camera);
        Check(camera.z_pos == 144,
              "airborne floor drop does not retract jump camera");
        collisionMode = 13; UpdateSceneCamera(camera);
        Check(camera.z_pos == 0, "wall still blocks airborne camera");
        cameraCollisionCalls = 0; state = 19; collisionMode = 0;
        UpdateSceneCamera(camera);
        Check(camera.z_pos == 16 && cameraCollisionCalls == 0,
              "ledge pull-up retracts forward anchor without ground sweep");
        Check(locomotion::FirstPersonAnchorZ(10, -20, 16) == -20 &&
              locomotion::FirstPersonAnchorZ(2, 144, 16) == 144,
              "interaction setting cannot extend a custom anchor into a wall");
        g_runtimeEnabled = false; cameraCollisionCalls = 0;
        UpdateSceneCamera(camera);
        Check(cameraCollisionCalls == 0, "third-person camera never runs wall sweep");
        collisionMode = 0; resetRoomscale();
        Detour_LaraAboveWater(itemMemory, nullptr);
        fraction = 128; UpdateLocomotion(camera);
        const int queries = collisionCalls;
        Detour_LaraAboveWater(itemMemory, nullptr);
        Check(pos.x_pos == 180 && collisionCalls == queries,
              "next simulation tick excludes drag not yet displayed");
        fraction = 256; UpdateLocomotion(camera); checkViews(0.02f, 0, -0.1f);
        pos.x_pos += 30; UpdateLocomotion(camera); checkViews(0.02f, 0, -0.1f);

        for (int guard = 0; guard < 12; ++guard) {
            resetRoomscale();
            if (guard == 0) config.firstPersonRoomscaleMove = false;
            if (guard == 1) config.positionalTracking = false;
            if (guard == 2) config.firstPersonHeadTranslation = false;
            if (guard == 3) g_shifted = true;
            if (guard == 4) water = 1;
            if (guard == 5) state = 15;
            if (guard == 6) vehicle = 0;
            if (guard == 7) hp = 0;
            if (guard == 8) physicalPose(0, 3, 0);
            if (guard == 9) physicalPose(0, 0.01f, 0);
            if (guard == 10) cutseq = 1;
            if (guard == 11) g_runtimeEnabled = false;
            Detour_LaraAboveWater(itemMemory, nullptr);
            if (guard == 6 && which == 1)
                Check(pos.x_pos == 180 && collisionCalls > 0,
                      "TR5 zero vehicle field does not suppress normal walking");
            else
                Check(pos.x_pos == 0 && collisionCalls == 0,
                      "guarded contexts/deadzone/tracking discontinuity never drag Lara");
        }
        for (float base : {-2.1f, 0.8f, 2.7f}) {
            resetRoomscale();
            physicalPose(0.3f); FirstPersonRecenter();
            physicalPose(0.7f, 0.2f, 0, -0.1f); g_headingBase = base;
            Detour_LaraAboveWater(itemMemory, nullptr);
            Check(std::fabs(pos.x_pos - std::cos(base) * 180) < 1 &&
                  std::fabs(pos.z_pos + std::sin(base) * 180) < 1,
                  "physical steps use tracking-to-world yaw, not head or chase-camera yaw");
            const float acceptedX = pos.x_pos / 1000.0f;
            const float acceptedZ = pos.z_pos / 1000.0f;
            fraction = 128; UpdateLocomotion(camera);
            float right, forward; VR().HeadFloorOffset(right, forward);
            Check(Near(right, 0.2f - (std::cos(base)*acceptedX - std::sin(base)*acceptedZ)*0.5f) &&
                  Near(forward, -(std::sin(base)*acceptedX + std::cos(base)*acceptedZ)*0.5f),
                  "interpolation consumes accepted drag in tracking coordinates");
            const int callsBeforeTurn = collisionCalls;
            float x=0, z=0, r=1;
            FirstPersonInput(x, z, r, false);
            renderStickFrame();
            Detour_LaraAboveWater(itemMemory, nullptr);
            Check(collisionCalls == callsBeforeTurn,
                  "stick turn with partly displayed room-scale drag cannot repeat the physical step");
        }
        resetRoomscale();
        for (int interaction : {15, 23, 45, 56}) {
            state = int16_t(interaction);
            physicalPose(0, 0.25f, -0.15f, -0.1f);
            UpdateLocomotion(camera);
            checkViews(0, 0, -0.1f);
        }
        state = 2; Detour_LaraAboveWater(itemMemory, nullptr);
        Check(collisionCalls == 0, "interaction displacement cannot queue a walk on release");

        // Resume a temporary camera/menu interruption at the last head-world
        // heading, but real relocations and explicit toggles align to Lara.
        resetRoomscale(); physicalPose(0.7f); g_headingBase = 0.4f;
        UpdateLocomotion(camera);
        const float savedHeading = g_lastHeadWorld;
        for (int interruption = 0; interruption < 12; ++interruption) {
            if (interruption==9 && which==1) continue; // no Von Croy in TR5
            *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InventoryActive) = interruption == 0;
            *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InTitle) = interruption == 1;
            cutseq = interruption == 2;
            cutseqNumber=interruption>=4 && interruption<=7 ? 12 : 0;
            cutseqTransition=cutseqNumber ? interruption-3 : 0;
            spotCamera=interruption==8;
            vonCroyScene=interruption==9;
            *reinterpret_cast<int32_t*>(cameraMemory + off::camera_type) =
                interruption==3 ? 1 : interruption>=10 ? interruption-6 : 0;
            const auto nativeCamera=camera;
            auto& cutsceneBits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
            cutsceneBits=0x7fff; SetMeshVisibility(true,false);
            g_gunTriggers.active=true; g_gunTriggers.pending[0]=g_gunTriggers.pending[1]=true;
            g_gunEquip.pending=true;
            UpdateSceneCamera(camera);
            Check(!g_active && !g_haveHeading && !g_scenePoseValid,
                  "UI/cutscene transitions/flyby/tutorial/fixed camera suspend FP");
            Check(g_runtimeEnabled && !std::memcmp(&camera,&nativeCamera,sizeof(camera)),
                  "cutscene uses untouched native camera without toggling FP preference");
            Check(cutsceneBits==0x7fff && !g_headHidden && !g_rollHidden,
                  "native Lara head/body visibility restored during cutscene");
            Check(!g_gunTriggers.active && !g_gunTriggers.WantsShot() && !g_gunEquip.pending,
                  "cutscene cancels queued hand shots and equip gestures");
            const int beforeDraw=draws,beforeHair=hairs;
            Detour_DrawCreatureHD(itemMemory,0,0); Detour_DrawHair(0);
            Check(draws==beforeDraw+1 && hairs==beforeHair+1,
                  "cutscene body and hair use native rendering");
            std::memset(appMemory, 0, sizeof(appMemory)); cutseq = 0;
            cutseqNumber=cutseqTransition=spotCamera=0; vonCroyScene=0;
            *reinterpret_cast<int32_t*>(cameraMemory + off::camera_type) = 0;
            const float resumeHeading=Wrap(g_lastHeadWorld+Wrap(-1.2f-g_lastBodyYaw));
            physicalPose(-0.4f); pos.y_rot = Angle(-1.2f);
            UpdateSceneCamera(camera);
            Check(g_active && std::fabs(Wrap(g_lastHeadWorld-resumeHeading))<0.002f, "resuming FP includes scripted body turn in preserved heading");
        }
        cutseqTransition=4; cutseqNumber=0;
        Check(!ScriptedCameraActive() && Gate(),"stale cutscene transition without an ID does not lock FP off");
        cutseqTransition=0;
        if (which==1) {
            vonCroyScene=1;
            Check(!ScriptedCameraActive(),"TR5 never reads a nonexistent tutorial-scene flag");
            vonCroyScene=0;
        }
        g_runtimeEnabled=false; spotCamera=1; UpdateSceneCamera(camera);
        spotCamera=0; UpdateSceneCamera(camera);
        Check(!g_active && !g_runtimeEnabled,"cutscene completion cannot enable FP for a third-person player");
        g_runtimeEnabled=true; UpdateSceneCamera(camera);
        pos.x_pos = prev.x_pos = 5000; pos.y_rot = Angle(-0.8f);
        UpdateLocomotion(camera);
        Check(Near(g_lastHeadWorld, -0.8f) && Near(g_dragCurrent.x, 0),
              "teleport resets room-scale state and aligns to Lara");

        // Both recenter routes reset the room-scale counters, not just HMD
        // position; otherwise the next render subtracts stale movement.
        resetRoomscale(); g_headingBase = 0.8f;
        g_dragPrevious = {0.02f, 0}; g_dragCurrent = {0.1f, 0}; g_dragShown = {0.05f, 0};
        FirstPersonRecenter(); checkViews(0, 0, 0);
        Check(Near(g_headingBase, 0.8f) && Near(g_dragPrevious.x, 0) &&
              Near(g_dragCurrent.x, 0) && Near(g_dragShown.x, 0), "End recenter preserves heading and clears counters");
        g_dragCurrent = {0.1f, 0}; VR().RecentreOffset();
        Check(Near(g_dragCurrent.x, 0), "Numpad recenter uses the same FP counter reset");

        const auto visibilityGunStatus=*reinterpret_cast<int16_t*>(laraMemory+2);
        *reinterpret_cast<int16_t*>(laraMemory+2)=4; // Existing armed/head/interaction coverage.
        // Roll overrides compose with head hiding and restore the original
        // mask in both classic and HD draw paths, including menu transitions.
        resetRoomscale();
        auto& bits = *reinterpret_cast<uint32_t*>(itemMemory + off::item_mesh_bits);
        constexpr uint32_t baseBits = 0x7ffb;
        for (bool hideHead : {false, true}) for (int roll : {23, 45, 66, 68, 72}) {
            RestoreHeadMesh(); bits = baseBits; config.firstPersonHideHead = hideHead;
            state = int16_t(roll); UpdateSceneCamera(camera);
            Check(g_rollHidden && bits == 0, "roll hides full classic mesh regardless of head setting");
            const int oldDraws = draws, oldHairs = hairs;
            Detour_DrawCreatureHD(itemMemory, 0, 0); Detour_DrawHair(0);
            Check(draws == oldDraws && hairs == oldHairs, "roll hides HD body and braid");
            state = 2; UpdateSceneCamera(camera);
            Check(bits == (hideHead ? baseBits & ~kHeadMeshBit : baseBits),
                  "ending roll restores body while retaining selected head visibility");
            state = int16_t(roll); UpdateSceneCamera(camera);
            *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InventoryActive) = 1;
            UpdateSceneCamera(camera);
            Check(bits == baseBits && !g_rollHidden && !g_headHidden,
                  "inventory during a roll restores full original mask");
            std::memset(appMemory, 0, sizeof(appMemory));
        }
        for (bool hideHead:{false,true}) for (int crouch:{71,80,81,84,85,86,105,106}) {
            RestoreHeadMesh(); bits=baseBits; config.firstPersonHideHead=hideHead;
            state=int16_t(crouch); UpdateSceneCamera(camera);
            Check(g_crouchHidden && bits==0 && !g_rollHidden,
                  "crouch/crawl idle, movement and turns hide classic body independently of head option");
            const int oldDraws=draws, oldHairs=hairs;
            Detour_DrawCreatureHD(itemMemory,0,0); Detour_DrawHair(0);
            Check(draws==oldDraws && hairs==oldHairs,"crouched HD body and hair do not draw");
            state=2; UpdateSceneCamera(camera);
            Check(!g_crouchHidden && bits==(hideHead ? baseBits&~kHeadMeshBit : baseBits),
                  "standing restores the pre-crouch mesh mask");
            state=int16_t(crouch); UpdateSceneCamera(camera);
            spotCamera=1; UpdateSceneCamera(camera);
            Check(!g_crouchHidden && bits==baseBits,"scripted camera restores crouched body normally");
            spotCamera=0; UpdateSceneCamera(camera);
            Check(g_crouchHidden && bits==0,"returning from cutscene restores crouch hiding");
            state=2; UpdateSceneCamera(camera);
        }
        for (bool hideHead:{false,true}) for (int swim:{13,17,18,35,33,34,47,48,49}) {
            RestoreHeadMesh(); bits=baseBits; config.firstPersonHideHead=hideHead;
            water=0; state=2; UpdateSceneCamera(camera);
            for (int swimming:{1,2,1,2}) {
                water=swimming; state=int16_t(swim);
                Check(!Gate(),"water status immediately gates FP regardless of current swimming animation");
                g_gunTriggers.active=true; g_gunTriggers.pending[0]=g_gunTriggers.pending[1]=true;
                g_gunEquip.pending=true; g_renderTurn.Sample(1,TurnTime());
                PHD_3DPOS nativeCamera{123,456,789,10,20,30,0}; camera=nativeCamera;
                UpdateSceneCamera(camera);
                Check(!g_active && !g_scenePoseValid && !g_haveHeading && g_runtimeEnabled &&
                      !std::memcmp(&camera,&nativeCamera,sizeof(camera)),
                      "surface and underwater use untouched native third-person camera while retaining FP preference");
                Check(!g_underwaterHidden && !g_headHidden && bits==baseBits,
                      "swimming restores full native classic Lara visibility");
                Check(!g_renderTurn.valid && !g_gunTriggers.WantsShot() && !g_gunEquip.pending,
                      "water entry cancels pending FP turn/fire/equip state");
                float x=.4f,z=.6f,r=.8f; FirstPersonInput(x,z,r,false);
                Check(x==.4f && z==.6f && r==.8f && !g_haveManualInput,
                      "native swimming stick input is not rewritten by first person");
                uint8_t lt=180,rt=210; UpdateGunTriggers(lt,rt,MotionTriggerMode(),100);
                Check(lt==180 && rt==210,"native swimming triggers remain untouched");
                const int beforeDraw=draws,beforeHair=hairs;
                for (int pass:{0,1,2}) Detour_DrawCreatureHD(itemMemory,0,pass);
                Detour_DrawHair(0);
                Check(draws==beforeDraw+3 && hairs==beforeHair+1,"swimming draws native HD body and braid");
            }
            for (int otherWater:{4,0,3,-1}) {
                water=otherWater; state=2; pos.y_rot=Angle(-1.1f); UpdateSceneCamera(camera);
                Check(g_active && bits==(hideHead ? baseBits&~kHeadMeshBit : baseBits) &&
                      Near(g_lastHeadWorld,-1.1f),
                      "wade/dry/fly/unknown status resumes FP aligned with current Lara facing");
                water=1; UpdateSceneCamera(camera);
            }
            spotCamera=1; water=0; UpdateSceneCamera(camera);
            Check(!g_active,"leaving water during a cutscene cannot override scripted-camera suspension");
            spotCamera=0; UpdateSceneCamera(camera); Check(g_active,"FP resumes after water and scripted camera end");
            water=2; g_runtimeEnabled=false; UpdateSceneCamera(camera);
            water=0; UpdateSceneCamera(camera);
            Check(!g_active && !g_runtimeEnabled && bits==baseBits,
                  "leaving water cannot force FP on when the user selected third person");
            g_runtimeEnabled=true; state=2; UpdateSceneCamera(camera);
        }
        for (bool hideHead:{false,true}) for (int hanging:{10,30,31,75,82,83,139,19,54}) {
            RestoreHeadMesh(); bits=baseBits; config.firstPersonHideHead=hideHead;
            state=int16_t(hanging); UpdateSceneCamera(camera);
            Check(g_ledgeArmsOnly && bits==(baseBits&kArmMeshBits),
                  "ledge idle, shimmy and turns hide classic torso/legs/head but keep both complete arms");
            const int oldHairs=hairs;
            Detour_DrawCreatureHD(itemMemory,0,0); Detour_DrawHair(0);
            Check(drawnBodyBits==kArmMeshBits && drawnUseBits==1 && hairs==oldHairs,
                  "HD ledge pass retains upper arms, forearms and hands, with no body or hair");
            Check(bits==(baseBits&kArmMeshBits),"ledge draw restores persistent mask");
            state=19; UpdateSceneCamera(camera);
            Check(g_ledgeArmsOnly && bits==(baseBits&kArmMeshBits),
                  "pull-up keeps torso hidden while interaction camera anchor is retracted");
            state=2; UpdateSceneCamera(camera);
            Check(!g_ledgeArmsOnly && bits==(hideHead ? baseBits&~kHeadMeshBit : baseBits),
                  "standing after pull-up restores body without losing the saved mesh mask");
            state=int16_t(hanging); UpdateSceneCamera(camera);
            spotCamera=1; UpdateSceneCamera(camera);
            Check(!g_ledgeArmsOnly && bits==baseBits,"cutscene suspends ledge visibility override");
            spotCamera=0; state=2; UpdateSceneCamera(camera);
        }
        for (int nativeState:{0,1,2,3,16,20,21,22,73,75,76,82,83,89,104,107,108}) {
            state=int16_t(nativeState);
            Check(!IsCrouchState(itemMemory),"standing/jump/hang/scripted states are not classified as crouch");
        }
        state=2; config.firstPersonHideHead=true;
        for (uint32_t initialBits:{0u,0x400u,0x7ffbu,UINT32_MAX}) {
            RestoreHeadMesh(); bits=initialBits; UpdateSceneCamera(camera);
            const uint32_t persistentBits=bits;
            for (int pass:{0,1,4}) {
                Detour_DrawCreatureHD(itemMemory,0,pass);
                Check(drawnUseBits==1 && drawnBodyBits==(UINT32_MAX&~kHeadMeshBit) && drawnPass==pass,
                      "FP-first HD body draw enables all native joints except head even with zero/stale initial mask");
                Check(bits==persistentBits,"headless HD body draw never contaminates persistent item mask");
                Detour_DrawCreatureHD(itemMemory,1,pass);
                Check(drawnBodyBits==(persistentBits&~kHeadMeshBit) && bits==persistentBits,
                      "already-masked native passes keep their original restrictions");
            }
            RestoreHeadMesh(); g_active=false;
            Detour_DrawCreatureHD(itemMemory,0,4);
            Check(drawnUseBits==0 && drawnBodyBits==initialBits && bits==initialBits,
                  "third-person draw keeps native mask and unmasked flag unchanged");
        }
        // Empty geometry slots at startup must not compare equal to empty heads.
        alignas(16) static uint8_t objectInfo[off::object_stride]{};
        alignas(16) static uint8_t heads[off::geom_stride*kLaraHeadGeoms]{};
        std::memset(objectInfo,0,sizeof(objectInfo));
        std::memset(heads,0,sizeof(heads));
        const auto oldObjects=dll.objects, oldHeads=dll.gLaraHeads;
        dll.objects=rva(objectInfo); dll.gLaraHeads=rva(heads);
        auto& object=*reinterpret_cast<int16_t*>(itemMemory+off::item_object);
        const auto oldObject=object; object=0;
        Check(!DrawingLaraHead(itemMemory),"uninitialized body and head meshes are not a face match");
        *reinterpret_cast<void**>(objectInfo+off::object_geom+off::geom_mesh)=objectInfo;
        *reinterpret_cast<void**>(heads+off::geom_mesh)=objectInfo;
        Check(DrawingLaraHead(itemMemory),"loaded matching face geometry is still hidden");
        g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
        state=2; g_active=g_scenePoseValid=true; g_headingItem=itemMemory;
        g_renderArm=-1; g_bodySkinScope=true; g_bodyVisualOffset={35,-60};
        float palette[15*12]{}; bits=kArmMeshBits;
        Check(Detour_GetJoints(itemMemory,palette,0)==15,"native body joint count retained");
        uint64_t visible=0; int count=0;
        const float* full=FirstPersonBodyPalette(visible,count);
        Check(full && count==15 && visible==kArmMeshBits && full[3]==35 && full[11]==-60,
              "body draw captures full palette with final camera fit before masking");
        std::memset(palette,0,sizeof(palette));
        Check(full[0]==1 && full[7*12]==1 && full[14*12]==1,
              "native masking cannot destroy captured hidden torso/head transforms");
        g_bodySkinScope=false;
        Check(!FirstPersonBodyPalette(visible,count),"NPC and later native draws cannot inherit body palette");
        // Integration: production GetJoints capture -> native visibility mask
        // -> production dynamic-bone body detection and spring solver.
        for (int jointCount:{15,33}) {
            float fixture[33*12]{};
            int32_t mapping[33]{};
            for (int j=0;j<jointCount;++j) {
                fixture[j*12]=fixture[j*12+5]=fixture[j*12+10]=1.f;
                // Skinning palettes can have overlapping origins; the real
                // classifier deliberately counts separated origins only.
                int origin=jointCount==15 ? (j<7 ? j%4 : j) : (j<20 ? j : 0);
                fixture[j*12+3]=float(100+origin*30);
                mapping[j]=j%15;
            }
            auto* geom=objectInfo+off::object_geom;
            *reinterpret_cast<int32_t*>(geom+28)=jointCount;
            *reinterpret_cast<const int32_t**>(geom+48)=mapping;
            skinFixture=fixture;skinFixtureCount=jointCount;
            for (bool hidden:{true,false}) {
                bits=0x7fff & ~kHeadMeshBit;
                if (hidden) bits &= ~kArmMeshBits;
                g_bodySkinScope=true;
                float nativePalette[33*12]{};
                Check(Detour_GetJoints(itemMemory,nativePalette,0)==jointCount,
                      "physics integration captures native body palette");
                TestDynamicBonesVisibility();
                g_renderArm=0;TestDynamicBonesNativeDraws();g_renderArm=-1;
                g_bodySkinScope=false;
                TestDynamicBonesNativeDraws();
            }
        }
        // The CPU floor-shadow anchor and GPU body must receive the same
        // world-space translation, regardless of yaw, animation interpolation
        // or arm visibility. Native shadow height remains untouched.
        g_hShadowJoint.m_trampoline=reinterpret_cast<void*>(&FakeShadowJoint);
        for (const auto& shadow:kShadowDlls) {
            g_shadowDll=&shadow;
            const auto caller=g_boundBase+shadow.torsoReturn;
            for (int yaw:{0,8192,16384,24576,-32768,-16384}) {
                for (int frac:{0,64,128,255,256}) for (bool hidden:{false,true}) {
                    const double angle=yaw*3.141592653589793/32768.;
                    g_bodyVisualOffset={float(150*std::sin(angle)),float(150*std::cos(angle))};
                    float fixture[15*12]{};
                    for (int j=0;j<15;++j) {
                        fixture[j*12]=fixture[j*12+5]=fixture[j*12+10]=1;
                        fixture[j*12+3]=float(shadowNativePoint.x);
                        fixture[j*12+7]=float(shadowNativePoint.y);
                        fixture[j*12+11]=float(shadowNativePoint.z);
                    }
                    skinFixture=fixture;skinFixtureCount=15;
                    bits=hidden ? 0x7fff&~kArmMeshBits : 0x7fff;
                    float body[15*12]{};
                    Detour_GetJoints(itemMemory,body,0);
                    for (int repeat=0;repeat<2;++repeat) {
                        PHD_VECTOR point{};const int before=shadowJointCalls;
                        GetJointForCaller(itemMemory,&point,7,frac,caller);
                        Check(shadowJointCalls==before+1 && shadowJointFraction==frac &&
                              shadowJointItem==itemMemory,"shadow preserves native interpolation and calls engine once");
                        Check(std::abs(point.x-body[7*12+3])<=.51f &&
                              std::abs(point.z-body[7*12+11])<=.51f &&
                              point.y==shadowNativePoint.y,"floor shadow matches rendered torso without height drift or accumulation");
                    }
                }
            }
            g_bodyVisualOffset={35,-60};
            // Every non-shadow joint caller (eyes, gun muzzles, hit tests,
            // classic rendering) and every non-grounded/third-person case
            // must receive the native result.
            for (int guard=0;guard<10;++guard) {
                auto* item=itemMemory;int joint=7;auto returnAddress=caller;
                if (guard==0) ++returnAddress;
                if (guard==1) joint=14;
                if (guard==2) item=laraMemory;
                if (guard==3) g_active=false;
                if (guard==4) g_scenePoseValid=false;
                if (guard==5) g_headingItem=nullptr;
                if (guard==6) state=3; // jump
                if (guard==7) state=19; // climb
                if (guard==8) water=1;
                if (guard==9) g_shadowDll=nullptr;
                PHD_VECTOR point{};
                GetJointForCaller(item,&point,joint,128,returnAddress);
                Check(point.x==shadowNativePoint.x && point.y==shadowNativePoint.y &&
                      point.z==shadowNativePoint.z,"unrelated callers and inactive body offsets retain native shadow anchor");
                g_active=g_scenePoseValid=true;g_headingItem=itemMemory;
                state=2;water=0;g_shadowDll=&shadow;
            }
        }
        g_shadowDll=nullptr;g_hShadowJoint.m_trampoline=nullptr;
        skinFixture=nullptr;skinFixtureCount=0;
        g_bodyVisualOffset={};
        object=oldObject; dll.objects=oldObjects; dll.gLaraHeads=oldHeads;
        RestoreHeadMesh(); bits=baseBits;
        state = 45; UpdateSceneCamera(camera); FirstPersonToggle();
        Check(bits == baseBits && !g_rollHidden && !g_haveHeading && !g_headingItem,
              "toggle during roll restores mesh and clears heading identity");
        *reinterpret_cast<int16_t*>(laraMemory+2)=visibilityGunStatus;
    }
    // Unarmed movement uses actual rendered masks, including unmasked HD
    // body passes. Animation frames cannot override forward-view arm hiding.
    for (int which:{0,1}) for (bool hideHead:{false,true})
    for (int gait:{0,1,2,5,6,7,16,20,21,22}) {
        game=dll.game=which;resetRoomscale();collisionMode=0;
        config.firstPersonHideHead=hideHead;
        auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
        auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
        const auto oldStatus=status,oldGun=gun;
        status=0;gun=1;state=int16_t(gait);
        auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
        RestoreHeadMesh();bits=0x7fff;g_unarmedArmVisibility={};
        const uint32_t visible=0x7fff & ~(hideHead ? kHeadMeshBit : 0u);
        const uint32_t hidden=visible & ~kArmMeshBits;
        for (int frame=0;frame<40;++frame) {
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number+2)=int16_t(frame);
            Head(frame*.03f,0);UpdateSceneCamera(camera);
            Check(g_unarmedArmsHidden && bits==hidden,"unarmed arms stay hidden looking forward throughout every ground gait");
            for (int pass:{0,1,4}) {
                Detour_DrawCreatureHD(itemMemory,0,pass);
                Check(drawnUseBits==1 && (drawnBodyBits&kArmMeshBits)==0 && bits==hidden,
                      "unmasked HD body passes cannot bring animated arms into forward view");
                Detour_DrawCreatureHD(itemMemory,1,pass);
                Check(drawnBodyBits==hidden && bits==hidden,"masked HD passes preserve arm hiding and restore item bits");
            }
        }
        Head(0,-20*kPi/180);UpdateSceneCamera(camera);
        Check(!g_unarmedArmsHidden && bits==visible,"glancing down restores both complete unarmed arms");
        Detour_DrawCreatureHD(itemMemory,0,0);
        Check((drawnBodyBits&kArmMeshBits)==kArmMeshBits,"unarmed hands render normally when looking down");
        for (float degrees:{-14.f,-11.f,-13.f}) {
            Head(0,degrees*kPi/180);UpdateSceneCamera(camera);
            Check(!g_unarmedArmsHidden,"small pitch fluctuations do not flicker revealed arms");
        }
        Head(0,-9*kPi/180);UpdateSceneCamera(camera);
        Check(g_unarmedArmsHidden,"raising view hides hands before reaching straight ahead");
        Head(0,-12*kPi/180);UpdateSceneCamera(camera);
        Check(g_unarmedArmsHidden,"hidden arms stay hidden until a deliberate downward glance");
        // Preserve a native non-arm visibility change while our arm mask owns
        // those bits; then restore the original arms on a downward glance.
        bits&=~1u;UpdateSceneCamera(camera);
        Head(0,-20*kPi/180);UpdateSceneCamera(camera);
        Check(bits==(visible&~1u),"native body changes survive hiding without permanently losing arms");
        Head(0,0);UpdateSceneCamera(camera);
        for (int nativeStatus:{1,2,3,4,5}) {
            status=int16_t(nativeStatus);UpdateSceneCamera(camera);
            Check(!g_unarmedArmsHidden && (bits&kArmMeshBits)==kArmMeshBits,
                  "drawing, holding and holstering guns or busy hands bypass unarmed movement hiding");
        }
        status=0;
        for (int heldObject:{7,8}) {
            gun=int16_t(heldObject);UpdateSceneCamera(camera);
            Check(!g_unarmedArmsHidden,"flare and torch hands remain visible");
        }
        gun=1;state=10;UpdateSceneCamera(camera);
        Check(!g_unarmedArmsHidden && (bits&kArmMeshBits)==kArmMeshBits,"ledge grabbing keeps native hands visible");
        state=int16_t(gait);UpdateSceneCamera(camera);
        Check(g_unarmedArmsHidden,"returning to unarmed movement reapplies the visibility rule");
        FirstPersonToggle();
        Check(!g_unarmedArmsHidden && bits==0x7ffe,"third-person transition restores arms and native body visibility");
        status=oldStatus;gun=oldGun;
    }
    // A mode switch can happen after this frame's pose was already sampled.
    // Absolute room position must not survive even for that first third-person draw.
    g_active = g_runtimeEnabled = g_runtimeInitialized = true;
    Head(0.7f);
    VR().m_offsetWorld[0] = 500; VR().m_offsetWorld[1] = -900;
    VR().m_offsetWorld[2] = 300; VR().m_offsetValid = true;
    FirstPersonToggle();
    const auto returnedHead = InvertRigid(VR().HeadView());
    Check(Near(returnedHead.r[0][3], 0) && Near(returnedHead.r[1][3], 0) &&
          Near(returnedHead.r[2][3], 0), "FP exit immediately centres third person at current physical position");
    Check(Near(VR().HeadYawRadians(), 0.7f), "handoff preserves physical look rotation");
    auto thirdPersonSample = [&]() {
        auto pose = VR().m_rawHeadPose;
        VR().WorldLockOffset(pose);
        VR().m_headFromTracking = InvertRigid(FromHmd(pose));
        return InvertRigid(VR().HeadView());
    };
    auto centredEyes = [&]() {
        const auto h = InvertRigid(VR().HeadView());
        const auto l = InvertRigid(VR().EyeView(Eye::Left));
        const auto r = InvertRigid(VR().EyeView(Eye::Right));
        float sep = 0;
        for (int i = 0; i < 3; ++i) {
            Check(Near(h.r[i][3], 0) && Near((l.r[i][3]+r.r[i][3])/2, 0),
                  "third-person eyes and culling share freshly centred origin");
            sep += (l.r[i][3]-r.r[i][3])*(l.r[i][3]-r.r[i][3]);
        }
        Check(Near(std::sqrt(sep),64), "third-person recenter preserves stereo IPD");
    };
    centredEyes();
    thirdPersonSample(); centredEyes(); // no camera yet; no absolute-position leak
    worldCameraValid = true; config.headOffsetWorld = true;
    thirdPersonSample(); centredEyes(); // first chase-camera frame
    VR().m_rawHeadPose.m[0][3] += 0.1f;
    VR().m_rawHeadPose.m[1][3] -= 0.05f;
    VR().m_rawHeadPose.m[2][3] += 0.12f;
    auto leaned = thirdPersonSample();
    Check(Near(leaned.r[0][3],100) && Near(leaned.r[1][3],50) && Near(leaned.r[2][3],120),
          "physical leaning after handoff remains tracked from new neutral");
    for (float yaw : {0.3f, 1.5f, -2.0f}) {
        const float c = std::cos(yaw), s = std::sin(yaw);
        worldCameraRot[0][0]=c; worldCameraRot[0][2]=s;
        worldCameraRot[2][0]=-s; worldCameraRot[2][2]=c;
        worldCameraPos[0] += 50;
        const auto h = thirdPersonSample();
        const float v[3] = {h.r[0][3], h.r[1][3], -h.r[2][3]};
        const float expected[3] = {100,50,-120};
        for (int i = 0; i < 3; ++i)
            Check(Near(worldCameraRot[0][i]*v[0]+worldCameraRot[1][i]*v[1]+worldCameraRot[2][i]*v[2], expected[i]),
                  "chase-camera rotation and forward movement do not orbit physical lean");
    }
    VR().RecentreOffset(); centredEyes(); thirdPersonSample(); centredEyes();
    config.headOffsetWorld = false;
    VR().m_rawHeadPose.m[0][3] += 0.1f;
    Check(Near(thirdPersonSample().r[0][3],100), "camera-relative offset option also uses handoff neutral");
    VR().RecenterThirdPersonHead(); thirdPersonSample(); centredEyes();
    config.headOffsetWorld = true;

    // Suspension into inventory follows the same handoff path as Y+LT.
    resetRoomscale(); Head(-0.9f);
    *reinterpret_cast<int32_t*>(appMemory + drva::app_off::InventoryActive) = 1;
    UpdateSceneCamera(camera);
    Check(!g_active, "inventory suspends first person");
    centredEyes(); thirdPersonSample(); centredEyes();
    std::memset(appMemory, 0, sizeof(appMemory));
    g_active = g_runtimeEnabled = true; VR().m_poseValid = false;
    FirstPersonToggle(); centredEyes();
    VR().m_poseValid = true; Head(1.2f);
    thirdPersonSample(); centredEyes();
    Check(!VR().m_thirdPersonRecenterPending, "tracking reacquisition completes deferred handoff neutral");
    game = 2; config.headOffsetWorld = false; Head(0);
    const auto tr6Head = thirdPersonSample();
    Check(Near(tr6Head.r[0][3],1000) && Near(tr6Head.r[1][3],-1700) && Near(tr6Head.r[2][3],-2000),
          "TR6 tracking does not inherit a TR4/5 first-person neutral");
    // Cold startup must equal a centred FP handoff without requiring a toggle.
    // Do not capture a headset on the desk during loading/title presentation.
    for (int which:{0,1}) for (bool worldLocked:{false,true}) {
        game=which; g_active=false; config.headOffsetWorld=worldLocked; worldCameraValid=false;
        VR().m_thirdPersonNeutralValid=VR().m_thirdPersonRecenterPending=false;
        VR().m_offsetValid=VR().m_recentreRequested=false; Head(0);
        thirdPersonSample(); centredEyes();
        Check(!VR().m_thirdPersonNeutralValid,"cold startup does not anchor before a valid gameplay camera");
        worldCameraValid=true;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InTitle)=1;
        thirdPersonSample(); centredEyes();
        Check(!VR().m_thirdPersonNeutralValid,"title camera cannot capture an early desk-height neutral");
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InTitle)=0;
        VR().m_poseValid=false;
        thirdPersonSample(); centredEyes();
        Check(!VR().m_thirdPersonNeutralValid,"invalid tracking cannot initialize startup neutral");
        VR().m_poseValid=true; Head(0.7f);
        VR().m_rawHeadPose.m[0][3]=4; VR().m_rawHeadPose.m[1][3]=1.8f; VR().m_rawHeadPose.m[2][3]=-3;
        thirdPersonSample(); centredEyes();
        Check(VR().m_thirdPersonNeutralValid && Near(VR().m_thirdPersonNeutral[0],4) &&
              Near(VR().m_thirdPersonNeutral[1],1.8f) && Near(VR().m_thirdPersonNeutral[2],-3),
              "first gameplay frame captures current physical position, not earlier loading pose");
        Check(Near(VR().HeadYawRadians(),0.7f),"startup centering preserves physical look rotation");
        for (float yaw:{-3.f,-1.5f,0.f,1.5f,3.f}) for (float pitch:{-0.8f,0.f,0.8f}) {
            const float c=std::cos(yaw),s=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
            const float rotation[3][3]={{c,0,s},{sp*s,cp,-sp*c},{-cp*s,sp,cp*c}};
            std::memcpy(worldCameraRot,rotation,sizeof(rotation));
            worldCameraPos[0]+=20; worldCameraPos[1]-=10; worldCameraPos[2]+=30;
            thirdPersonSample(); centredEyes();
            Check(Near(VR().m_thirdPersonNeutral[0],4),"stick camera yaw/pitch does not rebase physical neutral");
        }
        const float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
        std::memcpy(worldCameraRot,identity,sizeof(identity));
        VR().m_rawHeadPose.m[0][3]+=0.1f;
        const auto moved=thirdPersonSample();
        Check(std::fabs(moved.r[0][3]-100)<0.01f,"real lean remains tracked after automatic startup centering");
        VR().RecentreOffset(); thirdPersonSample(); centredEyes();
        worldCameraPos[0]+=10000;
        thirdPersonSample(); centredEyes();
        Check(VR().m_thirdPersonNeutralValid,"camera teleport cannot restore absolute room-space offset");
    }
    game = 1;
    // Diagnostic capture must distinguish camera animation from real motion,
    // and must not change any of the gameplay state it observes.
    resetRoomscale(); Head(0); FirstPersonRecenter();
    config.firstPersonDriftLog = true;
    g_haveManualInput = true; g_manualLocal = {0,1}; state = 1;
    float traceBody[3] = {0,0,0};
    PHD_VECTOR traceHead{10,-700,144};
    const auto savedPos = pos;
    const auto savedHeading = g_headingBase;
    TraceForwardCamera(itemMemory, traceBody, traceHead, 64, 1000);
    traceBody[2] = 20; traceHead = {-10,-700,164};
    TraceForwardCamera(itemMemory, traceBody, traceHead, 128, 1016);
    Check(Near(g_cameraMotionTrace.animatedSide.low,-10) &&
          Near(g_cameraMotionTrace.animatedSide.high,10) &&
          Near(g_cameraMotionTrace.rootSideStep.high,0) &&
          Near(g_cameraMotionTrace.eyeSideStep.low,-20),
          "diagnostics distinguish animated sideways sway from straight root movement");
    VR().m_rawHeadPose.m[0][3] += 0.025f;
    traceBody[2] = 40; traceHead.z = 184;
    TraceForwardCamera(itemMemory, traceBody, traceHead, 192, 1032);
    Check(Near(g_cameraMotionTrace.trackedSide.high,0.025f) &&
          Near(g_cameraMotionTrace.eyeSideStep.high,25),
          "diagnostics identify genuine physical lean separately");
    traceBody[0] = 5; traceBody[2] = 60; traceHead = {-5,-700,204};
    TraceForwardCamera(itemMemory, traceBody, traceHead, 256, 1048);
    Check(Near(g_cameraMotionTrace.rootSideStep.high,5) &&
          g_cameraMotionTrace.fracMin == 64 && g_cameraMotionTrace.fracMax == 256,
          "diagnostics identify root sideways movement and interpolation range");
    Check(std::memcmp(&savedPos,&pos,sizeof(pos)) == 0 && Near(savedHeading,g_headingBase),
          "camera diagnostics do not mutate body position or heading");
    TraceForwardCamera(itemMemory, traceBody, traceHead, 256, 2000);
    Check(g_cameraMotionTrace.samples == 0, "diagnostics log then reset the one-second window");
    TraceForwardCamera(itemMemory, traceBody, traceHead, 0, 2016);
    g_manualLocal = {1,0};
    TraceForwardCamera(itemMemory, traceBody, traceHead, 128, 2032);
    Check(g_cameraMotionTrace.samples == 0, "non-forward movement resets diagnostic window");
    config.firstPersonDriftLog = false; g_manualLocal = {0,1};
    TraceForwardCamera(itemMemory, traceBody, traceHead, 128, 2048);
    Check(g_cameraMotionTrace.samples == 0, "disabled diagnostics remain inactive");
    // Reproduce the reported forward -> side/back launch. Native transitions
    // retain the old gait for several frames, pass through stop, then enter
    // the requested gait INSIDE AnimateLara. Input intent is not gait state.
    for (int which:{0,1}) for (bool smoothing:{false,true}) for (int modern:{0,1})
        for (float heading:{0.f,0.73f,-2.4f}) for (int outgoing:{0,1,16,21,22})
        for (const auto& d:dirs) {
        const int incoming=d.action==locomotion::StepLeft ? 22 :
            d.action==locomotion::StepRight ? 21 : (d.action&locomotion::Back) ? 16 : 1;
        if (outgoing==incoming || (outgoing==0 && incoming==1)) continue;
        game=dll.game=which;
        resetRoomscale(); physicalPose(heading); FirstPersonRecenter();
        config.firstPersonRoomscaleMove=false;
        config.firstPersonMovementStabilization=smoothing;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
        auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
        auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
        auto& frame=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number+2);
        state=int16_t(outgoing); goal=state; flags=0; frame=0;
        collisionPush={}; blockStableMotion=collisionStartsFall=false;
        animationNextState=-1;
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeGaitTransitionAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAboveWater);
        // Prime the actual smoother with the outgoing animation's speed.
        float x=outgoing==22 ? -1.f : outgoing==21 ? 1.f : 0.f;
        float z=outgoing==16 ? -1.f : (outgoing==0 || outgoing==1) ? 1.f : 0.f, r=0;
        FirstPersonInput(x,z,r,false); Detour_LaraAboveWater(itemMemory,nullptr);
        x=d.x; z=d.z; FirstPersonInput(x,z,r,false);
        for (int tick=0;tick<10;++tick) {
            goal=int16_t(tick<4 ? 2 : incoming);
            animationNextState=tick==4 ? 2 : tick==7 ? incoming : -1;
            const auto before=pos;
            const int oldFrame=frame, animations=animationTicks, collisions=nativeCollisionPasses;
            Detour_LaraAboveWater(itemMemory,nullptr);
            if (tick<7) {
                Check(beforeNativeCollision.x==0 && beforeNativeCollision.z==0,
                      "direction change cannot redirect or boost the outgoing gait or stopping animation");
                Check(!g_rootMotion.valid,"direction handoff clears the old gait's speed history");
            } else {
                const float expectedSpeed=incoming==1 ? 70.f : 36.f;
                Check(std::fabs(locomotion::Length(beforeNativeCollision)-expectedSpeed)<2,
                      "new gait starts at its own speed on the animation transition tick");
                const float yaw=heading+d.offset;
                Check(std::fabs(beforeNativeCollision.x*std::cos(yaw)-beforeNativeCollision.z*std::sin(yaw))<2,
                      "accepted motion agrees with the new gait's collision direction");
            }
            Check(pos.y_pos==before.y_pos+7 && frame==oldFrame+1 && animationTicks==animations+1 &&
                  nativeCollisionPasses==collisions+1,
                  "direction handoff advances animation and preserves vertical motion and native collision");
        }
        // Native collision retains final authority over walls and corrections.
        blockStableMotion=true; collisionPush={3,-2};
        const auto blocked=pos; Detour_LaraAboveWater(itemMemory,nullptr);
        Check(pos.x_pos==blocked.x_pos+3 && pos.z_pos==blocked.z_pos-2,
              "gait handoff cannot undo native wall collision or pushback");
        blockStableMotion=false; collisionPush={}; animationNextState=-1;
        goal=0; flags=0;
    }
    // Immediate handoff re-enters the native standing dispatcher before its
    // terrain checks; the incoming animation supplies velocity on this tick.
    *reinterpret_cast<int16_t*>(immediateGaitAnimations+11*48+10)=2;
    *reinterpret_cast<int16_t*>(immediateGaitAnimations+11*48+28)=184;
    *reinterpret_cast<int16_t*>(immediateGaitAnimations+11*48+30)=185;
    for (int which:{0,1}) for (bool smoothing:{false,true}) for (int modern:{0,1})
        for (float heading:{0.f,0.73f,-2.4f}) for (const auto& d:dirs) {
        if (d.action==locomotion::Forward) continue;
        for (int oldAnim:{0,1,2,3,6,8,10,11,38,39,40,41,65,66,67,68,103}) {
            game=dll.game=which; resetRoomscale(); physicalPose(heading); FirstPersonRecenter();
            config.firstPersonRoomscaleMove=false; config.firstPersonMovementStabilization=smoothing;
            *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
            auto& anim=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
            auto& frame=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number+2);
            auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
            auto& required=*reinterpret_cast<int16_t*>(itemMemory+22);
            anim=int16_t(oldAnim); frame=5; required=0;
            state=goal=oldAnim==11 || oldAnim==103 ? 2 : oldAnim>=65 && oldAnim<=66 ? 22 :
                oldAnim>=67 && oldAnim<=68 ? 21 : oldAnim>=38 && oldAnim<=41 ? 16 :
                oldAnim>=1 && oldAnim<=3 ? 0 : 1;
            *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=0;
            collisionPush={}; blockStableMotion=collisionStartsFall=blockGaitStart=false;
            actionInput=0; animationNextState=-1; standingGaitChecks=0;
            dll.anims=rva(&immediateGaitTable);
            g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeGaitTransitionAnimate);
            g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeImmediateGaitAboveWater);
            float x=d.x,z=d.z,r=0; FirstPersonInput(x,z,r,false);
            const auto before=pos; const int animations=animationTicks, collisions=nativeCollisionPasses;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(state==d.gait && std::fabs(locomotion::Length(beforeNativeCollision)-36)<2,
                  "side/back starts on the first simulation tick, including reversals and repressed stopping gaits");
            Check(pos.y_pos==before.y_pos+7 && animationTicks==animations+1 && nativeCollisionPasses==collisions+1,
                  "immediate gait entry does not run extra animation or collision ticks");
            Check(std::fabs(beforeNativeCollision.x*std::cos(heading+d.offset)-
                  beforeNativeCollision.z*std::sin(heading+d.offset))<2,
                  "immediate gait uses matching movement and collision heading");
            const int checked=standingGaitChecks;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(standingGaitChecks==checked,"held side/back does not restart an active gait each tick");
            goal=2; // Release/re-press before the current gait enters its stop clip.
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(state==d.gait && standingGaitChecks==checked+1 && locomotion::Length(beforeNativeCollision)>30,
                  "repress cancels a pending stop before the stop animation begins");
            // Model a floor/ceiling rejection in native lara_as_stop.
            state=goal=1; anim=0; frame=4; blockGaitStart=true;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(state==2 && beforeNativeCollision.x==0 && beforeNativeCollision.z==0 &&
                  standingGaitChecks==checked+2,"native standing terrain rejection prevents immediate movement");
            // Authored poses, requested actions and missing animation data keep
            // the previously validated wait-for-gait fallback.
            for (int guard=0;guard<8;++guard) {
                state=goal=1; anim=0; frame=4; required=0; actionInput=d.action;
                *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=0;
                dll.anims=rva(&immediateGaitTable); immediateGaitTable=immediateGaitAnimations;
                if (guard==0) { state=goal=2; anim=24; } // impact kneel
                if (guard==1) anim=55; // step-up animation
                if (guard==2) goal=15;
                if (guard==3) required=56;
                if (guard==4) *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=8;
                if (guard==5) actionInput|=0x40;
                if (guard==6) dll.anims=0;
                if (guard==7) immediateGaitTable=nullptr;
                const int oldState=state,oldGoal=goal,animation=anim;
                PrepareGroundDirection(itemMemory,d.action);
                Check(anim==animation && state==oldState && goal==oldGoal && frame==4,
                      "instant handoff preserves landing, steps, jump/interaction, gravity and fallback behavior");
            }
            dll.anims=0; immediateGaitTable=immediateGaitAnimations;
            blockGaitStart=false; animationNextState=-1; required=0; goal=0;
            *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=0;
        }
    }
    // Continuous zigzags must move on EVERY tick, including side -> forward.
    // Use the production input/simulation/animation path and retain collision.
    for (int which:{0,1}) for (bool smoothing:{false,true}) for (int modern:{0,1})
    for (float heading:{0.f,0.73f,-2.4f}) {
        game=dll.game=which;resetRoomscale();physicalPose(heading);FirstPersonRecenter();
        config.firstPersonRoomscaleMove=false;config.firstPersonMovementStabilization=smoothing;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
        auto& anim=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
        auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
        state=goal=1;anim=0;
        *reinterpret_cast<int16_t*>(itemMemory+22)=0;
        *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=0;
        dll.anims=rva(&immediateGaitTable);
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeGaitTransitionAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeImmediateGaitAboveWater);
        blockStableMotion=collisionStartsFall=blockGaitStart=false;collisionPush={};
        const int sequence[]={1,0,2,0,1,2,4,3,0};
        for (int tick=0;tick<90;++tick) {
            const auto& d=dirs[sequence[tick%9]];
            actionInput=0;
            float x=d.x,z=d.z,turn=0;FirstPersonInput(x,z,turn,false);
            const int animations=animationTicks,collisions=nativeCollisionPasses;
            const auto old=pos;
            Detour_LaraAboveWater(itemMemory,nullptr);
            const float expected=d.action==locomotion::Forward ? 70.f : 36.f;
            Check(state==d.gait && std::fabs(locomotion::Length(beforeNativeCollision)-expected)<2,
                  "continuous zigzag enters forward/side/back gait with nonzero bounded motion every tick");
            Check(animationTicks==animations+1 && nativeCollisionPasses==collisions+1 && pos.y_pos==old.y_pos+7,
                  "zigzag retains one native animation/collision tick and vertical motion");
            Check(std::fabs(beforeNativeCollision.x*std::cos(heading+d.offset)-
                            beforeNativeCollision.z*std::sin(heading+d.offset))<2,
                  "zigzag movement agrees with incoming gait collision direction");
        }
        // Forward entry still respects native standing terrain rejection.
        state=goal=22;anim=65;blockGaitStart=true;
        float x=0,z=1,turn=0;actionInput=0;FirstPersonInput(x,z,turn,false);
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==2 && locomotion::Length(beforeNativeCollision)==0,
              "blocked forward entry never overrides native terrain rejection");
        blockGaitStart=false;state=goal=21;anim=67;blockStableMotion=true;
        const auto blocked=pos;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(locomotion::Length(beforeNativeCollision)>60 &&
              pos.x_pos==blocked.x_pos && pos.z_pos==blocked.z_pos,
              "instant forward entry preserves native wall collision");
        blockStableMotion=false;dll.anims=0;animationNextState=-1;goal=0;
    }
    // Backpedal startup is a 16-frame, constant 2-unit crawl before the
    // 10-unit loop. Advance only that command-free clip four frames per tick.
    for (int which:{0,1}) for (bool smoothing:{false,true}) {
        game=dll.game=which; resetRoomscale(); physicalPose(0); FirstPersonRecenter();
        config.firstPersonRoomscaleMove=false; config.firstPersonMovementStabilization=smoothing;
        auto& anim=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
        auto& frame=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number+2);
        auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
        auto* start=immediateGaitAnimations+41*48;
        *reinterpret_cast<int16_t*>(start+10)=16;
        *reinterpret_cast<int16_t*>(start+28)=744;
        *reinterpret_cast<int16_t*>(start+30)=759;
        state=goal=16; anim=41; frame=744; actionInput=0;
        *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=0;
        dll.anims=rva(&immediateGaitTable);
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeBackpedalAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAboveWater);
        collisionPush={}; blockStableMotion=collisionStartsFall=false;
        float x=0,z=-1,r=0; FirstPersonInput(x,z,r,false);
        for (int tick=0;tick<20;++tick) {
            const int animations=animationTicks,collisions=nativeCollisionPasses;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(tick<3 ? anim==41 && frame==748+tick*4 : anim==40 && frame==684+tick-3,
                  "backpedal reaches its normal loop after four startup ticks without speeding up the loop");
            Check(beforeNativeCollision.x==0 && beforeNativeCollision.z>=-30 && beforeNativeCollision.z<0,
                  "faster startup keeps backward motion bounded by existing top speed");
            Check(*reinterpret_cast<int16_t*>(itemMemory+off::item_speed)==(anim==41 ? 2 : 10),
                  "startup acceleration never boosts persistent native speed");
            Check(animationTicks==animations+1 && nativeCollisionPasses==collisions+1 && pos.y_pos==tick+1,
                  "startup shortening preserves one animation/collision call and native vertical motion");
            if (tick==11) Check(beforeNativeCollision.z<=-27,"backpedal reaches 90 percent speed promptly with smoothing enabled");
        }
        blockStableMotion=true; const auto blocked=pos;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(pos.z_pos==blocked.z_pos,"accelerated backpedal obeys native wall collision");
        blockStableMotion=false; x=z=0; FirstPersonInput(x,z,r,false); actionInput=0;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(beforeNativeCollision.z==0,"accelerated backpedal still stops immediately on release");
        // Authored commands, stops and missing data keep their original timing.
        for (int guard=0;guard<6;++guard) {
            state=goal=16; anim=41; frame=744; dll.anims=rva(&immediateGaitTable);
            uint64_t action=locomotion::Back|locomotion::Walk;
            if (guard==0) *reinterpret_cast<int16_t*>(start+40)=1;
            if (guard==1) goal=2;
            if (guard==2) action=locomotion::StepLeft;
            if (guard==3) anim=40;
            if (guard==4) dll.anims=0;
            if (guard==5) frame=743;
            const int savedFrame=frame;
            AccelerateBackpedalStart(itemMemory,action);
            Check(frame==savedFrame,"non-startup/command-bearing/invalid animations are not accelerated");
            *reinterpret_cast<int16_t*>(start+40)=0;
        }
        std::memset(start,0,48); dll.anims=0; goal=0;
    }
    // Side/back movement must never boost persistent native velocity or
    // multiply an animation transition/relocation. Exercise both sides,
    // backward, both games and the optional smoothing path.
    for (int which:{0,1}) for (bool smoothing:{false,true}) for (const auto& d:dirs) {
        game=dll.game=which;
        auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
        auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
        auto& speed=*reinterpret_cast<int16_t*>(itemMemory+off::item_speed);
        auto setup=[&]() {
            resetRoomscale(); physicalPose(0); FirstPersonRecenter();
            config.firstPersonMovementStabilization=smoothing;
            config.firstPersonRoomscaleMove=false;
            state=goal=int16_t(d.gait); flags=0; speed=10;
            startAirborne=blockStableMotion=animationStartsGravity=false;
            animationRetainsSpeed=collisionStartsFall=false;
            animationNextState=animationNextGoal=-1;
            unstableStep={0,10}; unstableHeight=7; collisionPush={};
            g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAnimate);
            g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAboveWater);
            float x=d.x,z=d.z,r=0; FirstPersonInput(x,z,r,false);
        };
        setup();
        for (int tick=0;tick<80;++tick) {
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(speed==10,"held directional movement never feeds boosted speed into native physics");
            Check(locomotion::Length(beforeNativeCollision)<=10*d.scale+1,
                  "held directional movement remains bounded across simulation ticks");
        }
        collisionStartsFall=true;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(speed==10 && state==3,"collision-triggered fall inherits unboosted native speed");
        collisionStartsFall=false; animationRetainsSpeed=true;
        for (int tick=0;tick<80;++tick) {
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(speed==10 && locomotion::Length(beforeNativeCollision)<=11 && !g_rootMotion.valid,
                  "holding side/back through a fall cannot compound inherited speed");
        }
        for (int transition=0;transition<8;++transition) {
            setup();
            if (transition==0) flags=8; // Gravity can precede a state change.
            if (transition==1) animationStartsGravity=true;
            if (transition==2) animationNextState=3;
            if (transition==3) animationNextGoal=15;
            if (transition==4) goal=56;
            if (transition==5) animationNextState=19;
            if (transition==6) unstableStep={600,10}; // Authored relocation.
            if (transition==7) { flags=8; animationRetainsSpeed=true; }
            Detour_LaraAboveWater(itemMemory,nullptr);
            if (transition==7)
                Check(speed==10 && locomotion::Length(beforeNativeCollision)<=11,
                      "gravity in an eligible ground state cannot multiply persistent speed");
            else
                Check(beforeNativeCollision.x==unstableStep.x && beforeNativeCollision.z==unstableStep.z &&
                      speed==int16_t(locomotion::Length(unstableStep)),
                      "ground transitions and relocations preserve native displacement and speed");
            Check(pos.y_pos==7 && !g_rootMotion.valid,
                  "excluded root correction preserves vertical motion and clears smoothing history");
        }
        animationRetainsSpeed=collisionStartsFall=animationStartsGravity=false;
        animationNextState=animationNextGoal=-1; goal=0; flags=0;
    }
    // Enabled stabilization: test the production animation/simulation hooks,
    // not only a camera math helper, and retain post-animation collision.
    for (int which:{0,1}) {
        game=dll.game=which;
        for (const auto& d:dirs) {
            resetRoomscale(); physicalPose(0); FirstPersonRecenter();
            state=int16_t(d.gait);
            config.firstPersonMovementStabilization=true;
            g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAnimate);
            g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAboveWater);
            float x=d.x,z=d.z,r=0; FirstPersonInput(x,z,r,false);
            const auto unit=locomotion::CardinalMovement({d.x,d.z});
            const auto direction=unit*(1/locomotion::Length(unit));
            unstableHeight=7; startAirborne=false; blockStableMotion=false;
            float filteredLow=10000,filteredHigh=0;
            for (int tick=0;tick<80;++tick) {
                const int amount=tick%2 ? 12 : 8;
                unstableStep={direction.x*amount+direction.z*3,
                              direction.z*amount-direction.x*3};
                const auto before=pos;
                const int animations=animationTicks,collisions=nativeCollisionPasses;
                Detour_LaraAboveWater(itemMemory,nullptr);
                const auto step=beforeNativeCollision;
                Check(std::fabs(step.x*direction.z-step.z*direction.x)<0.01f,
                      "all stabilized ground directions remove native lateral root wobble");
                Check(pos.y_pos==before.y_pos+7 && animationTicks==animations+1 &&
                      nativeCollisionPasses==collisions+1,
                      "stabilization preserves vertical motion and one animation/collision pass");
                if (tick>20) {
                    const float length=locomotion::Length(step);
                    filteredLow=std::min(filteredLow,length); filteredHigh=std::max(filteredHigh,length);
                }
            }
            Check(filteredHigh-filteredLow<4*d.scale,
                  "native speed pulses are reduced for forward, sidestep and backpedal");
            blockStableMotion=true; const auto blocked=pos;
            for (int tick=0;tick<20;++tick) Detour_LaraAboveWater(itemMemory,nullptr);
            Check(pos.x_pos==blocked.x_pos && pos.z_pos==blocked.z_pos,
                  "native wall collision remains authoritative after stabilized movement");
            blockStableMotion=false; unstableStep={}; unstableHeight=0;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(pos.x_pos==blocked.x_pos && pos.z_pos==blocked.z_pos && !g_rootMotion.valid,
                  "zero native motion stops immediately without filter drift or accumulated wall motion");
            x=z=r=0; FirstPersonInput(x,z,r,false);
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(!g_rootMotion.valid,"stick release discards speed history");
            unstableStep={3,5}; startAirborne=true;
            x=0; z=1; FirstPersonInput(x,z,r,false);
            const auto airborne=pos; Detour_LaraAboveWater(itemMemory,nullptr);
            Check(state==3 && pos.x_pos==airborne.x_pos+3 && pos.z_pos==airborne.z_pos+5 && !g_rootMotion.valid,
                  "native animation transition to airborne bypasses ground root filtering");
            startAirborne=false;
        }
        // Render the same moving root through deliberately violent head sway.
        // Start/stop and all standing gaits share one eye reference; HMD lean
        // remains a separate, immediate translation after the scene camera.
        resetRoomscale(); physicalPose(0); FirstPersonRecenter();
        collisionMode=0; gaitSway={}; fraction=256;
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));
        Check(Anchor(camera),"stable-eye baseline captured from the native head");
        for (int gait:{0,1,16,20,21,2}) {
            state=int16_t(gait);
            for (int tick=0;tick<24;++tick) {
                prev=pos; pos.x_pos+=5; pos.z_pos+=9; pos.y_pos+=2;
                gaitSway={tick%2 ? 180 : -180,tick%3 ? 90 : -90,tick%2 ? -130 : 130};
                for (int f:{0,64,128,192,256}) {
                    fraction=f;
                    Check(Anchor(camera),"stabilized gait camera remains valid");
                    const float t=f/256.f;
                    Check(std::fabs(camera.x_pos-(prev.x_pos+5*t))<=0.51f &&
                          std::fabs(camera.z_pos-(prev.z_pos+9*t+144))<=0.51f &&
                          std::fabs(camera.y_pos-(prev.y_pos+2*t-700))<=0.51f,
                          "ground eye ignores lateral, forward and vertical gait animation but follows interpolated root");
                }
            }
        }
        prev=pos; physicalPose(0,0.025f,0,-0.04f);
        Anchor(camera);
        checkViews(0.025f,0,-0.04f);
        const auto lean=InvertRigid(VR().HeadView());
        Check(std::fabs(camera.x_pos+lean.r[0][3]-(pos.x_pos+25))<1,
              "physical head translation is immediate, not EMA filtered");
        config.firstPersonMovementStabilization=false; gaitSway={100,50,70};
        Check(Anchor(camera) && camera.x_pos==pos.x_pos+100 && camera.y_pos==pos.y_pos-650 &&
              camera.z_pos==pos.z_pos+214 && !g_groundEye.valid,
              "INI opt-out restores the native animated eye");
        config.firstPersonMovementStabilization=true;
        state=71;
        Check(Anchor(camera) && camera.x_pos==pos.x_pos+100 && !g_groundEye.valid,
              "reverted crouch behavior stays native and never receives grounded-eye stabilization");
        state=2; gaitSway={}; Anchor(camera);
        pos.x_pos+=10000; prev=pos; gaitSway={10,20,30};
        Check(Anchor(camera) && camera.x_pos==pos.x_pos && camera.y_pos==pos.y_pos-700,
              "same-body relocation preserves standing calibration through transient animation");
        ++*reinterpret_cast<int32_t*>(appMemory+drva::app_off::level);
        Check(Anchor(camera) && camera.x_pos==pos.x_pos+10 && camera.y_pos==pos.y_pos-680,
              "different level invalidates old standing calibration even if Lara address is reused");
        state=2; g_runtimeEnabled=false; UpdateSceneCamera(camera);
        Check(!g_groundEye.valid && !g_rootMotion.valid,"leaving first person clears stabilization state");
    }
    // Stand -> jump/hang -> vault/pull-up -> stand. The last climb skeleton
    // can coexist with a newly relocated standing root during interpolation.
    // It must never replace the persistent standing reference (sky/side drift).
    for (int which:{0,1}) for (float heading:{0.f,0.8f,-1.9f}) for (int anchor:{80,100,144}) {
        game=dll.game=which; resetRoomscale(); ResetMovementStabilization();
        config.firstPersonMovementStabilization=true; config.firstPersonRoomscaleMove=false;
        config.firstPersonBodyFollowsHead=false; collisionMode=0;
        config.firstPersonAnchorZ=anchor;
        physicalPose(0); FirstPersonRecenter(); g_headingBase=heading;
        pos.y_rot=prev.y_rot=Angle(heading); fraction=256; gaitSway={};
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));
        Check(Anchor(camera),"capture standing reference before ledge sequence");
        const auto standingReference=g_groundEye.local;
        float neutral[3]; std::copy(VR().m_firstPersonNeutral,VR().m_firstPersonNeutral+3,neutral);
        for (int interaction:{3,10,30,31,75,82,83,139,19,54,56,57,58,59,60,61,71,80}) {
            state=int16_t(interaction); gaitSway={120,-480,-250};
            Head(0.7f,0.2f);
            VR().m_rawHeadPose.m[0][3]=neutral[0]+0.06f;
            VR().m_rawHeadPose.m[1][3]=neutral[1]-0.04f;
            VR().m_rawHeadPose.m[2][3]=neutral[2]+0.03f;
            const auto beforeRoot=pos;
            for (int f:{0,64,128,192,256}) {
                fraction=f; PHD_VECTOR animated{config.firstPersonAnchorX,config.firstPersonAnchorY,
                    locomotion::FirstPersonAnchorZ(interaction,config.firstPersonAnchorZ,config.firstPersonInteractionAnchorZ)};
                FakeGaitJoint(itemMemory,&animated,14,f);
                Check(Anchor(camera),"interaction remains on native animated head path");
                Check(camera.y_pos==animated.y,"vault/climb height follows native rendered head, not standing height above raised root");
                Check(g_groundEye.valid && Near(g_groundEye.local.x,standingReference.x) &&
                      Near(g_groundEye.local.y,standingReference.y) && Near(g_groundEye.local.z,standingReference.z),
                      "climb/jump/crouch never replaces or erases standing eye reference");
                Check(!std::memcmp(neutral,VR().m_firstPersonNeutral,sizeof(neutral)),
                      "interaction head turns and lean do not silently shift the physical neutral");
                Check(!std::memcmp(&beforeRoot,&pos,sizeof(pos)),"camera and visibility do not move or rotate constrained Lara");
            }
        }
        // Native mount completion advances root onto crate while cached head
        // matrices still contain a high, laterally shifted pull-up frame.
        state=19; Anchor(camera);
        Check(g_mountBodyTransition.active,"pull-up arms-only interval arms body handoff correction");
        prev=pos; pos.x_pos+=96; pos.z_pos+=128; pos.y_pos-=768;
        state=2; gaitSway={240,-1200,300};
        for (int f:{0,32,64,128,192,256}) {
            fraction=f; Check(Anchor(camera),"first standing frame after vault remains valid");
            const float t=f/256.f;
            const auto offset=locomotion::Rotate({standingReference.x,standingReference.z},heading);
            Check(std::fabs(camera.x_pos-(prev.x_pos+96*t+offset.x))<1.1f &&
                  std::fabs(camera.z_pos-(prev.z_pos+128*t+offset.z))<1.1f &&
                  std::fabs(camera.y_pos-(prev.y_pos-768*t+standingReference.y))<1.1f,
                  "post-pull-up camera remains at original root-relative height/centre instead of capturing stale climb offsets");
            PHD_VECTOR animated{config.firstPersonAnchorX,config.firstPersonAnchorY,config.firstPersonAnchorZ};
            FakeGaitJoint(itemMemory,&animated,14,f);
            locomotion::Vec view{},floor{};VR().FirstPersonViewOffset(view.x,view.z);VR().HeadFloorOffset(floor.x,floor.z);
            const auto physical=locomotion::Rotate(view-floor,g_headingBase)*LiveWorldUnitsPerMetre();
            float palette[12]={1,0,0,float(animated.x),0,1,0,float(animated.y),0,0,1,float(animated.z)};
            stabilization::OffsetBodyPalette(palette,1,g_bodyVisualOffset);
            Check(std::fabs(palette[3]-camera.x_pos-physical.x)<1.1f &&
                  std::fabs(palette[11]-camera.z_pos-physical.z)<1.1f,
                  "stale mount skeleton is fitted to final eye, preserving configured torso clearance");
            Check(std::fabs(g_groundEye.local.z-standingReference.z)<.001f,
                  "temporary body correction never becomes a permanent camera offset");

        }
        prev=pos; gaitSway={}; fraction=256; Anchor(camera);
        Check(Near(g_groundEye.local.y,standingReference.y),"settled standing frame retains the same camera height");
        g_runtimeEnabled=false; UpdateSceneCamera(camera);
        Check(!g_groundEye.valid,"FP exit still discards the saved standing reference");
        config.firstPersonBodyFollowsHead=true;
    }
    config.firstPersonAnchorZ=144;
    // Run -> wall impact (AS_SPLAT=12) -> stop. Raised crate floors must
    // remain blocking throughout, even when a query returns no X/Z shift.
    for (int which:{0,1}) for (float heading:{0.f,.8f,-1.9f}) {
        game=dll.game=which; resetRoomscale(); collisionMode=14;
        config.firstPersonMovementStabilization=true;
        g_headingBase=heading;
        impactNormalX=std::sin(heading); impactNormalZ=std::cos(heading);
        Head(0); VR().RecenterFirstPersonHead();
        float neutral[3]; std::copy(VR().m_firstPersonNeutral,VR().m_firstPersonNeutral+3,neutral);
        pos={}; prev={}; pos.y_rot=prev.y_rot=Angle(heading);
        const auto moved=locomotion::Rotate({0,20},heading);
        pos.x_pos=int32_t(std::lround(moved.x)); pos.z_pos=int32_t(std::lround(moved.z));
        const auto savedPos=pos,savedPrev=prev;
        for (int s:{0,1,12,15,3,28,25,26,27,29,9,2}) for (int f:{0,64,128,192,256}) for (float lean:{0.f,.08f}) {
            state=int16_t(s); fraction=f;
            Head(.6f,.2f); VR().m_rawHeadPose.m[2][3]=neutral[2]-lean;
            const auto wanted=locomotion::Rotate({0,200+20*f/256.f},heading);
            PHD_3DPOS p{int32_t(std::lround(wanted.x)),-700,int32_t(std::lround(wanted.z)),0,0,0,0};
            ClampRenderedHeadToCollision(itemMemory,p);
            const auto physical=locomotion::Rotate({0,lean*1000},heading);
            const float rendered=(p.x_pos+physical.x)*impactNormalX+(p.z_pos+physical.z)*impactNormalZ;
            Check(rendered<101.5f,"walk/impact/prepare-jump/jump/fall camera consistently stops before raised crate");
            Check(p.y_pos==-700,"wall-impact correction preserves camera height");
            Check(!std::memcmp(&pos,&savedPos,sizeof(pos)) && !std::memcmp(&prev,&savedPrev,sizeof(prev)),
                  "wall-impact camera never changes Lara's current or interpolated root");
            Check(!std::memcmp(neutral,VR().m_firstPersonNeutral,sizeof(neutral)),
                  "wall-impact correction does not shift tracking neutral");
        }
        collisionMode=0; impactNormalX=0; impactNormalZ=1;
        resetRoomscale();
    }
    // Real-height jump clearance: floor drops remain open; a crate at head
    // height blocks, and clears as soon as both the eye and margin are above.
    for (int which:{0,1}) for (float heading:{0.f,.8f,-1.9f}) {
        game=dll.game=which; resetRoomscale(); collisionMode=15;
        config.firstPersonMovementStabilization=true; g_headingBase=heading;
        impactNormalX=std::sin(heading); impactNormalZ=std::cos(heading);
        Head(0); VR().RecenterFirstPersonHead();
        float neutral[3]; std::copy(VR().m_firstPersonNeutral,VR().m_firstPersonNeutral+3,neutral);
        prev={}; pos={}; pos.y_pos=-1024; pos.y_rot=prev.y_rot=Angle(heading);
        const auto beforeRoot=pos,beforePrev=prev;
        for (int s:{3,9,25,26,27,28,29}) for (int f:{0,16,32,64,128,192,256})
        for (float leanY:{-.10f,0.f,.10f}) for (int gate=0;gate<4;++gate) {
            state=int16_t(s); fraction=f; Head(.6f,.2f);
            config.positionalTracking=gate!=1; config.firstPersonHeadTranslation=gate!=2;
            VR().SetHeadAtCamera(gate==3);
            VR().m_rawHeadPose.m[1][3]=neutral[1]+leanY;
            const auto desired=locomotion::Rotate({0,200},heading);
            PHD_3DPOS p{int32_t(std::lround(desired.x)),-700-4*f,int32_t(std::lround(desired.z)),0,0,0,0};
            const auto before=p;
            ClampRenderedHeadToCollision(itemMemory,p);
            const float eyeY=before.y_pos-(gate ? 0 : leanY*1000);
            if (-768<=eyeY+64)
                Check(p.x_pos*impactNormalX+p.z_pos*impactNormalZ+64<101.5f,
                      "jump/lean eye radius cannot penetrate the raised crate at head height");
            else Check(p.x_pos==before.x_pos && p.z_pos==before.z_pos,
                       "camera crosses crate freely once eye clearance is above its top");
            Check(p.y_pos==before.y_pos && !std::memcmp(&pos,&beforeRoot,sizeof(pos)) &&
                  !std::memcmp(&prev,&beforePrev,sizeof(prev)),"airborne clearance never changes jump animation height or roots");
            Check(!std::memcmp(neutral,VR().m_firstPersonNeutral,sizeof(neutral)),"jump clearance never recenters tracking");
        }
        VR().SetHeadAtCamera(false); config.positionalTracking=config.firstPersonHeadTranslation=true;
        collisionMode=0; impactNormalX=0; impactNormalZ=1; resetRoomscale();
    }
    // Hard stop: keep native animation displacement NONZERO after release.
    // The old release test cleared unstableStep first, masking native coasting.
    for (int which:{0,1}) for (int modern:{0,1}) for (bool smoothing:{false,true}) {
        game=dll.game=which;
        auto& goal=*reinterpret_cast<int16_t*>(itemMemory+off::item_goal_state);
        auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
        const auto setupStop=[&]() {
            resetRoomscale(); Head(0); FirstPersonRecenter(); ResetMovementStabilization();
            config.firstPersonRoomscaleMove=false;
            config.firstPersonMovementStabilization=smoothing;
            *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
            flags=0; goal=2; actionInput=0;
            animationNextState=animationNextGoal=-1; animationStartsGravity=false;
            startAirborne=blockStableMotion=false; collisionPush={};
            unstableStep={3,5}; unstableHeight=1;
            g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAnimate);
            g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAboveWater);
        };
        for (const auto& d:dirs) {
            setupStop(); state=d.z<0 ? 16 : d.x<-.5f ? 22 : d.x>.5f ? 21 : 1;
            float x=d.x,z=d.z,r=0; FirstPersonInput(x,z,r,false);
            actionInput=d.action; Detour_LaraAboveWater(itemMemory,nullptr);
            const auto released=pos;
            x=z=r=0; FirstPersonInput(x,z,r,false); actionInput=0;
            for (int tick=0;tick<60;++tick) {
                const int animationCount=animationTicks, collisionCount=nativeCollisionPasses;
                Detour_LaraAboveWater(itemMemory,nullptr);
                Check(pos.x_pos==released.x_pos && pos.z_pos==released.z_pos &&
                      *reinterpret_cast<int16_t*>(itemMemory+off::item_speed)==0,
                      "release immediately cancels native forward/side/back coasting with smoothing on or off");
                Check(pos.y_pos==released.y_pos+tick+1 && animationTicks==animationCount+1 &&
                      nativeCollisionPasses==collisionCount+1 && !g_rootMotion.valid && !g_hardStopRoot,
                      "hard stop preserves vertical animation, one native animation/collision pass and tick-local state");
            }
            x=d.x; z=d.z; FirstPersonInput(x,z,r,false); actionInput=d.action;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(pos.x_pos!=released.x_pos || pos.z_pos!=released.z_pos,
                  "stick re-engagement resumes movement immediately without a latched brake");
        }
        setupStop();
        float x=.1f,z=0,r=0; FirstPersonInput(x,z,r,false); Detour_LaraAboveWater(itemMemory,nullptr);
        Check(pos.x_pos==0 && pos.z_pos==0,"stick inside existing deadzone hard-stops without a new threshold");
        config.firstPersonRoomscaleMove=true; VR().m_rawHeadPose.m[0][3]+=.2f;
        const auto beforeStep=pos; Detour_LaraAboveWater(itemMemory,nullptr);
        Check(pos.x_pos>beforeStep.x_pos && pos.z_pos==beforeStep.z_pos && g_dragCurrent.x>0,
              "hard stop cancels animation drift but keeps collision-checked physical room-scale steps");
        setupStop(); x=z=r=0; FirstPersonInput(x,z,r,false);
        collisionPush={11,-9}; Detour_LaraAboveWater(itemMemory,nullptr);
        Check(pos.x_pos==11 && pos.z_pos==-9 && beforeNativeCollision.x==0 && beforeNativeCollision.z==0,
              "hard stop occurs before native collision and cannot undo its corrective displacement");
        for (int excluded=0;excluded<16;++excluded) {
            setupStop(); x=z=r=0; FirstPersonInput(x,z,r,false);
            if (excluded==0) g_active=false;
            if (excluded==1) cutseq=1;
            if (excluded==2) water=1;
            if (excluded==3) state=3;
            if (excluded==4) state=10;
            if (excluded==5) state=36;
            if (excluded==6) goal=15;
            if (excluded==7) goal=56;
            if (excluded==8) flags=8;
            if (excluded==9) g_jumpPressed=true;
            if (excluded==10) g_shifted=true;
            if (excluded==11) actionInput=locomotion::Forward;
            if (excluded==12) actionInput=0x10;
            if (excluded==13) actionInput=0x100;
            if (excluded==14) hp=0;
            if (excluded==15) g_haveManualInput=false;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(pos.x_pos==3 && pos.z_pos==5,
                  "hard stop excludes third person, cutscenes, water, airborne/interactions, jump/roll, other input and death");
        }
        for (int transition=0;transition<4;++transition) {
            setupStop(); x=z=r=0; FirstPersonInput(x,z,r,false);
            if (transition==0) animationNextState=3;
            if (transition==1) animationNextGoal=56;
            if (transition==2) animationStartsGravity=true;
            if (transition==3) unstableStep={600,5};
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(pos.x_pos==int32_t(unstableStep.x) && pos.z_pos==5,
                  "animation-triggered airborne/interaction/gravity/teleport movement bypasses braking");
        }
        setupStop(); g_hardStopRoot=true; ResetMovementStabilization();
        Check(!g_hardStopRoot,"camera/session reset clears any pending hard stop");
        goal=0; flags=0; resetRoomscale(); ResetMovementStabilization();
        config.firstPersonMovementStabilization=true;
    }
    // Artificial yaw must advance at view cadence, not controller-poll cadence.
    for (int pollHz:{15,30,60,120,240}) for (int viewHz:{30,60,72,80,90,120,144})
        for (float rate:{.25f,1.5f,-2.f}) {
            stabilization::RenderTurn turn;
            turn.Sample(rate,0);
            int sample=1; double total=0;
            for (int frame=1;frame<=viewHz*3;++frame) {
                const double now=double(frame)/viewHz;
                double next=double(sample)/pollHz+(sample%2 ? .001 : -.001);
                while (next<=now) {
                    turn.Sample(rate,next); ++sample;
                    next=double(sample)/pollHz+(sample%2 ? .001 : -.001);
                }
                const float step=turn.Step(now); total+=step;
                Check(std::fabs(step-rate/viewHz)<.00001,
                      "held stick produces an equal yaw step every view despite slower/faster/jittered controller polls");
                Check(turn.Step(now)==0,"duplicate view timestamp cannot double-integrate turn");
            }
            Check(std::fabs(total-rate*3)<.0001,"turn speed is independent of controller/render cadence");
            turn.Sample(0,3.001);
            Check(turn.Step(3.01)==0 && turn.Step(3.03)==0,"stick release stops immediately without yaw smoothing tail");
            turn.Sample(-rate,3.04);
            Check(std::fabs(turn.Step(3.05)+rate*.01f)<.00001,"reversal uses only time after the new direction was sampled");
            Check(turn.Step(4)==0,"missing input polls cannot leave a stale stick turn latched");
            turn.Sample(rate,10);
            Check(std::fabs(turn.Step(10.01)-rate*.01f)<.00001,"fresh input after a stall has no accumulated catch-up spin");
            turn.Reset(); Check(turn.Step(11)==0,"mode reset discards both turn velocity and clock");
        }
    {
        stabilization::RenderTurn turn;
        turn.Sample(1,1); turn.Sample(1,1.09);
        Check(Near(turn.Step(1.09),.05f),"long render interval caps yaw rather than making a large view jump");
        Check(turn.Step(.5)==0 && !turn.valid,"backward clock fails closed");
        turn.Sample(std::numeric_limits<float>::quiet_NaN(),1);
        Check(!turn.valid,"invalid stick velocity cannot poison heading");
    }
    for (int which:{0,1}) {
        game=dll.game=which; resetRoomscale(); Head(0); FirstPersonRecenter();
        ResetMovementStabilization(); config.firstPersonMovementStabilization=true;
        config.firstPersonRoomscaleMove=false; collisionMode=0; fraction=256; gaitSway={};
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));
        UpdateSceneCamera(camera);
        VR().m_rawHeadPose.m[0][3]+=.12f;
        VR().m_rawHeadPose.m[2][3]-=.08f;
        float x=0,z=0,r=1;
        const float before=g_headingBase;
        for (int poll=0;poll<5;++poll) { r=1; FirstPersonInput(x,z,r,false); }
        Check(g_headingBase==before && r==0,"multiple controller polls latch intent without stepping yaw");
        for (int view=0;view<3;++view) {
            const float previous=g_headingBase;
            g_renderTurn.frameTime=TurnTime()-1.0/90;
            UpdateSceneCamera(camera);
            Check(g_headingBase>previous && camera.y_rot==Angle(g_headingBase) &&
                  g_scenePose.y_rot==camera.y_rot && pos.y_rot>=0,
                  "render frames between input polls advance one heading shared by camera, scene and body following");
            float right,forward; VR().FirstPersonViewOffset(right,forward);
            const auto world=locomotion::Rotate({right,forward},g_headingBase);
            Check(std::fabs(world.x-.12f)<.0001f && std::fabs(world.z-.08f)<.0001f,
                  "each render-rate turn pivots physical lean once without moving its world offset");
            const auto head=InvertRigid(VR().HeadView());
            const auto left=InvertRigid(VR().EyeView(Eye::Left));
            const auto rightEye=InvertRigid(VR().EyeView(Eye::Right));
            for (int axis=0;axis<3;++axis)
                Check(Near((left.r[axis][3]+rightEye.r[axis][3])*.5f,head.r[axis][3]),
                      "render-rate stick turning keeps stereo eyes centred on the same tracked head");
        }
        r=0; FirstPersonInput(x,z,r,false); const float released=g_headingBase;
        g_renderTurn.frameTime=TurnTime()-1.0/90; UpdateSceneCamera(camera);
        Check(g_headingBase==released,"production camera does not coast after stick release");
        r=1; FirstPersonInput(x,z,r,false); cutseq=1; UpdateSceneCamera(camera);
        Check(!g_active && !g_renderTurn.valid,"cutscene suspension clears pending artificial yaw");
        cutseq=0; UpdateSceneCamera(camera); const float resumed=g_headingBase;
        g_renderTurn.frameTime=TurnTime()-1.0/60; UpdateSceneCamera(camera);
        Check(g_headingBase==resumed,"return from cutscene cannot replay old turn input");
        r=1; FirstPersonInput(x,z,r,false); FirstPersonRecenter();
        Check(!g_renderTurn.valid,"recenter preserves heading but drops pending turn timing");
        resetRoomscale(); ResetMovementStabilization();
    }
    // World stability: vary HMD orientation and real translation independently.
    // Test both render-only and native room-scale simulation/interpolation;
    // synthetic neck compensation may move the BODY, never the rendered world.
    for (int which:{0,1}) for (bool moveBody:{false,true}) {
        game=dll.game=which; resetRoomscale(); collisionMode=0;
        config.firstPersonMovementStabilization=true;
        physicalPose(0); Head(0); FirstPersonRecenter();
        ResetMovementStabilization();
        unstableStep={}; unstableHeight=0; startAirborne=blockStableMotion=false;
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeUnstableAboveWater);
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));
        gaitSway={}; fraction=256; Anchor(camera);
        const auto baseEye=camera;
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        for (int hand=0;hand<2;++hand) {
            VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[hand].m[0][3]+=hand ? 0.2f : -0.2f;
        }
        for (int tick=0;tick<=72;++tick) {
            const float yaw=tick*kPi/18, pitch=0.35f*std::sin(yaw);
            const float leanX=tick>36 ? 0.08f*std::sin(yaw) : 0;
            const float leanY=tick>36 ? 0.04f*std::cos(yaw) : 0;
            const float leanZ=tick>36 ? -0.05f*std::cos(yaw) : 0;
            Head(yaw,pitch);
            VR().m_rawHeadPose.m[0][3]+=leanX;
            VR().m_rawHeadPose.m[1][3]+=leanY;
            VR().m_rawHeadPose.m[2][3]+=leanZ;
            VR().m_headFromTracking=InvertRigid(FromHmd(VR().m_rawHeadPose));
            prev=pos;
            if (moveBody) Detour_LaraAboveWater(itemMemory,nullptr);
            for (int f:{0,64,128,192,256}) {
                fraction=f; Anchor(camera);
                const auto eye=InvertRigid(VR().HeadView());
                if (!(std::fabs(camera.x_pos+eye.r[0][3]-baseEye.x_pos-leanX*1000)<1.1f &&
                      std::fabs(camera.y_pos+eye.r[1][3]-baseEye.y_pos+leanY*1000)<1.1f &&
                      std::fabs(camera.z_pos-eye.r[2][3]-baseEye.z_pos+leanZ*1000)<1.1f))
                    std::printf("stability fixture: game=%d move=%d tick=%d frac=%d eye=(%.2f,%.2f,%.2f) expected=(%.2f,%.2f,%.2f) flipY=%d\n",
                        which,int(moveBody),tick,f,camera.x_pos+eye.r[0][3],camera.y_pos+eye.r[1][3],camera.z_pos-eye.r[2][3],
                        baseEye.x_pos+leanX*1000,baseEye.y_pos-leanY*1000,baseEye.z_pos-leanZ*1000,int(config.flipViewY));
                Check(std::fabs(camera.x_pos+eye.r[0][3]-baseEye.x_pos-leanX*1000)<1.1f &&
                      std::fabs(camera.y_pos+eye.r[1][3]-baseEye.y_pos+leanY*1000)<1.1f &&
                      std::fabs(camera.z_pos-eye.r[2][3]-baseEye.z_pos+leanZ*1000)<1.1f,
                      "world eye receives only real HMD translation, no yaw/pitch orbit or room-scale double motion");
                const auto leftEye=InvertRigid(VR().EyeView(Eye::Left));
                const auto rightEye=InvertRigid(VR().EyeView(Eye::Right));
                for (int axis=0;axis<3;++axis)
                    Check(Near((leftEye.r[axis][3]+rightEye.r[axis][3])*0.5f,eye.r[axis][3]),
                          "both stereo eyes share the same world-stable head centre");
                for (int hand=0;hand<2;++hand) {
                    float right,down,forward;
                    Check(VR().FirstPersonControllerOffset(hand,right,down,forward),"stable controller offset available");
                    Check(std::fabs(camera.x_pos+right*1000-baseEye.x_pos-(hand ? 200 : -200))<1.1f &&
                          std::fabs(camera.z_pos+forward*1000-baseEye.z_pos)<1.1f,
                          "stationary controllers cannot drift when only the head turns or leans");
                }
            }
        }
        config.firstPersonRoomscaleMove=false;
        collisionMode=11; Head(0); VR().m_rawHeadPose.m[0][3]+=0.4f;
        VR().m_headFromTracking=InvertRigid(FromHmd(VR().m_rawHeadPose));
        Anchor(camera);
        const auto clamped=InvertRigid(VR().HeadView());
        Check(camera.x_pos+clamped.r[0][3]<100,
              "wall clearance clamps the same raw rendered eye used by stereo, including physical lean");
        const auto native=VR().m_headFromTracking; g_active=false;
        const auto third=VR().TrackedHeadView();
        Check(std::memcmp(&native,&third,sizeof(native))==0,"third-person head tracking is unchanged");
    }

    // Native B-roll reversal: one animation command turns Lara; VR and pending
    // movement follow it once, preserving physical head movement and calibration.
    for (int which:{0,1}) for (bool stable:{false,true}) for (int degrees:{-170,0,90,170}) {
        game=dll.game=which;resetRoomscale();
        config.firstPersonMovementStabilization=stable;
        config.firstPersonBodyFollowsHead=false;config.firstPersonRoomscaleMove=false;
        config.firstPersonDriftLog=false;collisionMode=0;gaitSway={};
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeRollAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeAboveWater);
        pos.y_rot=Angle(degrees*kPi/180);prev=pos;fraction=256;
        physicalPose(.4f);FirstPersonRecenter();UpdateSceneCamera(camera);
        physicalPose(.7f,.025f,.015f,-.05f);
        float x=0,z=1,r=0;FirstPersonInput(x,z,r,false);
        const float initialBase=g_headingBase;
        const auto calibration=g_groundEye;
        for (int reversal=0;reversal<2;++reversal) {
            state=45;rollNextState=23;rollYawDelta=32768;
            const auto before=pos;const float oldBase=g_headingBase;
            const auto oldMove=g_manualWorld;
            locomotion::Vec oldOffset;VR().FirstPersonViewOffset(oldOffset.x,oldOffset.z);
            oldOffset=locomotion::Rotate(oldOffset,oldBase);
            const float neutralY=VR().m_firstPersonNeutral[1];
            g_renderTurn.Sample(.8f,TurnTime());g_rootMotion.valid=true;
            const int ticks=animationTicks;
            Detour_AnimateLara(itemMemory);
            Check(animationTicks==ticks+1 && pos.x_pos==before.x_pos+3 &&
                pos.y_pos==before.y_pos+7 && pos.z_pos==before.z_pos+5,
                "roll retains exactly one native animation update and root displacement");
            Check(uint16_t(int(pos.y_rot)-int(before.y_rot))==32768 &&
                std::fabs(std::fabs(Wrap(g_headingBase-oldBase))-kPi)<.0001f,
                "native Lara half-turn rotates VR frame once");
            Check(Near(g_manualWorld.x,-oldMove.x) && Near(g_manualWorld.z,-oldMove.z),
                "pending forward movement rotates to new facing immediately");
            Check(analogMemory[2]==Angle(g_lastHeadWorld) && analogMemory[3]==analogMemory[2] &&
                analogMemory[4]==45 && analogMemory[5]==pos.y_rot,
                "VR analog heading updates without erasing native roll transition");
            locomotion::Vec offset;VR().FirstPersonViewOffset(offset.x,offset.z);
            offset=locomotion::Rotate(offset,g_headingBase);
            Check(Near(offset.x,oldOffset.x) && Near(offset.z,oldOffset.z) &&
                VR().m_firstPersonNeutral[1]==neutralY,"roll pivot preserves world lean and duck calibration");
            Check(!g_rootMotion.valid && !g_renderTurn.valid,"roll clears stale gait and turn integration");
            const float newBase=g_headingBase;
            gaitSway={90,350,-300}; // Native roll drops the head halfway toward the floor.
            for (fraction=0;fraction<=256;fraction+=32) {
                UpdateSceneCamera(camera);
                const float rootY=prev.y_pos+(pos.y_pos-prev.y_pos)*(fraction/256.f);
                Check(Near(g_headingBase,newBase) && camera.x_rot==0 && camera.z_rot==0,
                    "repeated/interpolated views never reapply turn or somersault camera");
                Check(std::fabs(camera.y_pos-(rootY-350))<1.1f,
                    "ground roll camera follows the animated drop while above the floor");
                const auto flat=locomotion::Rotate(stable ? locomotion::Vec{calibration.local.x,calibration.local.z} :
                    locomotion::Vec{float(config.firstPersonAnchorX),float(config.firstPersonAnchorZ)},newBase);
                Check(std::fabs(camera.x_pos-(prev.x_pos+(pos.x_pos-prev.x_pos)*(fraction/256.f)+flat.x))<1.1f &&
                    std::fabs(camera.z_pos-(prev.z_pos+(pos.z_pos-prev.z_pos)*(fraction/256.f)+flat.z))<1.1f,
                    "camera forward anchor follows reversed heading at every render fraction");
            }
            fraction=256;gaitSway={};prev=pos;state=2;rollNextState=2;rollYawDelta=0;
            UpdateSceneCamera(camera);
            actionInput=0;analogMemory[0]=0;analogMemory[1]=20000;
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(seenYaw==Angle(g_lastHeadWorld) && seenAnalog[2]==seenYaw && (seenInput&locomotion::Forward),
                "held forward after roll uses new head/body direction without another input poll");
        }
        Check(std::fabs(Wrap(g_headingBase-initialBase))<.0001f,"two rolls restore original VR forward direction");
    }
    // Midjump B uses native flip clips, whose half-turn must also turn the VR
    // frame. Preserve native displacement/gravity and apply it once per flip.
    for (int which:{0,1}) for (bool modern:{false,true})
    for (int clip:{207,210,212}) for (int degrees:{-170,0,170}) {
        game=dll.game=which;resetRoomscale();collisionMode=0;gaitSway={};
        config.firstPersonBodyFollowsHead=true;config.firstPersonRoomscaleMove=false;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeRollAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeAboveWater);
        pos.y_rot=Angle(degrees*kPi/180);prev=pos;fraction=256;
        physicalPose(.3f);FirstPersonRecenter();UpdateSceneCamera(camera);
        physicalPose(.5f,.02f,.01f,-.03f);
        float x=0,z=1,r=0;FirstPersonInput(x,z,r,false);
        state=clip==212 ? 25 : 3;
        actionInput=0x100 | locomotion::Forward;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check((seenInput&0x100)!=0,"first-person jump input preserves native B/roll action");
        auto& animation=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
        animation=int16_t(clip);rollNextState=state;rollNextAnimation=clip;rollYawDelta=32768;
        auto& flags=*reinterpret_cast<uint32_t*>(itemMemory+0x1820);
        const auto savedFlags=flags;flags|=8;
        const auto before=pos;const float oldBase=g_headingBase;
        const auto oldMove=g_manualWorld;const float neutralY=VR().m_firstPersonNeutral[1];
        locomotion::Vec lean;VR().FirstPersonViewOffset(lean.x,lean.z);
        lean=locomotion::Rotate(lean,oldBase);
        const int ticks=animationTicks;
        Detour_AnimateLara(itemMemory);
        Check(animationTicks==ticks+1 && pos.x_pos==before.x_pos+3 &&
              pos.y_pos==before.y_pos+7 && pos.z_pos==before.z_pos+5 && (flags&8),
              "midair flip preserves native animation, trajectory and gravity");
        Check(std::fabs(std::fabs(Wrap(g_headingBase-oldBase))-kPi)<.0001f &&
              uint16_t(int(pos.y_rot)-int(before.y_rot))==32768,
              "forward/back jump flip rotates Lara and first-person heading together");
        Check(Near(g_manualWorld.x,-oldMove.x) && Near(g_manualWorld.z,-oldMove.z) &&
              analogMemory[2]==Angle(g_lastHeadWorld) && analogMemory[3]==analogMemory[2],
              "midair reversal updates pending movement and analog heading");
        locomotion::Vec newLean;VR().FirstPersonViewOffset(newLean.x,newLean.z);
        newLean=locomotion::Rotate(newLean,g_headingBase);
        Check(Near(lean.x,newLean.x) && Near(lean.z,newLean.z) &&
              VR().m_firstPersonNeutral[1]==neutralY,"jump reversal preserves physical lean and height calibration");
        const float turned=g_headingBase;
        rollYawDelta=0;
        Detour_AnimateLara(itemMemory);
        for (fraction=0;fraction<=256;fraction+=64) {
            UpdateSceneCamera(camera);
            Check(Near(g_headingBase,turned) && camera.x_rot==0 && camera.z_rot==0,
                  "held B and repeated views cannot repeat the half-turn or flip the camera upside down");
        }
        // Transition out of the flip, then land: body-follow keeps the new view.
        rollNextState=clip==212 ? 3 : 25;rollNextAnimation=clip==207 ? 209 : clip+1;
        Detour_AnimateLara(itemMemory);
        Check(Near(g_headingBase,turned),"leaving the flip animation cannot apply a second turn");
        flags=savedFlags;state=2;animation=11;fraction=256;
        UpdateSceneCamera(camera);TurnBodyToHead(itemMemory,1.f);
        Check(Near(g_headingBase,turned) &&
              std::fabs(Wrap(Radians(pos.y_rot)-g_lastHeadWorld))<.001f,
              "landing body-follow faces the reversed view instead of undoing the flip");
        rollNextAnimation=-1;
    }
    for (int stateValue:{3,25}) for (int clip:{0,77,208,209,211,213})
        Check(locomotion::NativeRollTurn(stateValue,stateValue,clip,clip,0,int16_t(-32768))==0,
              "ordinary airborne turns outside flip animations do not rotate the VR frame");

    // Clamp the final rendered headset centre against the actual sampled floor,
    // including deep physical ducking, slopes, low ceilings, and entry mid-roll.
    for (int which:{0,1}) for (bool calibrated:{false,true}) for (int mode:{0,16,17,18}) {
        game=dll.game=which;resetRoomscale();config.firstPersonMovementStabilization=true;
        config.firstPersonBodyFollowsHead=false;config.firstPersonRoomscaleMove=false;
        collisionMode=0;gaitSway={};physicalPose(0);FirstPersonRecenter();
        fraction=256;UpdateSceneCamera(camera);
        if (!calibrated) g_groundEye.Reset();
        const auto savedEye=g_groundEye;
        state=45;gaitSway={120,1300,-350};collisionMode=mode;
        physicalPose(0,.08f,.01f,-1.2f);
        const float neutralY=VR().m_firstPersonNeutral[1];
        const auto bodyBefore=pos;cameraCollisionCalls=0;
        UpdateSceneCamera(camera);
        const float eyeY=camera.y_pos+VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre();
        const auto tracked=InvertRigid(VR().HeadView());
        const float eyeX=camera.x_pos+tracked.r[0][3];
        const float floor=mode==0 ? 0.f : float(int(eyeX)/4);
        if (mode!=17) Check(eyeY<=floor-63 && eyeY>=(mode==18?-600.f:-1786.f)+63,
            "deep duck during roll stays clear of floor and ceiling at final eye position");
        else Check(std::abs(camera.y_pos-600)<=1,"missing floor sample preserves animated height without inventing a floor");
        Check(cameraCollisionCalls>0 && !std::memcmp(&pos,&bodyBefore,sizeof(pos)),
            "roll clearance queries world without moving Lara's collision root");
        Check(g_groundEye.valid==savedEye.valid && !std::memcmp(&g_groundEye.local,&savedEye.local,sizeof(savedEye.local)) &&
            VR().m_firstPersonNeutral[1]==neutralY,"roll collision correction cannot contaminate standing or HMD calibration");
        physicalPose(0);gaitSway={};collisionMode=0;state=2;UpdateSceneCamera(camera);
        Check(camera.y_pos==-700,"leaving roll restores normal standing eye without residual pushback");
    }
    // A complete down/up roll height profile follows the interpolated head
    // until floor clearance intervenes, then releases immediately on ascent.
    for (int which:{0,1}) for (bool stable:{false,true}) for (bool tracked:{false,true}) {
        game=dll.game=which;resetRoomscale();config.firstPersonMovementStabilization=stable;
        config.firstPersonBodyFollowsHead=false;config.firstPersonRoomscaleMove=false;
        config.firstPersonHeadTranslation=tracked;collisionMode=0;gaitSway={};
        physicalPose(0);FirstPersonRecenter();fraction=256;UpdateSceneCamera(camera);
        const auto savedEye=g_groundEye;
        physicalPose(0,0,0,-.15f);
        const float neutralY=VR().m_firstPersonNeutral[1];
        for (int rollState:{45,23}) {
            state=int16_t(rollState);
            for (int animatedY:{-700,-500,-300,-180,-80,0,100,0,-80,-180,-300,-500,-700}) {
                gaitSway={0,animatedY+700,0};UpdateSceneCamera(camera);
                const float offset=tracked ? VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre() : 0;
                const float expected=std::min(float(animatedY)+offset,-64.f);
                Check(std::fabs(camera.y_pos+offset-expected)<1.1f,
                    "roll descends and rises with native head, clamping only at floor clearance");
                Check(g_groundEye.valid==savedEye.valid && VR().m_firstPersonNeutral[1]==neutralY,
                    "animated roll drop never recaptures standing or physical height calibration");
            }
        }
        state=2;gaitSway={};physicalPose(0);UpdateSceneCamera(camera);
        Check(camera.y_pos==-700,"roll ascent leaves no floor-clamp offset behind");
    }
    // Mount correction follows stale interpolation, then releases by elapsed
    // time without changing ordinary running after the handoff has finished.
    for (int hz:{30,60,90,144}) {
        stabilization::MountBodyTransition transition;
        transition.Begin();
        const locomotion::Vec residual{200,-120};
        for (int frame=0;frame<hz;++frame) {
            const auto fix=transition.Correction(residual,-1600,frame/double(hz));
            Check(Near(fix.x,residual.x) && Near(fix.z,residual.z),
                  "unfinished climb pose keeps full correction independent of frame count");
        }
        for (int frame=0;frame<=hz;++frame) {
            const double elapsed=frame/double(hz);
            const auto fix=transition.Correction(residual,-700,1+elapsed);
            const float weight=float(std::max(0.0,1-elapsed/.12));
            Check(std::fabs(fix.x-residual.x*weight)<.002f && std::fabs(fix.z-residual.z*weight)<.002f,
                  "standing handoff releases smoothly with equal timing at every render rate");
        }
        Check(!transition.active,"mount fit cannot remain latched during normal running");
        transition.Begin();transition.Reset();
        Check(locomotion::Length(transition.Correction(residual,-1600,3))==0,
              "camera/session reset discards stale mount correction");
    }
    // Native hard landing is STOP + animation 24, not a dedicated state.
    // Use the production camera path, including floor clearance and calibration.
    for (int which:{0,1}) for (bool stable:{false,true}) for (bool calibrated:{false,true})
    for (float heading:{0.f,.7f,-1.9f}) for (float duck:{0.f,-.2f}) {
        game=dll.game=which;resetRoomscale();ResetMovementStabilization();
        config.firstPersonMovementStabilization=stable;config.firstPersonHeadTranslation=true;
        config.firstPersonBodyFollowsHead=false;config.firstPersonRoomscaleMove=false;
        collisionMode=0;gaitSway={};physicalPose(0);FirstPersonRecenter();
        dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));
        pos.y_rot=prev.y_rot=Angle(heading);fraction=256;
        auto& animation=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
        animation=0;UpdateSceneCamera(camera);
        if (!calibrated) g_groundEye.Reset();
        const auto saved=g_groundEye;
        physicalPose(0,0,0,duck);const float neutral=VR().m_firstPersonNeutral[1];
        state=2;animation=24;
        Check(UseHardLandingCamera(itemMemory),"TR4/5 live hard landing recognized by state and animation");
        for (int height:{-700,-600,-400,-200,-80,50,-80,-200,-400,-600,-700}) {
            gaitSway={30,height+700,-20};
            for (int f:{0,64,128,192,256}) {
                fraction=f;UpdateSceneCamera(camera);
                const float tracking=VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre();
                const float expected=std::min(float(height)+tracking,-64.f);
                Check(std::fabs(camera.y_pos+tracking-expected)<1.1f,
                      "impact kneel and recovery follow animated neck until final tracked eye reaches floor margin");
                Check(g_groundEye.valid==saved.valid && g_groundEye.local.x==saved.local.x &&
                      g_groundEye.local.y==saved.local.y && g_groundEye.local.z==saved.local.z,
                      "landing never captures kneeling height or replaces standing calibration");
                if (stable && calibrated && height+tracking < -65) {
                    const auto flat=locomotion::Rotate({saved.local.x,saved.local.z},g_headingBase);
                    Check(std::fabs(camera.x_pos-flat.x)<1.1f && std::fabs(camera.z_pos-flat.z)<1.1f,
                          "landing preserves stable horizontal anchor despite animated sway");
                }
                Check(VR().m_firstPersonNeutral[1]==neutral && pos.y_pos==0 && camera.x_rot==0 && camera.z_rot==0,
                      "landing preserves HMD neutral, collision root and head-controlled rotation");
            }
        }
        animation=0;gaitSway={};physicalPose(0);fraction=256;UpdateSceneCamera(camera);
        Check(camera.y_pos==-700 && (!stable || g_groundEye.valid),
              "settled standing restores normal height even after entering FP mid-kneel");
        for (int other:{0,11,27,42,50,51,103}) {
            animation=int16_t(other);gaitSway={0,200,0};UpdateSceneCamera(camera);
            Check(!UseHardLandingCamera(itemMemory) && (!stable || camera.y_pos==-700),
                  "idle, ordinary landing and vault animations keep existing standing stabilization");
        }
        state=2;animation=24;hp=0;
        const PHD_3DPOS native{400,500,600,20,30,40,0};camera=native;UpdateSceneCamera(camera);
        Check(!g_active && !std::memcmp(&camera,&native,sizeof(native)),
              "fatal landing still uses the native death camera");
        hp=1000;animation=0;gaitSway={};
    }
    for (int other:{1,3,8,9,19,23,28,45,54})
        Check(!locomotion::IsHardLanding(other,24),"landing exception does not include unrelated states");
    // Native turns outside an eligible FP ground/jump roll must stay native.
    for (int clip:{37,207,212}) for (int guard=0;guard<11;++guard) {
        resetRoomscale();config.firstPersonMovementStabilization=true;physicalPose(0);
        FirstPersonRecenter();UpdateSceneCamera(camera);state=45;rollNextState=23;rollYawDelta=32768;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=int16_t(clip);
        if (clip!=37) state=rollNextState=clip==207 ? 3 : 25;
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeRollAnimate);
        if (guard==0) g_active=false;
        if (guard==1) g_runtimeEnabled=false;
        if (guard==2) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=1;
        if (guard==3) *reinterpret_cast<int32_t*>(cameraMemory+off::camera_type)=kCamFixed;
        if (guard==4) cutseq=1;
        if (guard==5) hp=0;
        if (guard==6) water=1;
        if (guard==7) state=rollNextState=66;
        if (guard==8) state=rollNextState=72;
        if (guard==9) state=rollNextState=2;
        if (guard==10) rollYawDelta=100;
        const float base=g_headingBase;const auto before=pos;const int ticks=animationTicks;
        Detour_AnimateLara(itemMemory);
        Check(g_headingBase==base && animationTicks==ticks+1 &&
            uint16_t(int(pos.y_rot)-int(before.y_rot))==uint16_t(rollYawDelta),
            "third person, camera takeovers, death, water and unrelated turns remain native");
    }
    VR().m_system = nullptr;
    std::printf("OK: %d first-person regression checks passed.\n", checks);
    return 0;
}
