// Compile with MSVC /std:c++17 /O2 /Gy /EHsc and /link /OPT:REF.
// Exercises the actual implementation with engine memory and native calls
// stubbed. No game process, OpenVR runtime, or live code patching is involved.
// From a VS x64 developer prompt at the repo root:
// cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX
//    /Ithird_party\openvr\headers /Isrc tools\first_person_tests.cpp
//    /Febuild\first_person_tests.exe /Fobuild\first_person_tests.obj /link /OPT:REF user32.lib
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
bool worldCameraValid = false;
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
void __cdecl FakeAim(void*, uint8_t* arm) {
    *reinterpret_cast<int16_t*>(arm + 12) = 0;
    *reinterpret_cast<int16_t*>(arm + 14) = 123;
    *reinterpret_cast<int16_t*>(arm + 16) = 456;
}
void __cdecl FakeAnimate(uint8_t* item) {
    ++animationTicks;
    auto& pos = *reinterpret_cast<tr::PHD_3DPOS*>(item + tr::off::item_pos);
    pos.x_pos += 3; pos.y_pos += 7; pos.z_pos += 5;
    *reinterpret_cast<int16_t*>(item + tr::off::item_speed) = 10;
}
tr::locomotion::Vec unstableStep{};
int unstableHeight=0, nativeCollisionPasses=0;
bool blockStableMotion=false, startAirborne=false;
tr::locomotion::Vec beforeNativeCollision{};
tr::PHD_VECTOR gaitSway{};
void __cdecl FakeUnstableAnimate(uint8_t* item) {
    ++animationTicks;
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    p.x_pos+=int32_t(unstableStep.x); p.z_pos+=int32_t(unstableStep.z);
    p.y_pos+=unstableHeight;
    *reinterpret_cast<int16_t*>(item+tr::off::item_speed)=int16_t(tr::locomotion::Length(unstableStep));
    if (startAirborne) *reinterpret_cast<int16_t*>(item+tr::off::item_anim_state)=3;
}
void __cdecl FakeUnstableAboveWater(uint8_t* item,void*) {
    ++simulationTicks;
    auto& p=*reinterpret_cast<tr::PHD_3DPOS*>(item+tr::off::item_pos);
    const auto previous=p;
    tr::Detour_AnimateLara(item);
    beforeNativeCollision={float(p.x_pos-previous.x_pos),float(p.z_pos-previous.z_pos)};
    ++nativeCollisionPasses;
    if (blockStableMotion) { p.x_pos=previous.x_pos; p.z_pos=previous.z_pos; }
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
              c->badCeiling == 0 && c->flags == 5 && height == 762,
              "camera uses native ground or airborne collision parameters");
        Check(std::hypot(float(x - c->old[0]), float(z - c->old[2])) <= 16+std::sqrt(2.f),
              "rendered eye sweep advances at most 16 units plus two-axis integer rounding");
        Check(room == *reinterpret_cast<int16_t*>(itemMemory + tr::off::item_room),
              "camera sweep starts in Lara's room");
        c->floorSamples[0] = collisionMode == 12 ? 1000 : 0;
        if (collisionMode==14 && x*impactNormalX+z*impactNormalZ>=100)
            c->floorSamples[0]=-768; // raised crate, no synthetic shift needed
        c->floorSamples[1] = -1024;
        if (collisionMode == 11 && x >= 100) c->shift[0] = 99 - x;
        if (collisionMode == 13) c->type = 8;
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
bool IsOpticsZoomed() { return false; }
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
    LARGE_INTEGER freq{}; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&g_inputTime);
    g_inputTime.QuadPart -= freq.QuadPart / 60;
    float lx = 0, ly = 0, rx = 1;
    FirstPersonInput(lx, ly, rx, false);
    Check(rx == 0 && g_headingBase > 0, "stick updates FP heading and consumes native camera orbit");
    TurnBodyToHead(itemMemory, 1.0f / 60);
    Check(pos.y_rot > 0, "body follows stick heading");

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
                g_gunTriggers.Update(true,true,false,false,2);
                const int before=nativeShotCalls;
                Check(fire(rightCaller)==0,"LT cannot fire the right pistol/Uzi or Desert Eagle");
                if (dual) {
                    Check(fire(leftCaller)==-1 && nativeShotHand==0 && nativeShotCalls==before+1,
                          "LT release fires native left pistol/Uzi from left controller");
                    Check(fire(leftCaller)==0,"native left call cannot repeat a consumed tap");
                }
                g_gunTriggers.pending[0]=false;
                g_gunTriggers.Update(true,true,false,true,3);
                const int beforeRight=nativeShotCalls;
                if (dual) Check(fire(leftCaller)==0,"RT cannot fire left pistol/Uzi");
                Check(fire(rightCaller)==-1 && nativeShotHand==1 && nativeShotCalls==beforeRight+1,
                      "RT fires right pistol/Uzi or shared revolver branch from right controller");
                Check(!g_gunTriggers.WantsShot() && fire(rightCaller)==0,
                      "one RT request is consumed exactly once through native caller routing");
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
        Check(rt==0 && !g_gunTriggers.WantsShot(),"holding ready RT cannot turn into repeated native fire");
        status=1; lt=0; rt=255; UpdateGunTriggers(lt,rt,true,30);
        Check(rt==255 && !g_gunTriggers.WantsShot(),"ready-to-hands-busy transition immediately restores held grab");
        status=4; lt=0; rt=255; UpdateGunTriggers(lt,rt,true,40);
        Check(rt==0 && !g_gunTriggers.WantsShot(),"held grab cannot become a shot when guns become ready");
        lt=0; rt=0; UpdateGunTriggers(lt,rt,true,50);
        lt=255; rt=0; UpdateGunTriggers(lt,rt,true,60);
        lt=0; rt=0; UpdateGunTriggers(lt,rt,true,70);
        Check(rt==255 && g_gunTriggers.pending[0] && !g_gunTriggers.pending[1],
              "left-only tap still sends native firing request exclusively for left gun");
        for (uint8_t holdMode:{uint8_t(0),uint8_t(1)}) {
            appMemory[0x9ec]=holdMode; appMemory[0x9e4]=1; status=0;
            g_gunTriggers.Reset(); g_gunEquip.Reset();
            lt=rt=0; UpdateGunTriggers(lt,rt,true,0);
            lt=255; rt=200; UpdateGunTriggers(lt,rt,true,10);
            lt=255; rt=200; UpdateGunTriggers(lt,rt,true,1010);
            Check(lt==255 && rt==200,"1.0-second equip gesture coexists with native held RT in either draw style");
        }
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
    struct Direction { float x, z; uint64_t action; float offset; int scale; };
    const Direction dirs[] = {
        {0, 1, locomotion::Forward, 0, 1},
        {-1, 0, locomotion::StepLeft, -kPi/2, 3},
        {1, 0, locomotion::StepRight, kPi/2, 3},
        {0, -1, locomotion::Back | locomotion::Walk, kPi, 3},
        {0.2f, 1, locomotion::Forward, 0, 1}
    };
    for (int modern : {0, 1}) for (float cam : {-2.4f, 0.0f, 2.1f}) for (const auto& d : dirs) {
        *reinterpret_cast<int32_t*>(appMemory + drva::app_off::cfgFlags) = modern ? 2 : 0;
        state = 2; pos = {}; analogMemory[2] = Angle(cam);
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
        Check(*reinterpret_cast<int16_t*>(itemMemory + off::item_speed) == 10*d.scale && g_directionalRootScale == 1, "speed scaled for collision and scope restored");
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
    QueryPerformanceCounter(&g_inputTime);
    g_inputTime.QuadPart -= freq.QuadPart / 60;
    lx = ly = 0; rx = 1;
    FirstPersonInput(lx, ly, rx, false);
    const float turn = Wrap(g_headingBase - 0.3f);
    Check(turn > 0 && rx == 0, "real input applies artificial turn");
    checkViews(std::cos(turn) * 0.08f - std::sin(turn) * 0.12f,
               std::sin(turn) * 0.08f + std::cos(turn) * 0.12f, 0);
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
        // Reproduce normal engine initialization per game, then exercise the
        // actual input -> simulation -> animation path for every ground gait.
        // Previously these tests forced TR4's -1 sentinel even for TR5.
        for (int modern : {0, 1}) for (const auto& d : dirs) {
            resetRoomscale(); config.firstPersonRoomscaleMove = false;
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
            QueryPerformanceCounter(&g_inputTime); g_inputTime.QuadPart -= freq.QuadPart / 60;
            x=z=0; r=1; FirstPersonInput(x, z, r, false);
            Check(r == 0 && g_headingBase > oldBase,
                  "stick turn consumes native camera orbit in both games");
            TurnBodyToHead(itemMemory, 1.0f / 60);
            Check(Radians(pos.y_rot) > 1.0f, "body follows stick turn in both games");
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
        Check(camera.z_pos < 100, "ground floor discontinuity blocks camera sweep");
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
            QueryPerformanceCounter(&g_inputTime); g_inputTime.QuadPart -= freq.QuadPart / 60;
            FirstPersonInput(x, z, r, false);
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
            g_gunEquip.initialized=true;
            UpdateSceneCamera(camera);
            Check(!g_active && !g_haveHeading && !g_scenePoseValid,
                  "UI/cutscene transitions/flyby/tutorial/fixed camera suspend FP");
            Check(g_runtimeEnabled && !std::memcmp(&camera,&nativeCamera,sizeof(camera)),
                  "cutscene uses untouched native camera without toggling FP preference");
            Check(cutsceneBits==0x7fff && !g_headHidden && !g_rollHidden,
                  "native Lara head/body visibility restored during cutscene");
            Check(!g_gunTriggers.active && !g_gunTriggers.WantsShot() && !g_gunEquip.initialized,
                  "cutscene cancels queued hand shots and equip gestures");
            const int beforeDraw=draws,beforeHair=hairs;
            Detour_DrawCreatureHD(itemMemory,0,0); Detour_DrawHair(0);
            Check(draws==beforeDraw+1 && hairs==beforeHair+1,
                  "cutscene body and hair use native rendering");
            std::memset(appMemory, 0, sizeof(appMemory)); cutseq = 0;
            cutseqNumber=cutseqTransition=spotCamera=0; vonCroyScene=0;
            *reinterpret_cast<int32_t*>(cameraMemory + off::camera_type) = 0;
            physicalPose(-0.4f); pos.y_rot = Angle(-1.2f);
            UpdateSceneCamera(camera);
            Check(g_active && Near(g_lastHeadWorld, savedHeading), "resuming FP preserves viewing heading");
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
        for (bool hideHead:{false,true}) for (int hanging:{10,30,31,75,82,83,139}) {
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
            Check(!g_ledgeArmsOnly && bits==(hideHead ? baseBits&~kHeadMeshBit : baseBits),
                  "pull-up exits the arms-only mask without losing the standing body snapshot");
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
        object=oldObject; dll.objects=oldObjects; dll.gLaraHeads=oldHeads;
        RestoreHeadMesh(); bits=baseBits;
        state = 45; UpdateSceneCamera(camera); FirstPersonToggle();
        Check(bits == baseBits && !g_rollHidden && !g_haveHeading && !g_headingItem,
              "toggle during roll restores mesh and clears heading identity");
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
    // Enabled stabilization: test the production animation/simulation hooks,
    // not only a camera math helper, and retain post-animation collision.
    for (int which:{0,1}) {
        game=dll.game=which;
        for (const auto& d:dirs) {
            resetRoomscale(); physicalPose(0); FirstPersonRecenter();
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
        Check(Anchor(camera) && camera.x_pos==pos.x_pos+10 && camera.y_pos==pos.y_pos-680,
              "teleport recaptures the eye reference instead of retaining stale camera offsets");
        state=2; g_runtimeEnabled=false; UpdateSceneCamera(camera);
        Check(!g_groundEye.valid && !g_rootMotion.valid,"leaving first person clears stabilization state");
    }
    // Stand -> jump/hang -> vault/pull-up -> stand. The last climb skeleton
    // can coexist with a newly relocated standing root during interpolation.
    // It must never replace the persistent standing reference (sky/side drift).
    for (int which:{0,1}) for (float heading:{0.f,0.8f,-1.9f}) {
        game=dll.game=which; resetRoomscale(); ResetMovementStabilization();
        config.firstPersonMovementStabilization=true; config.firstPersonRoomscaleMove=false;
        config.firstPersonBodyFollowsHead=false; collisionMode=0;
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
        }
        prev=pos; gaitSway={}; fraction=256; Anchor(camera);
        Check(Near(g_groundEye.local.y,standingReference.y),"settled standing frame retains the same camera height");
        g_runtimeEnabled=false; UpdateSceneCamera(camera);
        Check(!g_groundEye.valid,"FP exit still discards the saved standing reference");
        config.firstPersonBodyFollowsHead=true;
    }
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
        for (int s:{1,12,12,2}) for (int f:{0,64,128,192,256}) for (float lean:{0.f,.08f}) {
            state=int16_t(s); fraction=f;
            Head(.6f,.2f); VR().m_rawHeadPose.m[2][3]=neutral[2]-lean;
            const auto wanted=locomotion::Rotate({0,200+20*f/256.f},heading);
            PHD_3DPOS p{int32_t(std::lround(wanted.x)),-700,int32_t(std::lround(wanted.z)),0,0,0,0};
            ClampRenderedHeadToCollision(itemMemory,p);
            const auto physical=locomotion::Rotate({0,lean*1000},heading);
            const float rendered=(p.x_pos+physical.x)*impactNormalX+(p.z_pos+physical.z)*impactNormalZ;
            Check(rendered<101.5f,"run/wall-impact/stop camera consistently stops before raised crate");
            Check(p.y_pos==-700,"wall-impact correction preserves camera height");
            Check(!std::memcmp(&pos,&savedPos,sizeof(pos)) && !std::memcmp(&prev,&savedPrev,sizeof(prev)),
                  "wall-impact camera never changes Lara's current or interpolated root");
            Check(!std::memcmp(neutral,VR().m_firstPersonNeutral,sizeof(neutral)),
                  "wall-impact correction does not shift tracking neutral");
        }
        collisionMode=0; impactNormalX=0; impactNormalZ=1;
        resetRoomscale();
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
    VR().m_system = nullptr;
    std::printf("OK: %d first-person regression checks passed.\n", checks);
    return 0;
}
