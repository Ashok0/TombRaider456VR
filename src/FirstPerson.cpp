#include "FirstPerson.h"
#include "HKScopeMath.h"
#include "Config.h"
#include "Engine.h"
#include "GameDll.h"
#include "InlineHook.h"
#include "Log.h"
#include "VRSystem.h"
#include "LocomotionMath.h"
#include "FirstPersonClearance.h"
#include "FirstPersonVisibility.h"
#include "FirstPersonStabilization.h"
#include "MotionGunMath.h"
#include "FirstPersonIK.h"
#include "FirstPersonBodyIK.h"
#include "MotionGunInput.h"
#include "LedgePullGesture.h"

#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace tr {
namespace {

struct PHD_3DPOS {
    int32_t x_pos, y_pos, z_pos;
    int16_t x_rot, y_rot, z_rot;
    int16_t padding;
};

struct PHD_VECTOR { int32_t x, y, z; };
struct ShotVector { int32_t x, y, z; int16_t room, padding; };
static_assert(sizeof(ShotVector)==16 && offsetof(ShotVector,room)==12);
static_assert(sizeof(PHD_3DPOS) == 20);
static_assert(sizeof(PHD_VECTOR) == 12);

namespace off {
constexpr uint32_t item_stride = 9416;
constexpr uint32_t item_next_active = 32;
constexpr uint32_t lara_target = 208;
constexpr uint32_t item_mesh_bits   = 12;
constexpr uint32_t item_floor       = 0;
constexpr uint32_t item_floor_prev  = 4;
constexpr uint32_t item_object      = 16;
constexpr uint32_t item_anim_state  = 18;
constexpr uint32_t item_goal_state  = 20;
constexpr uint32_t item_anim_number = 24;
constexpr uint32_t item_speed       = 34;
constexpr uint32_t item_hit_points  = 38;
constexpr uint32_t item_room        = 28;
constexpr uint32_t item_pos         = 96;
constexpr uint32_t item_pos_prev    = 6208;
constexpr uint32_t arm_lock         = 12;
constexpr uint32_t arm_y_rot        = 14;
constexpr uint32_t arm_x_rot        = 16;
constexpr uint32_t arm_z_rot        = 18;
constexpr uint32_t lara_left_arm    = 240;
constexpr uint32_t lara_right_arm   = 264;
constexpr uint32_t lara_head_y_rot  = 224;
constexpr uint32_t lara_torso_z_rot = 234;
constexpr uint32_t lara_turn_rate   = 220;
constexpr uint32_t lara_move_angle  = 222;
constexpr uint32_t lara_vehicle     = 38;
constexpr uint32_t lara_movement_flags = 68; // IsMoving is bit 5; CanMonkeySwing is bit 6.
constexpr uint32_t lara_skelebob = 350; // uint8: native TR5 X-ray rendering mode.
constexpr uint32_t camera_type      = 32;
constexpr uint32_t object_stride    = 3792;
constexpr uint32_t object_geom      = 112;
constexpr uint32_t geom_stride      = 120;
constexpr uint32_t geom_mesh        = 16;
} // namespace off

constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kHeadMeshBit = 1u << 14;
constexpr uint32_t kArmMeshBits = 0x3f00; // upper arm, forearm, hand on both sides
constexpr int kLaraHeadGeoms = 4;
constexpr int32_t kCamFixed = 1;
constexpr int32_t kCamCinematic = 4;

using Fn_GenerateW2V = void (__cdecl*)(PHD_3DPOS*);
using Fn_DrawCreatureHD = void (__cdecl*)(uint8_t*, int32_t, int32_t);
using Fn_DrawHair = void (__cdecl*)(int32_t);
using Fn_GetJointAbsPositionLerp = void (__cdecl*)(uint8_t*, PHD_VECTOR*, int32_t, int32_t);
using Fn_AimWeapon = void (__cdecl*)(void*, uint8_t*);
using Fn_GetJoints = int32_t (__cdecl*)(uint8_t*, float*, int32_t);
using Fn_FireWeapon = int32_t (__cdecl*)(int32_t, void*, void*, const int16_t*);
using Fn_SetGunFlash = void (__cdecl*)(int32_t, int32_t);
using Fn_PistolHandler = void (__cdecl*)(int32_t);
using Fn_GetTargetOnLOS = int32_t (__cdecl*)(
    PHD_VECTOR*, PHD_VECTOR*, int32_t, int32_t);
using Fn_LaraAboveWater = void (__cdecl*)(uint8_t*, void*);
using Fn_AnimateLara = void (__cdecl*)(uint8_t*);

hook::InlineHook g_hGenerateW2V;
hook::InlineHook g_hDrawCreatureHD;
hook::InlineHook g_hDrawHair;
hook::InlineHook g_hAimWeapon;
hook::InlineHook g_hGetJoints;
hook::InlineHook g_hDrawToShadow;
struct ShadowDll { uint32_t timestamp, draw, torsoReturn, renderPass, viewRel, cast; };
constexpr ShadowDll kShadowDlls[] = {
    {0x696B4999,0xB8A00,0xB8AF0,0x4F2DD0,0x4948A0,0xC7A80},
    {0x696B499C,0xAC310,0xAC400,0x4EE720,0x4EF930,0xBC6C0},
    {0x68C12FDA,0xB9480,0xB9572,0x4F3D10,0x4957E0,0xC8750},
    {0x68C12FE9,0xAC510,0xAC602,0x4EE660,0x4EF870,0xBCAB0},
};
const ShadowDll* g_shadowDll=nullptr;
hook::InlineHook g_hFireWeapon;
hook::InlineHook g_hPistolHandler;
hook::InlineHook g_hAnimatePistols;
hook::InlineHook g_hSetGunFlash;
hook::InlineHook g_hGetTargetOnLOS;
hook::InlineHook g_hLaraAboveWater;
hook::InlineHook g_hAnimateLara;
hook::InlineHook g_hDrawActionIndicators;
struct ActionIconDll {
    uint32_t timestamp, draw, points, count, matrix, perspective, centerX, centerY, nearZ, farZ;
};
constexpr ActionIconDll kActionIconDlls[] = {
    {0x696B4999,0xD3390,0x52CCC0,0x6601BC,0x494800,0x4947E4,0x4947DC,0x4947E0,0x494748,0x494740},
    {0x696B499C,0xCAA10,0x529F40,0x65AC9C,0x4EF900,0x4EF8E0,0x4EF8D8,0x4EF8DC,0x4EF880,0x4EE934},
    {0x68C12FDA,0xD3EA0,0x52DC00,0x6610FC,0x495740,0x495724,0x49571C,0x495720,0x495688,0x495680},
    {0x68C12FE9,0xCAC70,0x529E80,0x65ABDC,0x4EF840,0x4EF820,0x4EF818,0x4EF81C,0x4EF7C0,0x4EE874},
};
const ActionIconDll* g_actionIconDll = nullptr;
bool g_loggedActionIcon = false;
const GameDllLayout* g_boundDll = nullptr;
uint64_t g_boundBase = 0;
uint64_t g_failedBase = 0;

bool g_runtimeEnabled = false;
bool g_runtimeInitialized = false;
bool g_active = false;
bool g_haveHeading = false;
float g_headingBase = 0.0f;
float g_lastHeadWorld = 0.0f;
stabilization::RenderTurn g_renderTurn;
struct TurnTrace { double start=0; unsigned polls=0,views=0,moved=0; float maxStep=0; };
TurnTrace g_turnTrace;
LARGE_INTEGER g_bodyTime{};
uint8_t* g_headingItem = nullptr;
locomotion::Vec g_previousBody{};
locomotion::Vec g_dragPrevious{}, g_dragCurrent{}, g_dragShown{};
locomotion::Vec g_manualLocal{}, g_manualWorld{};
bool g_haveManualInput = false;
bool g_shifted = false;
bool g_jumpPressed = false;
uint64_t g_groundMoveAction = 0; // Scoped stick intent, not the active native gait.
bool g_stabilizeRoot=false;
bool g_hardStopRoot=false; // Scoped to one ordinary above-water simulation tick.
uint64_t g_stabilizeAction=0;
locomotion::Vec g_stabilizeDirection{};
firstperson::LedgePullGesture g_ledgePull;
stabilization::RootMotion g_rootMotion;
stabilization::GroundEye g_groundEye;
stabilization::MountBodyTransition g_mountBodyTransition;
locomotion::Vec g_bodyVisualOffset{};
float g_lastBodyYaw=0;
int g_headingLevel=-1;
bool g_bodySkinScope=false, g_bodySkinReady=false;
float g_bodySkinPalette[33*12]{};
float g_bodyRenderPalette[33*12]{};
firstperson::ArmTwistState g_unarmedTwist[2]{};
uint64_t g_bodySkinMask=0;
int g_bodySkinCount=0;

void ResetMovementStabilization() {
    g_ledgePull.Reset();
    g_renderTurn.Reset();
    g_turnTrace={};
    g_stabilizeRoot=false;
    g_hardStopRoot=false;
    g_groundMoveAction=0;
    g_rootMotion.Reset();
    g_groundEye.Reset();
    g_mountBodyTransition.Reset();
    g_bodyVisualOffset={};
}

bool g_meshOverride = false;
bool g_headHidden = false;
bool g_rollHidden = false;
bool g_crouchHidden = false;
bool g_underwaterHidden = false;
bool g_ledgeArmsOnly = false;
bool g_unarmedArmsHidden = false;
firstperson::UnarmedArmVisibility g_unarmedArmVisibility;
uint8_t* g_meshItem = nullptr;
uint32_t g_meshBaseBits = 0;
unsigned g_anchored = 0;
unsigned g_skipped = 0;
unsigned g_faceSkips = 0;
unsigned g_hairSkips = 0;
bool g_loggedFirst = false;
bool g_loggedLost = false;

// PDB and retail call sites were checked against the four supported DLLs.
// The two AnimatePistols return addresses distinguish the actual firing hand.
struct MotionDll {
    uint32_t timestamp, getJoints, fireWeapon, fireViewReturn;
    uint32_t rightFireReturn, leftFireReturn, app, pistolHandler;
    uint32_t getTargetOnLOS, hitLosReturn, missLosReturn;
    uint32_t setGunFlash, floatMatrixPtr, rightFlashReturn, leftFlashReturn;
    uint32_t findTargetPoint, lineOfSight;
    uint32_t animatePistols, getSpheres, nextItemActive, items;
};
constexpr MotionDll kMotionDlls[] = {
    {0x696B4999, 0xC09E0, 0x5E4B0, 0x5E5C4, 0x5A97F, 0x5AC2A, 0x663F08, 0x5A270,
     0x15860, 0x5E701, 0x5E79B, 0x8EDD0, 0x1B6D18, 0x2F763, 0x2F819, 0x5E2F0, 0x149F0, 0x5A680, 0xA3C30, 0x63DAB0, 0x699CA0},
    {0x696B499C, 0xB5880, 0x5B790, 0x5B8A4, 0x57CBF, 0x57F6A, 0x66D1A0, 0x576A0,
     0x132F0, 0x5BA0C, 0x5BAA3, 0x8D790, 0x1B2AC8, 0x306A3, 0x30756, 0x5B5D0, 0x12450, 0x579C0, 0x9CF90, 0x65AC92, 0x66D1A8},
    {0x68C12FDA, 0xC1440, 0x5E1B0, 0x5E2C6, 0x5A67F, 0x5A92A, 0x664E48, 0x59F70,
     0x15440, 0x5E401, 0x5E49B, 0x8F7E0, 0x1B7D18, 0x2F5D3, 0x2F689, 0x5DFF0, 0x145D0, 0x5A380, 0xA47D0, 0x63E9F0, 0x69ABE0},
    {0x68C12FE9, 0xB5A00, 0x5C4E0, 0x5C5F6, 0x58A0F, 0x58CBA, 0x66D0E0, 0x583F0,
     0x133A0, 0x5C759, 0x5C7F0, 0x8DAA0, 0x1B2AC8, 0x30B23, 0x30BD6, 0x5C320, 0x12500, 0x58710, 0x9D300, 0x65ABD2, 0x66D0E8},
};
const MotionDll* g_motionDll = nullptr;
// DrawLaraHD selects HAND_*_RUN (2/32/62) independently of the joint pose.
// Verified against both PDB and retail renderers and HAND_NAMES in the PDBs.
struct UnarmedHandDll { uint32_t timestamp, hands; };
constexpr UnarmedHandDll kUnarmedHandDlls[] = {
    {0x696B4999,0x52F3C0}, {0x696B499C,0x52C640},
    {0x68C12FDA,0x530300}, {0x68C12FE9,0x52C580},
};
const UnarmedHandDll* g_unarmedHandDll=nullptr;

// Separate, optional extension: a mismatch must not disable working dual guns.
struct LongGunDll {
    uint32_t timestamp, rifle, shotgun, special, crossbow, joint, initialise, items;
    uint32_t opticReturns[4];
};
constexpr LongGunDll kLongGunDlls[] = {
    {0x696B4999,0x55ED0,0x56420,0x56A70,0x57D30,0xA37C0,0x40840,0x699CA0,
     {0xE347F,0xE34C9,0xE3F6A,0xE3F88}},
    {0x696B499C,0x54D40,0x55150,0x557A0,0x558C0,0x9C9C0,0x45800,0x66D1A8,
     {0xD7A1F,0xD7A69,0xD84FA,0xD8518}},
    {0x68C12FDA,0x55BE0,0x56130,0x56770,0x57A20,0xA4360,0x40300,0x69ABE0,
     {0xE3F9F,0xE3FE9,0xE4A9E,0xE4ABC}},
    {0x68C12FE9,0x55A70,0x55E80,0x564C0,0x565E0,0x9CD30,0x46160,0x66D0E8,
     {0xD7C9F,0xD7CE9,0xD878E,0xD87AC}},
};
const LongGunDll* g_longGunDll=nullptr;
bool g_longGunHooksReady=false, g_opticShot=false;
int g_triggerWeapon=0;
hook::InlineHook g_hRifleHandler, g_hFireShotgun, g_hFireSpecial, g_hFireCrossbow;
hook::InlineHook g_hGunJoint, g_hInitialiseProjectile;
bool g_motionHooksReady = false;
bool g_scenePoseValid = false;
PHD_3DPOS g_scenePose{};
int g_renderArm = -1; // 0=left, 1=right
int g_renderHandJoint = -1;
// DrawLaraHD overwrites objects[item].geom for each body/hand/face pass.
// Only the right-HK GetJoints scope owns the correct immutable wrist binding.
// Cache that binding, never a world/controller pose (which would add lag).
motiongun::Frame g_hkScopeInverseBind{};
const uint8_t* g_hkScopeBindItem=nullptr;
motiongun::TriggerInput g_gunTriggers;
motiongun::EquipInput g_gunEquip;
bool g_handFired[2]{};
int g_firingHand = -1;
int16_t g_firingAim[2] = {};
PHD_3DPOS g_firingPose{};
motiongun::Vec g_firingDirection{};
unsigned g_motionArmDraws[2] = {};
unsigned g_motionShots[2] = {};
uint64_t g_motionReportTime = 0;

struct MotionRange {
    float low = 0, high = 0;
    void Add(float value, bool first) {
        low = first ? value : std::min(low, value);
        high = first ? value : std::max(high, value);
    }
};
struct CameraMotionTrace {
    uint64_t start = 0;
    unsigned samples = 0;
    locomotion::Vec previousBody{}, previousEye{};
    MotionRange animatedSide, rootSideStep, eyeSideStep, trackedSide, yawError;
    int fracMin = 256, fracMax = 0;
};
CameraMotionTrace g_cameraMotionTrace{};

const uint8_t kGenerateW2VPrologue[] = { 0x40, 0x53, 0x55, 0x56, 0x57 };
const uint8_t kDrawCreatureDebug[] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };
const uint8_t kDrawCreatureRetail[] = { 0x48, 0x89, 0x5C, 0x24, 0x18 };
const uint8_t kDrawHairPrologue[] = { 0x48, 0x89, 0x5C, 0x24, 0x20 };
const uint8_t kAimWeaponPrologue[] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };
const uint8_t kSetGunFlashTR4[] = {0x4C,0x8B,0xDC,0x48,0x81,0xEC,0x98,0,0,0};
const uint8_t kSetGunFlashTR5[] = {0x48,0x81,0xEC,0xA8,0,0,0};
const uint8_t kGetJointsPrologue[] = { 0x44, 0x89, 0x44, 0x24, 0x18 };
const uint8_t kFireWeaponPrologue[] = { 0x4C, 0x89, 0x44, 0x24, 0x18 };
const uint8_t kAnimatePistols[] = {0x48,0x89,0x5C,0x24,0x10};
const uint8_t kPistolHandlerTR4[] = { 0x48, 0x89, 0x5C, 0x24, 0x18 };
const uint8_t kPistolHandlerTR5[] = { 0x48, 0x89, 0x5C, 0x24, 0x10 };
const uint8_t kGetTargetOnLOSTR4[] = { 0x40, 0x55, 0x56, 0x57, 0x41, 0x56 };
const uint8_t kGetTargetOnLOSTR5[] = { 0x40, 0x55, 0x53, 0x57, 0x41, 0x55 };
const uint8_t kLaraAboveWaterTR4[] = { 0x48, 0x89, 0x6C, 0x24, 0x20 };
const uint8_t kLaraAboveWaterTR5[] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };
const uint8_t kAnimateLaraTR4[] = { 0x40, 0x53, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00 };
const uint8_t kAnimateLaraTR5[] = { 0x40, 0x53, 0x55, 0x56, 0x41, 0x57 };
const uint8_t kCollisionDebug[] = { 0x4C, 0x8B, 0xDC, 0x45, 0x89, 0x4B, 0x20 };
const uint8_t kCollisionRetail[] = { 0x4C, 0x8B, 0xDC, 0x45, 0x89, 0x43, 0x18 };
const uint8_t kGetFloor[] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };
const uint8_t kGetHeightTR4[] = { 0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56 };
const uint8_t kGetHeightTR5[] = { 0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x55 };
const uint8_t kItemNewRoom[] = { 0x48, 0x89, 0x5C, 0x24, 0x10 };

template <typename T>
T* Ptr(uint32_t rva) {
    return reinterpret_cast<T*>(g_boundBase + rva);
}

float Wrap(float radians) {
    return std::remainder(radians, 2.0f * kPi);
}

float Radians(int16_t angle) {
    return angle * (2.0f * kPi / 65536.0f);
}

int16_t Angle(float radians) {
    return static_cast<int16_t>(static_cast<int>(
        Wrap(radians) * (65536.0f / (2.0f * kPi))));
}

float Elapsed(LARGE_INTEGER& last) {
    LARGE_INTEGER now{}, freq{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    const float seconds = last.QuadPart && freq.QuadPart
        ? static_cast<float>(double(now.QuadPart - last.QuadPart) /
                             double(freq.QuadPart))
        : 0.0f;
    last = now;
    return std::clamp(seconds, 0.0f, 0.05f);
}

double TurnTime() {
    LARGE_INTEGER now{},freq{};
    QueryPerformanceCounter(&now); QueryPerformanceFrequency(&freq);
    return freq.QuadPart ? double(now.QuadPart)/double(freq.QuadPart) : 0;
}

void AdvanceStickTurn(double now) {
    const float turn=g_renderTurn.Step(now);
    if (turn!=0) {
        g_headingBase=Wrap(g_headingBase+turn);
        VR().PivotHeadFloorOffset(turn);
    }
    if (!Cfg().firstPersonDriftLog) { g_turnTrace={}; return; }
    auto& trace=g_turnTrace;
    if (!trace.start) trace.start=now;
    ++trace.views;
    if (turn!=0) ++trace.moved;
    trace.maxStep=std::max(trace.maxStep,std::fabs(turn)*180/kPi);
    if (now-trace.start>=1) {
        if (trace.moved) LogF("fp-turn: %.2fs polls=%u views=%u turned=%u max-step=%.3fdeg",
            now-trace.start,trace.polls,trace.views,trace.moved,trace.maxStep);
        trace={}; trace.start=now;
    }
}

bool IsSceneCall(const void* returnAddress) {
    return g_boundDll && g_boundBase &&
        reinterpret_cast<uint64_t>(returnAddress) ==
            g_boundBase + g_boundDll->w2vSceneReturn;
}

bool ScriptedCameraActive() {
    if (!g_boundDll || !g_boundBase) return false;
    const auto& d=*g_boundDll;
    // TR5 X-ray floor triggers set skelebob for the complete rendered effect.
    // Its side-on camera can alternate with chase/look types, so type alone
    // would repeatedly resume FP and corrupt the X-ray presentation. Suspend
    // through the normal camera gate, preserving the player's FP preference.
    if (d.game==1 && *Ptr<uint8_t>(d.lara+off::lara_skelebob)) return true;
    if (d.playingCutseq && *Ptr<int32_t>(d.playingCutseq)) return true;
    // Native handle_cutseq_triggering only uses this state while an ID is
    // active. Include fade-in/out without treating a stale trigger as a scene.
    if (d.cutseqNum && d.cutseqTrigger && *Ptr<int32_t>(d.cutseqNum) &&
        *Ptr<int32_t>(d.cutseqTrigger)) return true;
    if (d.useSpotCam && *Ptr<int32_t>(d.useSpotCam)) return true;
    return d.vonCroyCutscene && *Ptr<uint8_t>(d.vonCroyCutscene);
}

bool Gate() {
    if (!g_boundDll || !g_boundBase || !Cfg().enabled ||
        !Cfg().firstPerson || !g_runtimeEnabled)
        return false;
    if (!VR().active() || !VR().poseValid()) return false;
    if (InInventory() || InFMV() || InTitle()) return false;
    if (ScriptedCameraActive()) return false;
    const GameDllLayout& dll = *g_boundDll;
    const auto* item=*Ptr<uint8_t*>(dll.laraItem);
    if (!item || !firstperson::UseHeadCamera(
        *reinterpret_cast<const int16_t*>(item+off::item_hit_points))) return false;
    // Read the active animation state, not its goal: native switches can
    // request STOP before the interaction animation has finished. Suspending
    // through Gate restores the camera, full mesh and input together; it does
    // not toggle the user's first-person preference.
    if(firstperson::SwitchUsesThirdPerson(dll.game,
        *reinterpret_cast<const int16_t*>(item+off::item_anim_state))) return false;
    const int water=LaraWaterStatus();
    // Suspend the whole FP path, not just its camera or body mask. Wading
    // remains eligible; native surface/underwater swimming uses third person.
    if (water==1 || water==2) return false;
    const int32_t type = *Ptr<int32_t>(g_boundDll->camera + off::camera_type);
    return type != kCamFixed && type < kCamCinematic;
}

void RestoreHeadMesh() {
    if (g_meshOverride && g_meshItem && g_boundDll && g_boundBase &&
        GameDllBound() == g_boundDll && GameDllBase() == g_boundBase) {
        uint8_t* current = *Ptr<uint8_t*>(g_boundDll->laraItem);
        if (current == g_meshItem) {
            *reinterpret_cast<uint32_t*>(g_meshItem + off::item_mesh_bits) =
                g_meshBaseBits;
        }
    }
    g_meshOverride = false;
    g_meshItem = nullptr;
    g_headHidden = g_rollHidden = g_crouchHidden = g_underwaterHidden = g_ledgeArmsOnly = false;
    g_unarmedArmsHidden=false;
}

// Native icons are world points projected into a flat HUD, not Lara meshes.
// At head height a nearby floor/switch marker can be below that panel or just
// behind the eye. Keep these existing, native-approved prompts readable. Do
// not create interactions, bypass LOS, or reveal distant/behind-player markers.
bool PlaceActionIcon(PHD_VECTOR& point, const float* matrix, float perspective,
                     float centerX, float centerY, float nearZ, float farZ) {
    if (!std::isfinite(perspective) || perspective <= 0 ||
        !std::isfinite(centerX) || !std::isfinite(centerY) || centerX <= 0 || centerY <= 0 ||
        !std::isfinite(nearZ) || !std::isfinite(farZ) || nearZ < 0 || farZ <= nearZ + 64)
        return false;
    for (int i=0;i<12;++i) if (!std::isfinite(matrix[i])) return false;
    // fw2v_matrix stores unit rotation rows and the camera WORLD position in
    // its fourth column (not the usual affine translation).
    for (int row=0;row<3;++row) for (int other=0;other<=row;++other) {
        float dot=0;
        for (int j=0;j<3;++j) dot+=matrix[row*4+j]*matrix[other*4+j];
        if (std::fabs(dot-(row==other ? 1.0f : 0.0f))>0.002f) return false;
    }
    const float delta[3]={float(point.x)-matrix[3],float(point.y)-matrix[7],float(point.z)-matrix[11]};
    if (delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]>1024.0f*1024.0f) return false;
    float view[3]={};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) view[i]+=matrix[i*4+j]*delta[j];
    if (view[2]<-256 || view[2]>=farZ) return false;
    const float depth=std::max(256.0f,nearZ+32.0f);
    if (depth>=farZ) return false;
    const float maxX=0.70f*centerX/perspective, maxY=0.70f*centerY/perspective;
    const float denominator=std::max(depth,view[2]);
    const float x=std::clamp(view[0]/denominator,-maxX,maxX);
    const float y=std::clamp(view[1]/denominator,-maxY,maxY);
    if (view[2]>=depth && x==view[0]/denominator && y==view[1]/denominator) return false;
    const float adjusted[3]={x*denominator,y*denominator,denominator};
    int32_t result[3];
    for (int j=0;j<3;++j) {
        double world=matrix[j*4+3];
        for (int i=0;i<3;++i) world+=matrix[i*4+j]*adjusted[i];
        if (!std::isfinite(world) || world<double(INT32_MIN)+1 || world>double(INT32_MAX)-1) return false;
        result[j]=int32_t(std::lround(world));
    }
    point={result[0],result[1],result[2]};
    return true;
}

void __cdecl Detour_DrawActionIndicators() {
    using Fn = void (__cdecl*)();
    const auto original=g_hDrawActionIndicators.Original<Fn>();
    if (!g_active || !g_boundBase || !g_actionIconDll || !VR().poseValid()) {
        original(); return;
    }
    const auto& d=*g_actionIconDll;
    const int count=*Ptr<int32_t>(d.count);
    if (count<=0 || count>20) { original(); return; }
    auto* points=Ptr<PHD_VECTOR>(d.points);
    PHD_VECTOR saved[20];
    std::memcpy(saved,points,count*sizeof(PHD_VECTOR));
    bool changed=false;
    for (int i=0;i<count;++i)
        changed=PlaceActionIcon(points[i],Ptr<float>(d.matrix),float(*Ptr<int32_t>(d.perspective)),
            float(*Ptr<int32_t>(d.centerX)),float(*Ptr<int32_t>(d.centerY)),
            *Ptr<float>(d.nearZ),*Ptr<float>(d.farZ)) || changed;
    // Original keeps the player's Action Indicators setting, menu gates,
    // native icon artwork and HUD/stereo rendering. Restore before any logic
    // or subsequent eye/frame can observe our draw-local placement.
    original();
    std::memcpy(points,saved,count*sizeof(PHD_VECTOR));
    if (changed && !g_loggedActionIcon) {
        g_loggedActionIcon=true;
        Log("firstperson: nearby Action icon placement corrected on HUD");
    }
}

uint32_t VisibleMeshBits(uint32_t base, bool head, bool roll, bool crouch, bool ledge, bool underwater = false, bool hideArms = false) {
    if (roll || crouch || underwater) return 0;
    if (ledge) return base & kArmMeshBits;
    return base & ~(head ? kHeadMeshBit : 0u) & ~(hideArms ? kArmMeshBits : 0u);
}

void SetMeshVisibility(bool hideHead, bool hideRoll, bool hideCrouch = false,
                       bool ledgeArms = false, bool hideUnderwater = false, bool hideArms = false) {
    if ((!hideHead && !hideRoll && !hideCrouch && !ledgeArms && !hideUnderwater && !hideArms) || !g_boundDll || !g_boundBase) {
        RestoreHeadMesh();
        return;
    }
    uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (!item) {
        g_meshOverride = false;
        g_meshItem = nullptr;
        g_headHidden = g_rollHidden = g_crouchHidden = g_underwaterHidden = g_ledgeArmsOnly = false;
        g_unarmedArmsHidden=false;
        return;
    }
    if (!g_meshOverride || item != g_meshItem) {
        g_meshOverride = true;
        g_meshItem = item;
        g_meshBaseBits = *reinterpret_cast<uint32_t*>(
            item + off::item_mesh_bits);
        g_headHidden = g_rollHidden = g_crouchHidden = g_underwaterHidden = g_ledgeArmsOnly = false;
        g_unarmedArmsHidden=false;
    }
    auto& bits = *reinterpret_cast<uint32_t*>(item + off::item_mesh_bits);
    const uint32_t expected = VisibleMeshBits(g_meshBaseBits,g_headHidden,
        g_rollHidden,g_crouchHidden,g_ledgeArmsOnly,g_underwaterHidden,g_unarmedArmsHidden);
    if (g_meshOverride && bits != expected) {
        // Preserve native visibility changes outside our owned mask. During
        // a hidden stance we own the full mask and retain its original snapshot.
        if (!g_rollHidden && !g_crouchHidden && !g_underwaterHidden && !g_ledgeArmsOnly) {
            const uint32_t owned=kHeadMeshBit | (g_unarmedArmsHidden ? kArmMeshBits : 0u);
            g_meshBaseBits = (bits & ~owned) | (g_meshBaseBits & owned);
        }
    }
    g_headHidden = hideHead;
    g_rollHidden = hideRoll;
    g_crouchHidden = hideCrouch;
    g_underwaterHidden = hideUnderwater;
    g_ledgeArmsOnly = ledgeArms;
    g_unarmedArmsHidden=hideArms;
    bits = VisibleMeshBits(g_meshBaseBits,hideHead,hideRoll,hideCrouch,ledgeArms,hideUnderwater,hideArms);
}

bool IsCrouchState(const uint8_t* item) {
    if (!item) return false;
    // Verified in both games' lara_control_routines: duck, all4s/crawl,
    // all4turnl/r, crawlb and duckl/r. Duck-roll already uses IsRollState.
    switch (*reinterpret_cast<const int16_t*>(item + off::item_anim_state)) {
    case 71: case 80: case 81: case 84: case 85: case 86:
    case 105: case 106: return true;
    default: return false;
    }
}

bool IsRollState(const uint8_t* item) {
    if (!item) return false;
    switch (*reinterpret_cast<const int16_t*>(item + off::item_anim_state)) {
    case 23: case 45: case 66: case 68: case 72: return true;
    default: return false;
    }
}

bool CanTurnBody(const uint8_t* item) {
    if (*reinterpret_cast<const int16_t*>(item + off::item_hit_points) <= 0 ||
        LaraWaterStatus() != 0)
        return false;
    // Vehicle is an active item index / -1 sentinel ONLY in TR4. TR5 retains
    // the struct slot but InitialiseLara leaves it zero and LaraAboveWater
    // never reads it. Applying TR4's guard there disables all normal FP gait,
    // body turning and room-scale motion. A shared offset is not shared use.
    if (g_boundDll->game == 0 &&
        *Ptr<int16_t>(g_boundDll->lara + off::lara_vehicle) >= 0)
        return false;
    // MoveLaraPosition owns root/yaw while auto-aligning to a switch or
    // other interaction. The state can still be stop/walk/run at this point;
    // head following, roomscale drag and FP gait overrides must all yield.
    if ((*Ptr<uint32_t>(g_boundDll->lara + off::lara_movement_flags) & 0x20) != 0)
        return false;
    // Include TR4/5 sprint (73) in ground steering/body fitting. Otherwise
    // native modern controls turn the sprinting body away from the FP view.
    // Ladders, pickups, jumps and interactions retain their native facing.
    switch (*reinterpret_cast<const int16_t*>(item + off::item_anim_state)) {
    case 0: case 1: case 2: case 5: case 6: case 7:
    case 16: case 20: case 21: case 22: case 73: return true;
    default: return false;
    }
}

bool CanSteerMonkeyBars(const uint8_t* item) {
    return item && LaraWaterStatus()==0 &&
        *reinterpret_cast<const int16_t*>(item+off::item_hit_points)>0 &&
        locomotion::IsMonkeyBarState(*reinterpret_cast<const int16_t*>(item+off::item_anim_state)) &&
        (*Ptr<uint32_t>(g_boundDll->lara+off::lara_movement_flags)&0x40)!=0;
}

bool UseLandingCamera(const uint8_t* item) {
    return item && CanTurnBody(item) && locomotion::IsLandingAnimation(g_boundDll->game,
        *reinterpret_cast<const int16_t*>(item+off::item_anim_state),
        *reinterpret_cast<const int16_t*>(item+off::item_anim_number));
}

bool HideLandingHead(const uint8_t* item) {
    return g_active && g_boundDll && g_boundBase && item &&
        item==*Ptr<uint8_t*>(g_boundDll->laraItem) && UseLandingCamera(item);
}

bool CanModifyGroundMotion(const uint8_t* item) {
    if (!CanTurnBody(item) || (*reinterpret_cast<const uint32_t*>(item+0x1820)&8))
        return false;
    // A requested jump/interaction must keep its animation displacement even
    // before current_anim_state has caught up with goal_anim_state.
    switch (*reinterpret_cast<const int16_t*>(item+off::item_goal_state)) {
    case 0: case 1: case 2: case 5: case 6: case 7:
    case 16: case 20: case 21: case 22: case 73: return true;
    default: return false;
    }
}

void TurnBodyToHead(uint8_t* item, float dt) {
    if (!Cfg().firstPersonBodyFollowsHead || !CanTurnBody(item)) return;
    auto& pos = *reinterpret_cast<PHD_3DPOS*>(item + off::item_pos);
    const float delta = Wrap(g_headingBase + VR().HeadYawRadians() - Radians(pos.y_rot));
    const float dead = std::clamp(Cfg().firstPersonBodyDeadzoneDegrees, 0.0f, 90.0f)
        * (kPi / 180.0f);
    const float move = std::copysign(std::max(0.0f, std::fabs(delta) - dead), delta);
    const float limit = std::max(0.0f, Cfg().firstPersonBodyTurnDegreesPerFrame)
        * 60.0f * dt * (kPi / 180.0f);
    pos.y_rot = Angle(Radians(pos.y_rot) + std::clamp(move, -limit, limit));
}

using GunVec = motiongun::Vec;
using GunBasis = motiongun::Basis;
using motiongun::Add;
using motiongun::Sub;
using motiongun::Transform;
struct GunPose {
    GunVec nativeHand{}, trackedHand{}, muzzle{}, direction{};
    GunBasis desired{};
    int16_t yaw = 0, pitch = 0;
};

struct LongShot {
    int weapon=0;
    GunPose gun{};
    int16_t baseAim[2]{};
    bool projectile=false;
    mutable bool fired=false; // Aggregate pellets/projectiles into one haptic burst.
};
const LongShot* g_longShot=nullptr;

int CurrentMotionWeapon() {
    return g_boundDll ? *Ptr<int16_t>(g_boundDll->lara+4) : 0;
}
bool MotionWeaponSupported(int gun) {
    return motiongun::DualWeapon(gun) ||
        (g_longGunHooksReady && motiongun::SupportedWeapon(gun));
}

HHOOK g_calibrationMessageHook=nullptr;
HWND g_calibrationWindow=nullptr;
bool g_calibrationActive=false;
motiongun::CalibrationKeys g_calibrationKeys{};
int g_calibrationCommands[64]{};
unsigned g_calibrationCommandCount=0;

const char* ControllerTrackingBlockedReason() {
    if (!Cfg().firstPersonMotionGuns) return "disabled-in-INI";
    if (!g_boundDll || !g_boundBase || GameDllBound()!=g_boundDll ||
        GameDllBase()!=g_boundBase) return "game-DLL-changing";
    if (!g_motionHooksReady || !g_motionDll) return "motion-hooks-unavailable";
    if (!g_scenePoseValid) return "no-scene-camera";
    if (!g_active) return "first-person-inactive";
    if (!g_haveHeading) return "no-heading";
    if (!Gate()) return "first-person-gate";
    if (!Cfg().positionalTracking) return "positional-tracking-disabled";
    if (!Cfg().firstPersonHeadTranslation) return "head-translation-disabled";
    // Classic graphics use a separate Lara renderer. Keep the old head-aim
    // path there until that renderer has its own tracked-arm implementation.
    uint8_t* app = *Ptr<uint8_t*>(g_motionDll->app);
    if (!app || !(*reinterpret_cast<const uint8_t*>(app + 0x9e4) & 1))
        return "classic-graphics-or-no-app";
    const uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (item != g_headingItem) return "Lara-item-changed";
    return nullptr;
}

const char* MotionBlockedReason() {
    if (const auto* reason=ControllerTrackingBlockedReason()) return reason;
    const uint8_t* lara = Ptr<uint8_t>(g_boundDll->lara);
    const int16_t status = *reinterpret_cast<const int16_t*>(lara + 2);
    const int16_t gun = *reinterpret_cast<const int16_t*>(lara + 4);
    if (status != 4) return "guns-not-ready";
    if (!MotionWeaponSupported(gun)) return "weapon-not-supported";
    vr::HmdMatrix34_t pose{};
    if (motiongun::DualWeapon(gun) && !VR().ControllerPose(0,pose))
        return "left-controller-pose-missing";
    if (!VR().ControllerPose(1,pose)) return "right-controller-pose-missing";
    return nullptr;
}

bool MotionReady() {
    return MotionBlockedReason()==nullptr;
}

bool MotionTriggerMode() {
    if (!Cfg().firstPersonMotionGuns || !g_boundDll || !g_boundBase ||
        GameDllBound()!=g_boundDll || GameDllBase()!=g_boundBase ||
        !g_motionHooksReady || !g_motionDll || !g_active || !Gate() ||
        !Cfg().positionalTracking || !Cfg().firstPersonHeadTranslation) return false;
    const auto* app=*Ptr<uint8_t*>(g_motionDll->app);
    if (!app || !(app[0x9e4]&1)) return false;
    const auto* lara=Ptr<uint8_t>(g_boundDll->lara);
    int gun=*reinterpret_cast<const int16_t*>(lara+4);
    if (gun==0) gun=*reinterpret_cast<const int16_t*>(lara+8); // last equipped
    return MotionWeaponSupported(gun);
}

bool WeaponControlMode() {
    if (!g_boundDll || !g_boundBase || GameDllBound()!=g_boundDll ||
        GameDllBase()!=g_boundBase || !g_active || !Gate()) return false;
    int gun=*Ptr<int16_t>(g_boundDll->lara+4);
    if (!gun) gun=*Ptr<int16_t>(g_boundDll->lara+8);
    return MotionWeaponSupported(gun);
}

const char* g_gunPoseFailure[2] = {"not-built","not-built"};
unsigned g_headAimFallbacks[2]{};
void ReportMotionState(const char* where) {
    if (!Cfg().firstPersonMotionGuns || !g_boundDll || !g_boundBase ||
        GameDllBound()!=g_boundDll || GameDllBase()!=g_boundBase) return;
    const auto* reason=MotionBlockedReason();
    const auto* lara=Ptr<uint8_t>(g_boundDll->lara);
    vr::HmdMatrix34_t pose{};
    float r=0,d=0,f=0;
    const bool left=VR().ControllerPose(0,pose), right=VR().ControllerPose(1,pose);
    const bool leftOffset=VR().FirstPersonControllerOffset(0,r,d,f);
    const bool rightOffset=VR().FirstPersonControllerOffset(1,r,d,f);
    const uint8_t* app=g_motionDll ? *Ptr<uint8_t*>(g_motionDll->app) : nullptr;
    LogF("firstperson: Touch state [%s] blocked=%s game=%d gun=%d status=%d HD=%d scene=%d active=%d heading=%d poses=%d/%d offsets=%d/%d build=%s/%s head-fallbacks=%u/%u",
        where,reason ? reason : "none",g_boundDll->game,
        int(*reinterpret_cast<const int16_t*>(lara+4)),
        int(*reinterpret_cast<const int16_t*>(lara+2)),
        app ? int(app[0x9e4]&1) : -1,int(g_scenePoseValid),int(g_active),
        int(g_haveHeading),int(left),int(right),int(leftOffset),int(rightOffset),
        g_gunPoseFailure[0],g_gunPoseFailure[1],
        g_headAimFallbacks[0],g_headAimFallbacks[1]);
}

bool CalibrationFocused() {
    return g_calibrationWindow && GetForegroundWindow()==g_calibrationWindow;
}

LRESULT CALLBACK CalibrationMessageHook(int code, WPARAM remove, LPARAM param) {
    // Only consume messages actually removed from this game's render/window
    // thread queue. Never hook other applications or use a global keyboard hook.
    if (code>=0 && remove==PM_REMOVE) {
        auto* msg=reinterpret_cast<MSG*>(param);
        if (msg->hwnd==g_calibrationWindow &&
            (msg->message==WM_KEYDOWN || msg->message==WM_KEYUP ||
             msg->message==WM_SYSKEYDOWN || msg->message==WM_SYSKEYUP)) {
            const int key=int(msg->wParam)-VK_F1;
            const bool down=msg->message==WM_KEYDOWN || msg->message==WM_SYSKEYDOWN;
            int command=-1;
            // Never dereference a game-DLL address from the message hook:
            // game switches can unload it before the next render update.
            const bool active=g_calibrationActive && CalibrationFocused() &&
                !InInventory() && !InTitle() && !InFMV();
            const bool controlKey=msg->wParam==VK_CONTROL ||
                msg->wParam==VK_LCONTROL || msg->wParam==VK_RCONTROL;
            // Ctrl is native Action/Fire. Reserve the modifier too while
            // calibration is available, so adjusting a gun cannot fire it.
            const bool consumed=controlKey
                ? g_calibrationKeys.ControlEvent(down,active,
                    (GetKeyState(VK_MENU)&0x8000)!=0)
                : g_calibrationKeys.Event(key,down,
                    (GetKeyState(VK_CONTROL)&0x8000)!=0,
                    (GetKeyState(VK_SHIFT)&0x8000)!=0,
                    (GetKeyState(VK_MENU)&0x8000)!=0,active,
                    down && (msg->lParam & (LPARAM(1)<<30)),command);
            if (consumed) {
                if (command>=0 && g_calibrationCommandCount<64)
                    g_calibrationCommands[g_calibrationCommandCount++]=command;
                // Block the matching keyup too, even if Ctrl was released first.
                msg->message=WM_NULL; msg->wParam=0; msg->lParam=0;
            }
        }
    }
    return CallNextHookEx(g_calibrationMessageHook,code,remove,param);
}

void PollMotionGunCalibration() {
    g_calibrationActive=Cfg().firstPersonMotionGunHotkeys &&
        g_boundDll && GameDllBound()==g_boundDll && GameDllBase()==g_boundBase &&
        MotionReady();
    if (!g_calibrationMessageHook && Cfg().firstPersonMotionGuns &&
        Cfg().firstPersonMotionGunHotkeys) {
        HWND window=GetForegroundWindow();
        DWORD process=0;
        const DWORD thread=GetWindowThreadProcessId(window,&process);
        if (window && process==GetCurrentProcessId() && thread==GetCurrentThreadId()) {
            g_calibrationWindow=window;
            g_calibrationMessageHook=SetWindowsHookExW(
                WH_GETMESSAGE,CalibrationMessageHook,nullptr,thread);
            if (g_calibrationMessageHook)
                Log("gun calibration: Ctrl+F1..F6 position, Ctrl+Shift+F1..F6 angles; F7 save/restore");
        }
    }
    if (g_calibrationActive && CalibrationFocused()) {
        for (unsigned i=0;i<g_calibrationCommandCount;++i) {
            const int command=g_calibrationCommands[i];
            if (command==6) {
                const bool saved=SaveMotionGunCalibration();
                MessageBeep(saved ? MB_OK : MB_ICONERROR);
            } else if (command==13) {
                RestoreMotionGunCalibration();
                MessageBeep(MB_OK);
            } else AdjustMotionGunCalibration(command);
        }
    }
    g_calibrationCommandCount=0;
}

void ReportMotionActivity() {
    if (!g_boundDll || !g_boundBase || GameDllBound()!=g_boundDll ||
        GameDllBase()!=g_boundBase || !Cfg().firstPersonMotionGuns) return;
    const uint64_t now = GetTickCount64();
    if (!g_motionReportTime) g_motionReportTime = now;
    if (now - g_motionReportTime < 5000) return;
    const bool ready = MotionReady();
    if (g_active || g_headAimFallbacks[0] || g_headAimFallbacks[1])
        ReportMotionState("periodic");
    if (ready || g_motionArmDraws[0] || g_motionArmDraws[1] ||
        g_motionShots[0] || g_motionShots[1]) {
        LogF("firstperson: Touch ready=%d arms L=%u R=%u shots L=%u R=%u",
             ready ? 1 : 0,
             g_motionArmDraws[0], g_motionArmDraws[1],
             g_motionShots[0], g_motionShots[1]);
    }
    g_motionArmDraws[0] = g_motionArmDraws[1] = 0;
    g_motionShots[0] = g_motionShots[1] = 0;
    g_headAimFallbacks[0] = g_headAimFallbacks[1] = 0;
    g_motionReportTime = now;
}

// Native joint 10 is the right gun and joint 13 the left gun. Query the
// interpolated native joint axes so the renderer's whole arm pass can be moved
// rigidly without touching Lara's simulation skeleton or doing IK.
GunVec JointPoint(uint8_t* item, int joint, int x, int y, int z) {
    PHD_VECTOR p{x, y, z};
    const int frac = std::clamp(*Ptr<int32_t>(g_boundDll->frameFrac), 0, 256);
    reinterpret_cast<Fn_GetJointAbsPositionLerp>(
        g_boundBase + g_boundDll->getJointAbsPositionLerp)(item, &p, joint, frac);
    return {float(p.x), float(p.y), float(p.z)};
}

bool BuildControllerWrist(int hand, motiongun::Frame& out, GunBasis& controller) {
    if (hand < 0 || hand > 1) return false;
    if (const auto* reason=ControllerTrackingBlockedReason()) {
        g_gunPoseFailure[hand]=reason;
        return false;
    }
    auto rejected=[hand](const char* reason) {
        g_gunPoseFailure[hand]=reason; return false;
    };
    float right, down, forward;
    vr::HmdMatrix34_t tracked{};
    if (!VR().FirstPersonControllerOffset(hand, right, down, forward) ||
        !VR().ControllerPose(hand, tracked))
        return rejected("controller-offset-unavailable");
    if (!std::isfinite(right) || !std::isfinite(down) || !std::isfinite(forward) ||
        std::fabs(right)>2 || std::fabs(down)>2 || std::fabs(forward)>2)
        return rejected("controller-offset-over-2m");
    const float scale = LiveWorldUnitsPerMetre();
    if (!std::isfinite(scale) || scale <= 1) return rejected("invalid-world-scale");
    const auto& calibration=LiveMotionGunCalibration();
    controller = motiongun::CalibratedController(
        motiongun::ControllerBasis(tracked.m, g_headingBase),calibration);
    out.origin = motiongun::HandInWorld(
        {float(g_scenePose.x_pos), float(g_scenePose.y_pos), float(g_scenePose.z_pos)},
        right, down, forward, g_headingBase, scale);
    // Keep the recovered bind-pose pivot path. Calibrate the mesh grip in
    // local gun space so that point, rather than a displaced wrist origin,
    // stays on the tracked controller through wrist and physical body turns.
    out.basis = motiongun::GunBasis(controller);
    out.origin = motiongun::GripFrame(out.basis, out.origin,
        calibration.gripForwardMetres*scale,
        calibration.raiseMetres*scale,calibration.rightMetres*scale).origin;

    motiongun::Frame inverse{};
    return motiongun::Inverse(out,inverse); // Reject invalid tracked rotations too.
}

// These are render-pose permissions, separate from gameplay body turning.
// Jumping can retain controllers; ledges/mounts keep their authored hand grips.
bool FullBodyGripState(const uint8_t* item) {
    if(!item) return false;
    const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
    return locomotion::IsLedgeHangState(state) || locomotion::IsLedgeMountState(state);
}
bool FullBodyState(const uint8_t* item) {
    if(!item || *reinterpret_cast<const int16_t*>(item+off::item_hit_points)<=0 ||
       LaraWaterStatus()!=0 || (g_boundDll->game==0 &&
       *Ptr<int16_t>(g_boundDll->lara+off::lara_vehicle)>=0)) return false;
    if(CanTurnBody(item) || FullBodyGripState(item)) return true;
    switch(*reinterpret_cast<const int16_t*>(item+off::item_anim_state)) {
    case 3: case 9: case 15: case 25: case 26: case 27: case 71: case 73: return true;
    default: return false;
    }
}
bool FullBodyIKReady() {
    if(!Cfg().firstPersonFullBodyIK || ControllerTrackingBlockedReason()) return false;
    const auto* item=*Ptr<uint8_t*>(g_boundDll->laraItem);
    const int status=*Ptr<int16_t>(g_boundDll->lara+2);
    if(!FullBodyState(item) || (status!=0 && !MotionReady()) ||
       (status==0 && (!Cfg().firstPersonUnarmedIK || CurrentMotionWeapon()<0 || CurrentMotionWeapon()>6))) return false;
    motiongun::Frame wrist{};GunBasis controller{};vr::HmdMatrix34_t head{};
    return VR().HeadPose(head) && BuildControllerWrist(0,wrist,controller) && BuildControllerWrist(1,wrist,controller);
}

bool UnarmedIKReady() {
    if (!Cfg().firstPersonUnarmedIK || ControllerTrackingBlockedReason()) return false;
    const auto* item=*Ptr<uint8_t*>(g_boundDll->laraItem);
    // Full-body free jumps retain controller hands; ledges retain authored grips.
    if (!item || !(CanTurnBody(item) || (Cfg().firstPersonFullBodyIK &&
        FullBodyState(item) && !FullBodyGripState(item))) || *Ptr<int16_t>(g_boundDll->lara+2)!=0 ||
        CurrentMotionWeapon()<0 || CurrentMotionWeapon()>6) return false;
    motiongun::Frame wrist{}; GunBasis controller{};
    return BuildControllerWrist(0,wrist,controller) && BuildControllerWrist(1,wrist,controller);
}

bool BuildGunPose(int hand, GunPose& out) {
    if (hand<0 || hand>1) return false;
    if (const auto* reason=MotionBlockedReason()) {
        g_gunPoseFailure[hand]=reason; return false;
    }
    auto rejected=[hand](const char* reason) {
        g_gunPoseFailure[hand]=reason; return false;
    };
    motiongun::Frame wrist{}; GunBasis controller{};
    if (!BuildControllerWrist(hand,wrist,controller)) return false;
    out.desired=wrist.basis; out.trackedHand=wrist.origin;
    const int16_t gun = *Ptr<int16_t>(g_boundDll->lara + 4);
    uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    const int joint = hand ? 10 : 13;
    out.nativeHand = JointPoint(item, joint, 0, 0, 0);
    const auto& body = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    if (std::fabs(out.nativeHand.x - body.x_pos) > 4096 ||
        std::fabs(out.nativeHand.y - body.y_pos) > 4096 ||
        std::fabs(out.nativeHand.z - body.z_pos) > 4096)
        return rejected("native-wrist-out-of-range");
    out.muzzle = Add(out.trackedHand,
        Transform(out.desired, motiongun::MuzzleLocal(gun, hand, g_boundDll->game)));
    if (!std::isfinite(out.muzzle.x) || !std::isfinite(out.muzzle.y) ||
        !std::isfinite(out.muzzle.z)) return rejected("nonfinite-muzzle");
    const GunVec ray{controller.r[0][2], controller.r[1][2],
                     controller.r[2][2]};
    out.direction = ray;
    const float flat = std::hypot(ray.x, ray.z);
    if (!std::isfinite(flat)) return rejected("nonfinite-barrel");
    out.yaw = Angle(std::atan2(ray.x, ray.z));
    out.pitch = Angle(std::atan2(-ray.y, flat));
    g_gunPoseFailure[hand]="ok";
    return true;
}

// The light projection is built before S_InitialisePolyList replaces the scene
// camera. Preserve its native matrix, then rebase only the receiver's origin.
bool g_shadowDepthScope=false, g_shadowReceiverPatched=false, g_loggedShadowReceiver=false;
mat4* g_shadowReceiver=nullptr;
mat4 g_nativeShadowReceiver{}, g_patchedShadowReceiver{};

void RestoreShadowReceiver() {
    if (g_shadowReceiver && g_shadowReceiverPatched &&
        !std::memcmp(g_shadowReceiver,&g_patchedShadowReceiver,sizeof(mat4))) {
        *g_shadowReceiver=g_nativeShadowReceiver;
        VidState().consts|=kShadow;
    }
    g_shadowReceiverPatched=false;
}

void __cdecl Detour_DrawToShadow(int32_t room) {
    RestoreShadowReceiver();g_shadowReceiver=nullptr;
    const bool previous=g_shadowDepthScope;g_shadowDepthScope=true;
    g_hDrawToShadow.Original<void (__cdecl*)(int32_t)>()(room);
    g_shadowDepthScope=previous;
    if (!previous && VidState().shadow) {
        g_shadowReceiver=VidState().shadow;
        g_nativeShadowReceiver=*g_shadowReceiver;
    }
}

void RebaseShadowReceiver(const PHD_3DPOS& native,const PHD_3DPOS& scene) {
    RestoreShadowReceiver();
    if (!g_shadowReceiver || !g_active || !g_scenePoseValid) return;
    const double delta[3]={double(scene.x_pos)-native.x_pos,
                           double(scene.y_pos)-native.y_pos,
                           double(scene.z_pos)-native.z_pos};
    g_patchedShadowReceiver=g_nativeShadowReceiver;
    // Column-major M * translate(C_firstPerson - C_native): for a world point
    // P, M_new*(P-C_firstPerson) == M_native*(P-C_native). No HMD rotation,
    // light-camera change, caster movement or per-eye offset belongs here.
    for (int row=0;row<4;++row) {
        double t=g_nativeShadowReceiver.m[12+row];
        for (int axis=0;axis<3;++axis) t+=g_nativeShadowReceiver.m[axis*4+row]*delta[axis];
        g_patchedShadowReceiver.m[12+row]=float(t);
    }
    *g_shadowReceiver=g_patchedShadowReceiver;g_shadowReceiverPatched=true;
    VidState().consts|=kShadow;
    if (!g_loggedShadowReceiver) {
        LogF("firstperson shadow: receiver origin native=(%d,%d,%d) FP=(%d,%d,%d); light camera isolated",
             native.x_pos,native.y_pos,native.z_pos,scene.x_pos,scene.y_pos,scene.z_pos);
        g_loggedShadowReceiver=true;
    }
}


bool DrawingNativeShadow() {
    // DrawCreatureHD's third argument selects joint interpolation, NOT the
    // shadow pass. DrawToShadow selects the DLL-wide gRenderPass=4 instead.
    return g_boundBase && g_shadowDll && *Ptr<int32_t>(g_shadowDll->renderPass)==4;
}

bool BodyVisualOffsetApplies(const uint8_t* item) {
    return g_active && g_scenePoseValid && g_boundDll && g_boundBase &&
        item && item==g_headingItem && item==*Ptr<uint8_t*>(g_boundDll->laraItem) &&
        CanTurnBody(item);
}

bool ReadIKJoint(const uint8_t* obj,const float* joints,int count,int bones,
                 const int32_t* mapping,const float* poses,int joint,motiongun::Frame& frame) {
    int pivot=-1;
    for(int i=0;i<count;++i) if((mapping ? mapping[i] : i)==joint) { pivot=i;break; }
    if(pivot<0) return false;
    motiongun::Frame inverseBind{},bind{};
    if(bones>0) inverseBind=motiongun::HdInverseBind(motiongun::ReadRows(poses+pivot*12));
    else {
        const auto* p=reinterpret_cast<const float*>(obj+1676)+pivot*16;
        for(int row=0;row<3;++row) for(int col=0;col<3;++col) inverseBind.basis.r[row][col]=p[col*4+row];
        inverseBind.origin={p[12],p[13],p[14]};
    }
    if(!motiongun::Inverse(inverseBind,bind)) return false;
    frame=motiongun::Multiply(motiongun::ReadRows(joints+pivot*12),bind);
    return firstperson::Finite(frame);
}

bool ApplyFullBodyIK(uint8_t* item,float* joints,int count) {
    using namespace motiongun;
    if(!FullBodyIKReady() || count<15 || count>33) return false;
    const int object=*reinterpret_cast<const int16_t*>(item+off::item_object);
    if(object<0) return false;
    const auto* obj=Ptr<uint8_t>(g_boundDll->objects)+object*off::object_stride;
    const auto* geom=obj+off::object_geom;
    const int bones=*reinterpret_cast<const int32_t*>(geom+28);
    const auto* mapping=bones>0 ? *reinterpret_cast<const int32_t* const*>(geom+48) : nullptr;
    const auto* poses=bones>0 ? *reinterpret_cast<const float* const*>(geom+72) : nullptr;
    if(bones>0 && (bones!=count || !mapping || !poses)) return false;
    Frame original[15]{},native[15]{},posed[15]{},correction[15]{};
    const Vec fit=BodyVisualOffsetApplies(item) ? Vec{g_bodyVisualOffset.x,0,g_bodyVisualOffset.z} : Vec{};
    for(int joint=0;joint<15;++joint) {
        if(!ReadIKJoint(obj,joints,count,bones,mapping,poses,joint,original[joint])) return false;
        native[joint]=original[joint];native[joint].origin=Sub(native[joint].origin,fit);
    }
    const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
    const Vec anchor=JointPoint(item,Cfg().firstPersonJoint,Cfg().firstPersonAnchorX,Cfg().firstPersonAnchorY,
        locomotion::FirstPersonAnchorZ(state,Cfg().firstPersonAnchorZ,Cfg().firstPersonInteractionAnchorZ));
    const float scale=LiveWorldUnitsPerMetre();
    locomotion::Vec view{};VR().FirstPersonViewOffset(view.x,view.z);
    view=locomotion::Rotate(view,g_headingBase)*scale;
    const Vec eye{float(g_scenePose.x_pos)+view.x,float(g_scenePose.y_pos)+VR().FirstPersonVerticalOffset()*scale,
                  float(g_scenePose.z_pos)+view.z};
    Vec headDelta=Sub(eye,anchor);
    if(!firstperson::Finite({firstperson::IdentityBasis(),headDelta}) || firstperson::Length(headDelta)>4096) return false;
    const auto& current=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    const auto& previous=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos_prev);
    const float fraction=std::clamp(*Ptr<int32_t>(g_boundDll->frameFrac),0,256)/256.f;
    const float nativeHeading=Radians(previous.y_rot)+Wrap(Radians(current.y_rot)-Radians(previous.y_rot))*fraction;
    const bool grip=FullBodyGripState(item),unarmed=*Ptr<int16_t>(g_boundDll->lara+2)==0;
    const float heading=grip ? nativeHeading : Wrap(g_headingBase+VR().HeadYawRadians());
    const Basis body{{{std::cos(heading),0,std::sin(heading)},{0,1,0},{-std::sin(heading),0,std::cos(heading)}}};
    Frame inverseBody{};Inverse({body,{}},inverseBody);
    vr::HmdMatrix34_t head{};if(!VR().HeadPose(head)) return false;
    const Basis headRotation=Multiply(ControllerBasis(head.m,g_headingBase),inverseBody.basis);
    const Vec forward{std::sin(heading),0,std::cos(heading)};
    if(grip) {
        if(!firstperson::SolveBody(native,headDelta,headRotation,forward,scale,posed)) return false;
    } else {
        const int joint=Cfg().firstPersonJoint;
        if(joint<0 || joint>=15) return false;
        // JointPoint is in world space; recovered palette joints are relative
        // to the native render camera. Convert both anchors through the same
        // native joint, after undoing the separate legacy body-fit translation.
        const Vec anchorInPalette=Add(native[joint].origin,Sub(anchor,JointPoint(item,joint,0,0,0)));
        if(!firstperson::SolveBodyAtEye(native,joint,anchorInPalette,Add(anchorInPalette,headDelta),
            Wrap(heading-nativeHeading),headRotation,forward,posed)) return false;
    }
    firstperson::ArmTwistState twist[2]={g_unarmedTwist[0],g_unarmedTwist[1]};
    for(int hand=0;hand<2;++hand) {
        const int first=hand ? 8 : 11;
        Frame chain[3]={posed[first],posed[first+1],posed[first+2]},changes[3]{},target=native[first+2];
        if(!grip) {
            GunBasis controller{};if(!BuildControllerWrist(hand,target,controller)) return false;
            const auto wrist=JointPoint(item,first+2,0,0,0);
            target.origin=Add(native[first+2].origin,Sub(target.origin,wrist));
        }
        const float side=hand ? 1.f : -1.f;
        Vec pole{side*.6f*std::cos(heading)-.25f*std::sin(heading),1.f,
                 -side*.6f*std::sin(heading)-.25f*std::cos(heading)};
        if(grip) pole=Sub(native[first+1].origin,native[first].origin);
        const auto neutral=GunBasis(body);
        if(!firstperson::SolveArm(chain,target,pole,changes,&twist[hand],&neutral,unarmed && !grip)) return false;
        for(int i=0;i<3;++i) posed[first+i]=Multiply(changes[i],chain[i]);
    }
    // Validate every correction before touching a palette. Helpers sharing a
    // native joint receive the same transform, including nonidentity bindings.
    for(int joint=0;joint<15;++joint) {
        Frame inverse{};if(!Inverse(original[joint],inverse)) return false;
        correction[joint]=Multiply(posed[joint],inverse);
        if(!firstperson::Finite(correction[joint])) return false;
    }
    for(int i=0;i<count;++i) {
        const int joint=mapping ? mapping[i] : i;
        if(joint>=0 && joint<15) WriteRows(Multiply(correction[joint],ReadRows(joints+i*12)),joints+i*12);
    }
    if(unarmed && !grip) { g_unarmedTwist[0]=twist[0];g_unarmedTwist[1]=twist[1]; }
    return true;
}

// Work only on the eye-view skinning palette. Native simulation joints and
// the separately captured physics palette retain the original animation.
bool ApplyUnarmedIK(uint8_t* item, float* joints, int count) {
    if (!UnarmedIKReady() || count<15 || count>33) return false;
    const int object=*reinterpret_cast<const int16_t*>(item+off::item_object);
    if (object<0) return false;
    const auto* obj=Ptr<uint8_t>(g_boundDll->objects)+object*off::object_stride;
    const auto* geom=obj+off::object_geom;
    const int bones=*reinterpret_cast<const int32_t*>(geom+28);
    const auto* mapping=bones>0 ? *reinterpret_cast<const int32_t* const*>(geom+48) : nullptr;
    const auto* poses=bones>0 ? *reinterpret_cast<const float* const*>(geom+72) : nullptr;
    if (bones>0 && (bones!=count || !mapping || !poses)) return false;
    motiongun::Frame correction[6]{};
    auto leftTwist=g_unarmedTwist[0],rightTwist=g_unarmedTwist[1];
    for (int hand=0;hand<2;++hand) {
        const int first=hand ? 8 : 11;
        motiongun::Frame native[3]{};
        for (int segment=0;segment<3;++segment)
            if(!ReadIKJoint(obj,joints,count,bones,mapping,poses,first+segment,native[segment])) return false;
        motiongun::Frame target{}; GunBasis controller{};
        if (!BuildControllerWrist(hand,target,controller)) return false;
        // Native palettes are camera-relative. Convert the shared gun world
        // target using the native wrist and undo the visible body's eye fit.
        const auto nativeWrist=JointPoint(item,first+2,0,0,0);
        const auto& body=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
        if (std::fabs(nativeWrist.x-body.x_pos)>4096 ||
            std::fabs(nativeWrist.y-body.y_pos)>4096 ||
            std::fabs(nativeWrist.z-body.z_pos)>4096) return false;
        target.origin=Add(native[2].origin,Sub(target.origin,nativeWrist));
        if (BodyVisualOffsetApplies(item))
            target.origin=Sub(target.origin,{g_bodyVisualOffset.x,0,g_bodyVisualOffset.z});
        const float heading=Radians(reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot);
        const float side=hand ? 1.f : -1.f;
        const GunVec pole{side*.6f*std::cos(heading)-.25f*std::sin(heading),
                          1.f,-side*.6f*std::sin(heading)-.25f*std::cos(heading)};
        const GunBasis bodyBasis{{{std::cos(heading),0,std::sin(heading)},
                                  {0,1,0},{-std::sin(heading),0,std::cos(heading)}}};
        const auto neutralWrist=motiongun::GunBasis(bodyBasis);
        if (!firstperson::SolveArm(native,target,pole,correction+hand*3,
                                   hand ? &rightTwist : &leftTwist,&neutralWrist)) return false;
    }
    g_unarmedTwist[0]=leftTwist;g_unarmedTwist[1]=rightTwist;
    // Commit only after both arms solve. Duplicate HD helper bones mapped to
    // a segment receive the same correction, preserving blended vertices.
    for (int i=0;i<count;++i) {
        const int joint=mapping ? mapping[i] : i;
        const int index=joint>=11 && joint<=13 ? joint-11 :
                        joint>=8 && joint<=10 ? joint-8+3 : -1;
        if (index>=0) motiongun::WriteRows(motiongun::Multiply(correction[index],
            motiongun::ReadRows(joints+i*12)),joints+i*12);
    }
    return true;
}

int32_t __cdecl Detour_GetJoints(uint8_t* item, float* joints, int32_t pass) {
    g_renderHandJoint = -1;
    g_bodySkinReady=false;
    const int32_t count = g_hGetJoints.Original<Fn_GetJoints>()(item, joints, pass);
    // Shadow positions must match native third person, independent of the
    // head-dependent body fit used only to keep geometry out of the eye view.
    // Also exclude shadow palettes from tracked-hand transforms and physics.
    if (DrawingNativeShadow()) return count;
    if (g_renderArm<0 && joints && count>0 && count<=33 && g_active &&
        g_boundDll && item && item==g_headingItem &&
        item==*Ptr<uint8_t*>(g_boundDll->laraItem)) {
        if (BodyVisualOffsetApplies(item))
            stabilization::OffsetBodyPalette(joints,count,g_bodyVisualOffset);
        const int object=*reinterpret_cast<const int16_t*>(item+off::item_object);
        if (g_bodySkinScope && object>=0) {
            const auto* geom=Ptr<uint8_t>(g_boundDll->objects)+object*off::object_stride+off::object_geom;
            const int bones=*reinterpret_cast<const int32_t*>(geom+28);
            const auto* mapping=bones>0 ? *reinterpret_cast<const int32_t* const*>(geom+48) : nullptr;
            if (bones<=0 || (bones==count && mapping)) {
                g_bodySkinMask=firstperson::SkinJointMask(
                    *reinterpret_cast<const uint32_t*>(item+off::item_mesh_bits),mapping,count);
                std::memcpy(g_bodySkinPalette,joints,count*12*sizeof(float));
                g_bodySkinCount=count; g_bodySkinReady=true;
            }
        }
        if(!ApplyFullBodyIK(item,joints,count)) ApplyUnarmedIK(item,joints,count);
        if (g_bodySkinReady) std::memcpy(g_bodyRenderPalette,joints,count*12*sizeof(float));
    }
    if (g_renderArm < 0 || count < 15 || !joints || !MotionReady() ||
        item != *Ptr<uint8_t*>(g_boundDll->laraItem)) return count;
    GunPose gun{};
    if (!BuildGunPose(g_renderArm, gun)) return count;
    const int nativeJoint = g_renderArm ? 10 : 13;
    const int object = *reinterpret_cast<const int16_t*>(item + off::item_object);
    if (object < 0) return count;
    const uint8_t* obj = Ptr<uint8_t>(g_boundDll->objects) +
        object * off::object_stride;
    const uint8_t* geom = obj + off::object_geom;
    const int bones = *reinterpret_cast<const int32_t*>(geom + 28);
    int pivot = nativeJoint;
    motiongun::Frame inverseBind{};
    if (bones > 0) {
        if (bones != count || count > 256) return count;
        const auto mapping = *reinterpret_cast<const int32_t* const*>(geom + 48);
        const auto poses = *reinterpret_cast<const float* const*>(geom + 72);
        if (!mapping || !poses) return count;
        pivot = -1;
        for (int i=0; i<count; ++i)
            if (mapping[i] == nativeJoint) { pivot=i; break; }
        if (pivot < 0) return count;
        inverseBind = motiongun::HdInverseBind(
            motiongun::ReadRows(poses + pivot*12));
    } else {
        if (count > 33 || pivot >= count) return count;
        // object_info::BindMatrix is column-major mat4[33].
        const float* bind = reinterpret_cast<const float*>(obj + 1676) + pivot*16;
        for (int row=0; row<3; ++row)
            for (int col=0; col<3; ++col)
                inverseBind.basis.r[row][col]=bind[col*4+row];
        inverseBind.origin={bind[12],bind[13],bind[14]};
    }
    const auto palette = motiongun::ReadRows(joints + pivot*12);
    motiongun::Frame bind{}, correction{};
    if (!motiongun::Inverse(inverseBind, bind)) return count;
    const auto wrist = motiongun::Multiply(palette, bind);
    const motiongun::Frame desired{gun.desired,
        Add(wrist.origin, Sub(gun.trackedHand, gun.nativeHand))};
    if (!motiongun::PaletteCorrection(palette, inverseBind, desired, correction))
        return count;
    if (g_renderArm==1 && g_boundDll->game==1 && CurrentMotionWeapon()==5) {
        if (g_hkScopeBindItem!=item)
            Log("HK scope: captured wrist binding from right-hand HK draw");
        g_hkScopeInverseBind=inverseBind;
        g_hkScopeBindItem=item;
    }
    // MaskJoints runs after us. Move the whole palette, including HD helper
    // bones, so weighted hand/gun vertices receive the same rigid transform.
    for (int joint=0; joint<count; ++joint) {
        float* bone=joints + joint*12;
        motiongun::WriteRows(motiongun::Multiply(correction,
            motiongun::ReadRows(bone)), bone);
    }
    ++g_motionArmDraws[g_renderArm];
    g_renderHandJoint = pivot;
    return count;
}

void __cdecl Detour_SetGunFlash(int32_t weapon, int32_t left) {
    const auto original = g_hSetGunFlash.Original<Fn_SetGunFlash>();
    const uint64_t caller = reinterpret_cast<uint64_t>(_ReturnAddress()) - g_boundBase;
    const int hand = g_motionDll && MotionWeaponSupported(weapon)
        ? (caller == g_motionDll->rightFlashReturn && !left ? 1 :
           caller == g_motionDll->leftFlashReturn && left == 1 ? 0 : -1) : -1;
    GunPose gun{};
    if (hand < 0 || !BuildGunPose(hand, gun)) { original(weapon,left); return; }
    float* fm = *Ptr<float*>(g_motionDll->floatMatrixPtr);
    int32_t* im = *Ptr<int32_t*>(g_boundDll->phdMxptr);
    if (!fm || !im) { original(weapon,left); return; }
    float savedFloat[12];
    int32_t savedInt[12];
    std::memcpy(savedFloat,fm,sizeof(savedFloat));
    std::memcpy(savedInt,im,sizeof(savedInt));
    const auto native = motiongun::ReadRows(fm);
    const motiongun::Frame desired{gun.desired,
        Add(native.origin, Sub(gun.trackedHand,gun.nativeHand))};
    motiongun::WriteRows(desired,fm);
    for (int i=0; i<12; ++i)
        im[i]=int32_t(std::lround(fm[i]*16384.0f));
    // Native SetGunFlash applies the same HD MuzzleLocal offset, flash spin
    // and weapon-specific orientation. Never touch enemy or mirror callers.
    original(weapon,left);
    std::memcpy(fm,savedFloat,sizeof(savedFloat));
    std::memcpy(im,savedInt,sizeof(savedInt));
}

bool AssistGunShot(const GunPose& gun, void* target, motiongun::Vec& direction) {
    // A magnified reticle must not silently steer onto an enemy off-axis.
    if (FirstPersonHKScopeAiming()) return false;
    if (!target || !g_headingItem || !g_motionDll ||
        *reinterpret_cast<const int16_t*>(static_cast<uint8_t*>(target)+off::item_hit_points)<=0)
        return false;
    using FindTarget = void (__cdecl*)(void*, ShotVector*);
    using LineOfSight = int32_t (__cdecl*)(ShotVector*, ShotVector*);
    using GetFloor = void* (__cdecl*)(int32_t,int32_t,int32_t,int16_t*);
    ShotVector end{};
    reinterpret_cast<FindTarget>(g_boundBase+g_motionDll->findTargetPoint)(target,&end);
    motiongun::Vec candidate{};
    if (!motiongun::AssistedDirection(gun.muzzle,gun.direction,
            {float(end.x),float(end.y),float(end.z)},candidate)) return false;
    ShotVector start{int32_t(std::lround(gun.muzzle.x)),
        int32_t(std::lround(gun.muzzle.y)),int32_t(std::lround(gun.muzzle.z)),
        *reinterpret_cast<const int16_t*>(g_headingItem+off::item_room),0};
    reinterpret_cast<GetFloor>(g_boundBase+g_boundDll->getFloor)(
        start.x,start.y,start.z,&start.room);
    if (!reinterpret_cast<LineOfSight>(g_boundBase+g_motionDll->lineOfSight)(&start,&end))
        return false;
    direction=candidate;
    return true;
}

uint8_t* SelectGunTarget(const GunPose& gun, motiongun::Vec& ray) {
    using namespace motiongun;
    if (!g_motionDll) return nullptr;
    const auto& d=*g_motionDll;
    if (!d.nextItemActive || !d.getSpheres || !d.findTargetPoint || !d.lineOfSight) return nullptr;
    auto* items=*Ptr<uint8_t*>(d.items);
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);
    if (!items || !lara) return nullptr;
    struct Sphere { int32_t x,y,z,r; };
    using Fn_Spheres=int32_t (__cdecl*)(uint8_t*,Sphere*,int32_t);
    using Fn_TargetPoint=void (__cdecl*)(uint8_t*,ShotVector*);
    using Fn_LOS=int32_t (__cdecl*)(ShotVector*,ShotVector*);
    using Fn_Floor=void* (__cdecl*)(int32_t,int32_t,int32_t,int16_t*);
    ShotVector source{int32_t(std::lround(gun.muzzle.x)),
                      int32_t(std::lround(gun.muzzle.y)),
                      int32_t(std::lround(gun.muzzle.z)),
                      *reinterpret_cast<int16_t*>(lara+off::item_room),0};
    reinterpret_cast<Fn_Floor>(g_boundBase+g_boundDll->getFloor)(source.x,source.y,source.z,&source.room);
    auto visible=[&](Vec point,int16_t room) {
        ShotVector start=source;
        ShotVector end{int32_t(std::lround(point.x)),int32_t(std::lround(point.y)),
                       int32_t(std::lround(point.z)),room,0};
        return reinterpret_cast<Fn_LOS>(g_boundBase+d.lineOfSight)(&start,&end)!=0;
    };
    uint8_t* direct=nullptr;
    uint8_t* assisted=nullptr;
    Vec assistRay{};
    float nearest=20480.f, bestCos=-1.f, bestDistance=8192.f*8192.f;
    const bool allowAssist=!FirstPersonHKScopeAiming();
    int index=*Ptr<int16_t>(d.nextItemActive);
    for (int visited=0; index>=0 && index<4096 && visited<4096; ++visited) {
        auto* item=items+index*off::item_stride;
        index=*reinterpret_cast<int16_t*>(item+off::item_next_active);
        if (item==lara || *reinterpret_cast<int16_t*>(item+off::item_hit_points)<=0) continue;
        const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
        const Vec delta=Sub({float(pos.x_pos),float(pos.y_pos),float(pos.z_pos)},gun.muzzle);
        if (Dot(delta,delta)>20480.f*20480.f) continue;
        const int object=*reinterpret_cast<int16_t*>(item+off::item_object);
        if (object<0) continue;
        const int meshes=*reinterpret_cast<int16_t*>(Ptr<uint8_t>(g_boundDll->objects)+object*off::object_stride);
        Sphere spheres[256]{};
        if (meshes<=0 || meshes>256) continue;
        const int count=reinterpret_cast<Fn_Spheres>(g_boundBase+d.getSpheres)(item,spheres,1);
        if (count<=0 || count>256) continue;
        ShotVector targetPoint{};
        reinterpret_cast<Fn_TargetPoint>(g_boundBase+d.findTargetPoint)(item,&targetPoint);
        const Vec nativePoint{float(targetPoint.x),float(targetPoint.y),float(targetPoint.z)};
        Vec bodyPoint{};
        float bodyDistance=std::numeric_limits<float>::max();
        for (int i=0;i<count;++i) {
            const auto& sphere=spheres[i];
            if (sphere.r<=0) continue;
            const Vec centre{float(sphere.x),float(sphere.y),float(sphere.z)};
            float distance=0;
            if (ShotSphere(gun.muzzle,gun.direction,centre,float(sphere.r),distance) &&
                distance<nearest && visible(Add(gun.muzzle,Scale(gun.direction,distance)),targetPoint.room)) {
                direct=item;
                nearest=distance;
            }
            // Pick a real hit sphere near the native animated body target,
            // rather than an arbitrary height above the creature's origin.
            const Vec offset=Sub(centre,nativePoint);
            const float distance2=Dot(offset,offset);
            if (distance2<bodyDistance) { bodyDistance=distance2; bodyPoint=centre; }
        }
        Vec candidate{};
        if (!allowAssist || bodyDistance==std::numeric_limits<float>::max() ||
            !AssistedDirection(gun.muzzle,gun.direction,bodyPoint,candidate)) continue;
        const float cosine=Dot(candidate,gun.direction);
        const Vec offset=Sub(bodyPoint,gun.muzzle);
        const float distance2=Dot(offset,offset);
        if ((cosine>bestCos || (cosine==bestCos && distance2<bestDistance)) &&
            visible(bodyPoint,targetPoint.room)) {
            bestCos=cosine; bestDistance=distance2;
            assisted=item; assistRay=candidate;
        }
    }
    if (direct) return direct; // Direct barrel hits take priority over assistance.
    if (assisted) ray=assistRay;
    return assisted;
}

int32_t FireWeaponForHand(int hand, int32_t weapon, void* target, void* extra,
                         const int16_t* aim) {
    const bool longShot=g_longShot && g_longShot->weapon==weapon && hand==1;
    const bool separateTriggers=!longShot && hand>=0 && g_gunTriggers.active && MotionTriggerMode();
    // Native return 0 skips ammo/hits, muzzle flash, shell, sound and shot stats.
    // Keep unknown callers and non-motion modes entirely native.
    if (separateTriggers && !g_gunTriggers.pending[hand]) return 0;
    GunPose gun{};
    const int previousHand = g_firingHand;
    bool assisted=false, tracked=false;
    int hpBefore=0;
    if (hand >= 0 && aim && (longShot ? (gun=g_longShot->gun,true) : BuildGunPose(hand, gun))) {
        if (separateTriggers) g_gunTriggers.Consume(hand);
        tracked=true;
        g_firingDirection=gun.direction;
        target=SelectGunTarget(gun,g_firingDirection);
        if (target) hpBefore=*reinterpret_cast<const int16_t*>(
            static_cast<uint8_t*>(target)+off::item_hit_points);
        g_firingHand = hand;
        assisted=motiongun::Dot(motiongun::Sub(g_firingDirection,gun.direction),
            motiongun::Sub(g_firingDirection,gun.direction))>1e-8f;
        // Shotgun randomises each pellet BEFORE FireWeapon. Subtract the
        // pre-volley aim, not that pellet's aim, to retain all native spread.
        g_firingAim[0] = longShot ? g_longShot->baseAim[0] : aim[0];
        g_firingAim[1] = longShot ? g_longShot->baseAim[1] : aim[1];
        g_firingPose.x_pos = int32_t(std::lround(gun.muzzle.x));
        g_firingPose.y_pos = int32_t(std::lround(gun.muzzle.y));
        g_firingPose.z_pos = int32_t(std::lround(gun.muzzle.z));
        g_firingPose.x_rot = gun.pitch;
        g_firingPose.y_rot = gun.yaw;
        if (assisted) {
            constexpr float angleUnits=32768.f/kPi;
            const auto ray=g_firingDirection;
            g_firingPose.y_rot=int16_t(std::lround(std::atan2(ray.x,ray.z)*angleUnits));
            g_firingPose.x_rot=int16_t(std::lround(std::atan2(-ray.y,
                std::sqrt(ray.x*ray.x+ray.z*ray.z))*angleUnits));
        }
        if (!g_motionShots[hand]) {
            LogF("firstperson: Touch %s muzzle=(%d,%d,%d) yaw=%d pitch=%d",
                 hand ? "right" : "left", g_firingPose.x_pos,
                 g_firingPose.y_pos, g_firingPose.z_pos,
                 int(g_firingPose.y_rot), int(g_firingPose.x_rot));
        }
        ++g_motionShots[hand];
    }
    // Do not turn a queued controller shot into a head-aimed shot on pose loss.
    if (separateTriggers && !tracked) return 0;
    const int32_t result = g_hFireWeapon.Original<Fn_FireWeapon>()(
        weapon, target, extra, aim);
    if (tracked && result) {
        g_handFired[hand]=true;
        if (longShot) g_longShot->fired=true;
        else VR().GunShotHaptic(hand);
    }
    if (tracked && g_motionShots[hand]<=100)
        LogF("firstperson: Touch %s shot target=%p assist=%d hp=%d->%d native=%d",
            hand ? "right" : "left",target,int(assisted),hpBefore,
            target ? int(*reinterpret_cast<const int16_t*>(
                static_cast<uint8_t*>(target)+off::item_hit_points)) : 0,result);
    g_firingHand = previousHand;
    return result;
}

// One trigger request owns the complete native firing operation, not one
// FireWeapon call (a shotgun operation contains six of those).
template<class Fire>
void RunLongShot(int weapon, bool projectile, Fire fire) {
    if (CurrentMotionWeapon()!=weapon || !MotionTriggerMode()) { fire(nullptr); return; }
    if (g_gunTriggers.active && !g_opticShot && !g_gunTriggers.pending[1]) return;
    LongShot shot{};
    shot.weapon=weapon; shot.projectile=projectile;
    if (!BuildGunPose(1,shot.gun)) return; // Never silently shoot from the head.
    if (g_gunTriggers.active && !g_opticShot) g_gunTriggers.Consume(1);
    auto* arm=Ptr<uint8_t>(g_boundDll->lara+off::lara_left_arm);
    int16_t saved[3]{};
    std::memcpy(saved,arm+off::arm_lock,sizeof(saved));
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(g_headingItem+off::item_pos);
    *reinterpret_cast<int16_t*>(arm+off::arm_lock)=1;
    *reinterpret_cast<int16_t*>(arm+off::arm_y_rot)=int16_t(shot.gun.yaw-body.y_rot);
    *reinterpret_cast<int16_t*>(arm+off::arm_x_rot)=
        int16_t(shot.gun.pitch-(projectile ? body.x_rot : 0));
    shot.baseAim[0]=shot.gun.yaw;
    shot.baseAim[1]=shot.gun.pitch;
    const auto* previous=g_longShot;
    g_longShot=&shot;
    fire(&shot);
    if (shot.fired) VR().GunShotHaptic(1);
    g_longShot=previous;
    std::memcpy(arm+off::arm_lock,saved,sizeof(saved));
}

void __cdecl Detour_FireShotgun() {
    RunLongShot(4,false,[](const LongShot*) {
        g_hFireShotgun.Original<void (__cdecl*)()>()();
    });
}

void __cdecl Detour_FireGrenade() {
    RunLongShot(5,true,[](const LongShot*) {
        g_hFireSpecial.Original<void (__cdecl*)()>()();
    });
}

void __cdecl Detour_FireHK(int32_t running) {
    RunLongShot(5,false,[&](const LongShot*) {
        g_hFireSpecial.Original<void (__cdecl*)(int32_t)>()(running);
    });
}

void __cdecl Detour_FireCrossbow(PHD_3DPOS* pose) {
    // TR5's launcher requires a valid native laser/grapple target. Its
    // non-optic null-pose branch is not a usable native firing path.
    if (!pose && g_boundDll && g_boundDll->game==1 && MotionTriggerMode()) {
        g_gunTriggers.Consume(1); // Don't carry an invalid attempt into later optic mode.
        return;
    }
    RunLongShot(6,true,[&](const LongShot* shot) {
        PHD_3DPOS tracked{};
        auto* argument=pose;
        if (shot && pose) {
            tracked=*pose;
            tracked.x_pos=int32_t(std::lround(shot->gun.muzzle.x));
            tracked.y_pos=int32_t(std::lround(shot->gun.muzzle.y));
            tracked.z_pos=int32_t(std::lround(shot->gun.muzzle.z));
            // The native optic LOS has already selected/quantised the valid
            // grapple anchor from our muzzle. Preserve its attachment angle.
            if (!g_opticShot) {
                tracked.x_rot=shot->gun.pitch;
                tracked.y_rot=shot->gun.yaw;
                tracked.z_rot=0;
            }
            argument=&tracked;
        }
        // Keep TR4's null-pose branch: it owns normal ammunition consumption.
        g_hFireCrossbow.Original<void (__cdecl*)(PHD_3DPOS*)>()(argument);
    });
}

void __cdecl Detour_GunJoint(uint8_t* item, PHD_VECTOR* point, int32_t joint) {
    if (g_longShot && item==g_headingItem && point && joint==10) {
        const auto& gun=g_longShot->gun;
        const auto p=Add(gun.trackedHand,Transform(gun.desired,
            {float(point->x),float(point->y),float(point->z)}));
        *point={int32_t(std::lround(p.x)),int32_t(std::lround(p.y)),int32_t(std::lround(p.z))};
        return;
    }
    g_hGunJoint.Original<void (__cdecl*)(uint8_t*,PHD_VECTOR*,int32_t)>()(item,point,joint);
}

void __cdecl Detour_InitialiseProjectile(int16_t index) {
    if (g_longShot && g_longShot->projectile && g_longGunDll && index>=0) {
        auto* items=*Ptr<uint8_t*>(g_longGunDll->items);
        if (items) {
            auto* item=items+size_t(index)*9416;
            const int object=*reinterpret_cast<int16_t*>(item+off::item_object);
            const int expected=g_boundDll->game==1 ? 0x158 :
                g_longShot->weapon==5 ? 0x16d : 0x168;
            if (object==expected) {
                g_longShot->fired=true;
                auto& pos=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
                const auto muzzle=g_longShot->gun.muzzle;
                if (g_boundDll->game==1) {
                    pos.x_pos=int32_t(std::lround(muzzle.x));
                    pos.y_pos=int32_t(std::lround(muzzle.y));
                    pos.z_pos=int32_t(std::lround(muzzle.z));
                } // TR4 already used the tracked joint and native floor guard.
                auto& room=*reinterpret_cast<int16_t*>(item+off::item_room);
                room=*reinterpret_cast<const int16_t*>(g_headingItem+off::item_room);
                using Floor=void* (__cdecl*)(int32_t,int32_t,int32_t,int16_t*);
                reinterpret_cast<Floor>(g_boundBase+g_boundDll->getFloor)(
                    pos.x_pos,pos.y_pos,pos.z_pos,&room);
                // InitialiseItem now links the projectile into the correct
                // room and seeds its previous position at the tracked muzzle.
            }
        }
    }
    g_hInitialiseProjectile.Original<void (__cdecl*)(int16_t)>()(index);
}

void __cdecl Detour_RifleHandler(int32_t weapon) {
    g_hRifleHandler.Original<Fn_PistolHandler>()(weapon);
    if (!MotionReady()) return;
    auto* lara=Ptr<uint8_t>(g_boundDll->lara);
    for (uint32_t offset=off::lara_head_y_rot;offset<=off::lara_torso_z_rot;offset+=2)
        *reinterpret_cast<int16_t*>(lara+offset)=0;
}

int32_t FireWeaponForCaller(uint64_t caller,int32_t weapon,void* target,void* extra,
                            const int16_t* aim) {
    // AnimatePistols skips its right call for the revolver/Desert Eagle (2).
    // Its shared left-arm call fires that RIGHT-hand gun, or the left pistol/Uzi.
    const int hand = g_longShot && g_longShot->weapon==weapon ? 1 :
        g_motionDll && (motiongun::DualWeapon(weapon) || weapon==motiongun::Revolver)
        ? (caller == g_motionDll->rightFireReturn && motiongun::DualWeapon(weapon) ? 1 :
           caller == g_motionDll->leftFireReturn ? (weapon==motiongun::Revolver ? 1 : 0) : -1) : -1;
    return FireWeaponForHand(hand,weapon,target,extra,aim);
}

int32_t __cdecl Detour_FireWeapon(int32_t weapon, void* target, void* extra,
                                  const int16_t* aim) {
    return FireWeaponForCaller(reinterpret_cast<uint64_t>(_ReturnAddress())-g_boundBase,
        weapon,target,extra,aim);
}

int32_t GetTargetOnLOSForCaller(uint64_t caller,PHD_VECTOR* source,PHD_VECTOR* dest,
                               int32_t flags,int32_t mode) {
    bool opticCaller=false;
    if (g_longGunHooksReady && g_longGunDll && g_firingHand<0)
        for (auto address:g_longGunDll->opticReturns) opticCaller|=caller==address;
    const int weapon=CurrentMotionWeapon();
    if (opticCaller && (weapon==motiongun::Revolver || weapon==5 || weapon==6) && MotionTriggerMode()) {
        GunPose gun{};
        if (!BuildGunPose(1,gun))
            return g_hGetTargetOnLOS.Original<Fn_GetTargetOnLOS>()(source,dest,flags,0);
        ShotVector start{int32_t(std::lround(gun.muzzle.x)),int32_t(std::lround(gun.muzzle.y)),
            int32_t(std::lround(gun.muzzle.z)),
            *reinterpret_cast<int16_t*>(g_headingItem+off::item_room),0};
        using Floor=void* (__cdecl*)(int32_t,int32_t,int32_t,int16_t*);
        reinterpret_cast<Floor>(g_boundBase+g_boundDll->getFloor)(start.x,start.y,start.z,&start.room);
        const auto endpoint=Add(gun.muzzle,motiongun::Scale(gun.direction,20000.f));
        ShotVector end{int32_t(std::lround(endpoint.x)),int32_t(std::lround(endpoint.y)),
            int32_t(std::lround(endpoint.z)),start.room,0};
        if (mode && g_gunTriggers.active) mode=g_gunTriggers.Consume(1) ? mode : 0;
        const bool previous=g_opticShot;
        g_opticShot=mode!=0;
        const int result=g_hGetTargetOnLOS.Original<Fn_GetTargetOnLOS>()(
            reinterpret_cast<PHD_VECTOR*>(&start),reinterpret_cast<PHD_VECTOR*>(&end),flags,mode);
        g_opticShot=previous;
        // Native binocular code validates ammo/cadence before passing fire
        // mode. Scoped revolver/HK shots bypass FireWeapon; a miss still fires.
        // Launchers report successful creation through RunLongShot instead.
        if (mode && (weapon==motiongun::Revolver || (g_boundDll->game==1 && weapon==5)))
            VR().GunShotHaptic(1);
        // Do not overwrite the scene camera, whose vectors the native caller
        // passes directly. The native reticle uses its own LOS result.
        return result;
    }
    if (source && dest && g_firingHand >= 0 && g_motionDll &&
        (caller == g_motionDll->hitLosReturn ||
         caller == g_motionDll->missLosReturn)) {
        // GenerateW2V already rebased BOTH native branches to the muzzle.
        // Preserve the sphere-hit segment and its native HitTarget fallback.
        // Only a miss gets the full-length spread-adjusted impact ray.
        const bool confirmedHit=caller==g_motionDll->hitLosReturn;
        const auto endpoint=motiongun::CollisionEndpoint(confirmedHit,
            {float(dest->x),float(dest->y),float(dest->z)},
            {float(source->x),float(source->y),float(source->z)},g_firingDirection);
        if (!confirmedHit) {
            dest->x=int32_t(std::lround(endpoint.x));
            dest->y=int32_t(std::lround(endpoint.y));
            dest->z=int32_t(std::lround(endpoint.z));
        }
        const PHD_VECTOR requested = *dest;
        const int32_t result =
            g_hGetTargetOnLOS.Original<Fn_GetTargetOnLOS>()(
                source, dest, flags, mode);
        if (g_motionShots[g_firingHand] == 1)
            LogF("firstperson: Touch %s LOS src=(%d,%d,%d) ray=(%d,%d,%d) result=%d sphere=%d",
                 g_firingHand ? "right" : "left",
                 source->x, source->y, source->z,
                 requested.x, requested.y, requested.z, result,int(confirmedHit));
        return result;
    }
    return g_hGetTargetOnLOS.Original<Fn_GetTargetOnLOS>()(
        source, dest, flags, mode);
}

int32_t __cdecl Detour_GetTargetOnLOS(PHD_VECTOR* source,PHD_VECTOR* dest,
                                     int32_t flags,int32_t mode) {
    return GetTargetOnLOSForCaller(reinterpret_cast<uint64_t>(_ReturnAddress())-g_boundBase,
        source,dest,flags,mode);
}

void __cdecl Detour_AnimatePistols(int32_t weapon) {
    const auto original=g_hAnimatePistols.Original<void (__cdecl*)(int32_t)>();
    if (!MotionReady() || !g_gunTriggers.WantsShot() ||
        weapon!=*Ptr<int16_t>(g_boundDll->lara+4)) {
        original(weapon); return;
    }
    auto& target=*Ptr<void*>(g_boundDll->lara+off::lara_target);
    static uint64_t nextLog=0;
    if (target && GetTickCount64()>=nextLog) {
        const int left=*Ptr<int16_t>(g_boundDll->lara+off::lara_left_arm+off::arm_lock);
        const int right=*Ptr<int16_t>(g_boundDll->lara+off::lara_right_arm+off::arm_lock);
        if (!left || !right) {
            LogF("firstperson: controller free fire bypasses native target lock=%d/%d pending=%d/%d",
                 left,right,int(g_gunTriggers.pending[0]),int(g_gunTriggers.pending[1]));
            nextLog=GetTickCount64()+5000;
        }
    }
    motiongun::ScopedControllerAim aim(target,true);
    original(weapon);
}

void __cdecl Detour_PistolHandler(int32_t weapon) {
    const bool separate=g_gunTriggers.active && MotionTriggerMode() && MotionReady();
    auto* lara=separate ? Ptr<uint8_t>(g_boundDll->lara) : nullptr;
    int16_t beforeFlash[2]{};
    if (separate) {
        for (int hand=0;hand<2;++hand)
            beforeFlash[hand]=*reinterpret_cast<int16_t*>(lara+
                (hand ? off::lara_right_arm : off::lara_left_arm)+20);
        g_handFired[0]=g_handFired[1]=false;
    }
    g_hPistolHandler.Original<Fn_PistolHandler>()(weapon);
    if (separate) {
        auto& left=*reinterpret_cast<int16_t*>(lara+off::lara_left_arm+20);
        auto& right=*reinterpret_cast<int16_t*>(lara+off::lara_right_arm+20);
        // Pistols (1) and Uzis (3) already write the matching arm's counter.
        // Only the revolver (2), handled by RifleHandler, uses the shared
        // left-arm firing branch with a right-hand flash.
        if (!g_handFired[0]) left=beforeFlash[0];
        if (!g_handFired[1]) right=beforeFlash[1];
    }
    if (!MotionReady()) return;
    // PistolHandler copies the averaged arm aim into Lara's torso and head.
    // The FP camera anchors to the animated head on the next render, so a
    // physical hand movement otherwise drags the entire view. Keep the arms'
    // independent aim, but leave torso/head neutral for the camera skeleton.
    lara = Ptr<uint8_t>(g_boundDll->lara);
    for (uint32_t offset = off::lara_head_y_rot;
         offset <= off::lara_torso_z_rot; offset += sizeof(int16_t))
        *reinterpret_cast<int16_t*>(lara + offset) = 0;
}

void __cdecl Detour_AimWeapon(void* weapon, uint8_t* arm) {
    g_hAimWeapon.Original<Fn_AimWeapon>()(weapon, arm);
    if (!g_active || !g_haveHeading || !Gate()) return;
    uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (item != g_headingItem) return;
    uint8_t* lara = Ptr<uint8_t>(g_boundDll->lara);
    if (arm != lara + off::lara_left_arm && arm != lara + off::lara_right_arm) return;
    // TR5 RICH3 grapple animation state 2 is a terminal raised loop with no
    // holster transition. AnimateShotgun enters it when arm.lock is forced,
    // even with no fire input. Preserve the engine's lock for this weapon;
    // controller rendering/optic shots do not require a persistent aim lock.
    const bool preserveNativeLock=g_boundDll->game==1 && CurrentMotionWeapon()==6;
    // Native rifles use left_arm as their shared aim state; the actual grip
    // and trigger belong to the right controller, including the revolver.
    const int hand = !motiongun::DualWeapon(CurrentMotionWeapon()) ? 1 :
        arm == lara + off::lara_left_arm ? 0 : 1;
    GunPose gun{};
    if (BuildGunPose(hand, gun)) {
        const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
        if (!preserveNativeLock) *reinterpret_cast<int16_t*>(arm + off::arm_lock) = 1;
        *reinterpret_cast<int16_t*>(arm + off::arm_y_rot) =
            Angle(Radians(gun.yaw) - Radians(pos.y_rot));
        *reinterpret_cast<int16_t*>(arm + off::arm_x_rot) = gun.pitch;
        *reinterpret_cast<int16_t*>(arm + off::arm_z_rot) = 0;
        return;
    }
    if (!Cfg().firstPersonHeadAim) return;
    if (Cfg().firstPersonMotionGuns && !g_headAimFallbacks[hand]++)
        ReportMotionState("head-aim-fallback");
    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    const int16_t yaw = Angle(g_headingBase + VR().HeadYawRadians() - Radians(pos.y_rot));
    const int16_t pitch = Angle(VR().HeadPitchRadians());
    // PistolHandler and RifleHandler both call here BEFORE animating/firing.
    // Their native code uses these angles for the visible arms, torso and shot
    // direction. Lock=1 prevents long guns from adding torso rotation twice.
    // Set both arms: the revolver path aims the left arm but fires the right.
    for (const uint32_t offset : { off::lara_left_arm, off::lara_right_arm }) {
        uint8_t* a = lara + offset;
        if (!preserveNativeLock) *reinterpret_cast<int16_t*>(a + off::arm_lock) = 1;
        *reinterpret_cast<int16_t*>(a + off::arm_y_rot) = yaw;
        *reinterpret_cast<int16_t*>(a + off::arm_x_rot) = pitch;
        *reinterpret_cast<int16_t*>(a + off::arm_z_rot) = 0;
    }
}

// COLL_INFO from both TR4/5 PDBs. Never reuse laracoll: its saved position
// and trigger pointer belong to the native simulation tick.
struct RoomCollision {
    int32_t floorSamples[18];
    int32_t radius, badPos, badNeg, badCeiling;
    int32_t shift[3], old[3];
    int16_t oldState, oldAnim, oldFrame, facing, quadrant, type;
    int16_t* trigger;
    uint8_t tiltX, tiltZ, hitBaddie, hitStatic;
    uint16_t flags;
};
static_assert(sizeof(RoomCollision) == 144);
static_assert(offsetof(RoomCollision, facing) == 118);
static_assert(offsetof(RoomCollision, trigger) == 128);
static_assert(offsetof(RoomCollision, flags) == 140);
using Fn_GetCollisionInfo = void(__cdecl*)(RoomCollision*, int32_t, int32_t,
                                          int32_t, int16_t, int32_t);
using Fn_GetFloor = void*(__cdecl*)(int32_t, int32_t, int32_t, int16_t*);
using Fn_GetHeight = int32_t(__cdecl*)(void*, int32_t, int32_t, int32_t);
using Fn_ItemNewRoom = void(__cdecl*)(int16_t, int16_t);

void UpdateLaraRoom(uint8_t* item) {
    // TR4/5 inline TR1-3's UpdateLaraRoom(item, -381) in LaraAboveWater.
    // Reproduce that exact sequence after each accepted room-scale sweep.
    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    int16_t room = *reinterpret_cast<int16_t*>(item + off::item_room);
    void* floor = reinterpret_cast<Fn_GetFloor>(g_boundBase + g_boundDll->getFloor)(
        pos.x_pos, pos.y_pos - 381, pos.z_pos, &room);
    *reinterpret_cast<int32_t*>(item + off::item_floor_prev) =
        *reinterpret_cast<int32_t*>(item + off::item_floor);
    *reinterpret_cast<int32_t*>(item + off::item_floor) =
        reinterpret_cast<Fn_GetHeight>(g_boundBase + g_boundDll->getHeight)(
            floor, pos.x_pos, pos.y_pos - 381, pos.z_pos);
    if (room != *reinterpret_cast<int16_t*>(item + off::item_room))
        reinterpret_cast<Fn_ItemNewRoom>(g_boundBase + g_boundDll->itemNewRoom)(
            *Ptr<int16_t>(g_boundDll->lara), room);
}

locomotion::Vec DragBody(uint8_t* item) {
    using namespace locomotion;
    if (!CanTurnBody(item) || g_shifted || !Cfg().firstPersonRoomscaleMove ||
        !Cfg().positionalTracking || !Cfg().firstPersonHeadTranslation) return {};
    if (!g_boundDll->getCollisionInfo || !g_boundDll->getFloor ||
        !g_boundDll->getHeight || !g_boundDll->itemNewRoom) return {};
    Vec pending;
    VR().HeadFloorOffset(pending.x, pending.z);
    const float scale = LiveWorldUnitsPerMetre();
    if (!std::isfinite(scale) || scale <= 1) return {};
    pending = pending - Rotate(g_dragCurrent - g_dragShown, -g_headingBase);
    const Vec requested = Rotate(DragRequest(pending,
        Cfg().firstPersonRoomscaleDeadzoneMetres), g_headingBase) * scale;
    const float distance = Length(requested);
    // Do not turn tracking loss/teleports into a physical walk across a level.
    if (!std::isfinite(distance) || distance < 1 || distance > scale * 2) return {};
    auto& pos = *reinterpret_cast<PHD_3DPOS*>(item + off::item_pos);
    const Vec initial{float(pos.x_pos), float(pos.z_pos)};
    const int count = std::clamp(static_cast<int>(std::ceil(distance / 32)), 1, 128);
    const Vec step = requested * (1.0f / count);
    Vec remainder{};
    for (int i = 0; i < count; ++i) {
        remainder = remainder + step;
        const int dx = static_cast<int>(std::round(remainder.x));
        const int dz = static_cast<int>(std::round(remainder.z));
        remainder = remainder - Vec{float(dx), float(dz)};
        RoomCollision coll{};
        coll.radius = 100;
        coll.badPos = 384; coll.badNeg = -384; coll.badCeiling = 0;
        coll.flags = 5; // slopes are walls, lava is a pit (native lara_col_stop)
        coll.old[0] = pos.x_pos; coll.old[1] = pos.y_pos; coll.old[2] = pos.z_pos;
        coll.facing = Angle(std::atan2(step.x, step.z));
        const int x = pos.x_pos + dx, z = pos.z_pos + dz;
        const int16_t room = *reinterpret_cast<int16_t*>(item + off::item_room);
        reinterpret_cast<Fn_GetCollisionInfo>(g_boundBase + g_boundDll->getCollisionInfo)(
            &coll, x, pos.y_pos, z, room, 762);
        if (coll.floorSamples[0] < -384 || coll.floorSamples[0] > 384 ||
            coll.floorSamples[1] >= 0 || coll.type == 8 || coll.type == 16 || coll.type == 32)
            break;
        const int acceptedX = x + coll.shift[0] - pos.x_pos;
        const int acceptedZ = z + coll.shift[2] - pos.z_pos;
        if (!AcceptCollisionDragStep({float(dx),float(dz)},
            {float(acceptedX),float(acceptedZ)})) break;
        pos.x_pos += acceptedX; pos.z_pos += acceptedZ;
        UpdateLaraRoom(item);
        if (!acceptedX && !acceptedZ) break;
    }
    const Vec actual = Vec{float(pos.x_pos), float(pos.z_pos)} - initial;
    g_dragCurrent = g_dragCurrent + actual * (1 / scale);
    return actual * (1 / scale);
}

// Catch the native turn180 animation command before input/body-follow can
// pull Lara back toward the old VR heading. Native code already turns Lara;
// rotate only the VR frame and pending movement, exactly once per half-turn.
void AdvanceLaraAnimation(uint8_t* item) {
    const bool observe=item && item==g_headingItem && g_active && g_haveHeading &&
        Gate() && LaraWaterStatus()==0;
    const int beforeState=observe ? *reinterpret_cast<const int16_t*>(item+off::item_anim_state) : -1;
    const int beforeAnimation=observe ? *reinterpret_cast<const int16_t*>(item+off::item_anim_number) : -1;
    const int16_t beforeYaw=observe ? reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot : 0;
    g_hAnimateLara.Original<Fn_AnimateLara>()(item);
    if (!observe || !Gate() || LaraWaterStatus()!=0) return;
    const int afterState=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
    const int afterAnimation=*reinterpret_cast<const int16_t*>(item+off::item_anim_number);
    const int16_t afterYaw=reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot;
    const float turn=locomotion::NativeRollTurn(beforeState,afterState,beforeAnimation,afterAnimation,beforeYaw,afterYaw);
    if (turn==0) return;
    g_headingBase=Wrap(g_headingBase+turn);
    VR().PivotHeadFloorOffset(turn);
    g_manualWorld=locomotion::Rotate(g_manualWorld,turn);
    g_lastHeadWorld=Wrap(g_headingBase+VR().HeadYawRadians());
    g_lastBodyYaw=Radians(afterYaw);
    auto* analog=Ptr<int16_t>(g_boundDll->analogInput);
    analog[2]=analog[3]=Angle(g_lastHeadWorld);
    g_rootMotion.Reset();
    g_renderTurn.Reset();
    LogF("firstperson: native roll turned VR heading 180 degrees state=%d->%d anim=%d->%d",
         beforeState,afterState,beforeAnimation,afterAnimation);
}

void AccelerateBackpedalStart(uint8_t* item,uint64_t action) {
    // Animation 41 holds a slow 2-unit step for 16 frames before the normal
    // 10-unit backward loop. Shorten that command-free startup, rather than
    // boosting persistent velocity or borrowing another gait's displacement.
    if ((action&locomotion::Directions)!=locomotion::Back || !g_boundDll->anims ||
        *reinterpret_cast<const int16_t*>(item+off::item_anim_state)!=16 ||
        *reinterpret_cast<const int16_t*>(item+off::item_goal_state)!=16 ||
        *reinterpret_cast<const int16_t*>(item+off::item_anim_number)!=41 ||
        *reinterpret_cast<const int16_t*>(item+22)!=0) return;
    const auto* anims=*Ptr<const uint8_t*>(g_boundDll->anims);
    if (!anims) return;
    const auto* start=anims+41*48;
    const int16_t first=*reinterpret_cast<const int16_t*>(start+28);
    const int16_t last=*reinterpret_cast<const int16_t*>(start+30);
    auto& frame=*reinterpret_cast<int16_t*>(item+off::item_anim_number+2);
    if (*reinterpret_cast<const int16_t*>(start+10)!=16 ||
        *reinterpret_cast<const int16_t*>(start+40)!=0 ||
        first<0 || last<=first || frame<first || frame>last) return;
    // Native AnimateLara still runs once, adds its own frame and performs the
    // normal transition to the backward loop, including its native velocity.
    frame=int16_t(std::min(int(frame)+3,int(last)));
}

// Match TR1-3: one animation tick, scaling only horizontal root displacement.
// Native collision still runs afterwards, and vertical/gravity motion is intact.
void __cdecl Detour_AnimateLara(uint8_t* item) {
    auto original = &AdvanceLaraAnimation;
    // Gravity/goal changes can precede current_anim_state. Check both sides
    // of AnimateLara: a grounded tick can execute a jump or relocation command.
    const bool groundMotion=item && item==g_headingItem && CanModifyGroundMotion(item);
    const uint64_t action=groundMotion ? g_groundMoveAction : 0;
    const bool stabilize=g_stabilizeRoot && groundMotion;
    const bool hardStop=g_hardStopRoot && groundMotion;
    if (!action && !stabilize && !hardStop) {
        if (g_stabilizeRoot) g_rootMotion.Reset();
        original(item);
        return;
    }
    auto& pos = *reinterpret_cast<PHD_3DPOS*>(item + off::item_pos);
    const int32_t oldX = pos.x_pos, oldZ = pos.z_pos;
    if (action) AccelerateBackpedalStart(item,action);
    original(item);
    const double distance=std::hypot(double(pos.x_pos)-oldX,double(pos.z_pos)-oldZ);
    if (!CanModifyGroundMotion(item) || distance>256) {
        // Leave native jumps, falls, interactions and authored relocations
        // untouched. Validate BEFORE applying the directional multiplier.
        g_rootMotion.Reset();
        return;
    }
    const bool changingGait=action && !locomotion::GroundGaitMatchesAction(
        *reinterpret_cast<const int16_t*>(item+off::item_anim_state),action);
    if (hardStop || changingGait) {
        // Keep the stopping animation, Y/gravity and subsequent collision.
        // Room-scale drag already happened BEFORE this snapshot, so only the
        // animation's residual horizontal movement is canceled, not real steps.
        pos.x_pos=oldX; pos.z_pos=oldZ;
        *reinterpret_cast<int16_t*>(item+off::item_speed)=0;
        g_rootMotion.Reset();
        return;
    }
    // Input may ask for side/back while the native run is still stopping.
    // Only the resulting, matching gait supplies the directional velocity.
    // Cancel outgoing movement above, before its collision routine runs, and
    // start the new gait on the very tick AnimateLara transitions into it.
    const int scale=locomotion::DirectionalRootScale(action,false);
    if (distance*scale>256) { g_rootMotion.Reset(); return; }
    pos.x_pos = oldX + (pos.x_pos - oldX) * scale;
    pos.z_pos = oldZ + (pos.z_pos - oldZ) * scale;
    // Scale this displacement only. Native gravity animation reuses item_speed
    // from the previous tick; writing our boosted/filtered speed back can feed
    // it into a fall and multiply it again while a ground state is still set.
    // Native animation has run exactly once; its collision routine has NOT.
    // Never straighten a collision-resolved position or touch vertical motion.
    if (stabilize) {
        locomotion::Vec step;
        if (g_rootMotion.Step({float(pos.x_pos-oldX),float(pos.z_pos-oldZ)},
            g_stabilizeDirection,*reinterpret_cast<int16_t*>(item+off::item_anim_state),
            g_stabilizeAction,step)) {
            pos.x_pos=oldX+int32_t(step.x); pos.z_pos=oldZ+int32_t(step.z);
        }
    }
}

void PrepareGroundDirection(uint8_t* item,uint64_t action) {
    using namespace locomotion;
    // Shorten ordinary forward/side/back gait handoffs in both directions.
    // Without Forward here, every sidestep -> forward change waits for a stop.
    // Native lara_as_stop performs its floor/ceiling checks and chooses the
    // target animation itself.
    // Never splice a landing, step-up/down, jump or interaction animation.
    if (!g_boundDll->anims || !CanModifyGroundMotion(item) ||
        (*Ptr<uint64_t>(g_boundDll->input)&(0x10|0x40|0x100|0x1000)) ||
        *reinterpret_cast<const int16_t*>(item+22)!=0) return; // required state
    const auto direction=action&Directions;
    if (direction!=Forward && direction!=StepLeft && direction!=StepRight && direction!=Back) return;
    const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
    const int animation=*reinterpret_cast<const int16_t*>(item+off::item_anim_number);
    const int goal=*reinterpret_cast<const int16_t*>(item+off::item_goal_state);
    const bool stopping=animation==38 || animation==39 || animation==66 || animation==68;
    // Run <-> sprint is still forward travel. Do not reset to standing while
    // native control is entering/exiting sprint or it can never reach full speed.
    if (GroundGaitMatchesAction(state,action) && !stopping && GroundGaitMatchesAction(goal,action)) return;
    const bool ordinary=(state==1 && (animation==0 || animation==6 || animation==8 || animation==10)) ||
        (state==0 && ((animation>=1 && animation<=5) || animation==7 || animation==9 || animation==20 || animation==21)) ||
        (state==2 && (animation==11 || animation==103)) ||
        (state==73 && animation>=223 && animation<=225) || // native sprint loop/startups
        (state==16 && animation>=38 && animation<=41) ||
        (state==22 && (animation==65 || animation==66)) ||
        (state==21 && (animation==67 || animation==68));
    if (!ordinary) return;
    const auto* anims=*Ptr<const uint8_t*>(g_boundDll->anims);
    if (!anims) return;
    const auto* idle=anims+11*48;
    const int16_t first=*reinterpret_cast<const int16_t*>(idle+28);
    const int16_t last=*reinterpret_cast<const int16_t*>(idle+30);
    if (*reinterpret_cast<const int16_t*>(idle+10)!=2 || first<0 || last<=first) return;
    *reinterpret_cast<int16_t*>(item+off::item_anim_number)=11;
    *reinterpret_cast<int16_t*>(item+off::item_anim_number+2)=first;
    *reinterpret_cast<int16_t*>(item+off::item_anim_state)=2;
    *reinterpret_cast<int16_t*>(item+off::item_goal_state)=2;
    *reinterpret_cast<int16_t*>(item+off::item_speed)=0;
    g_rootMotion.Reset();
}

bool UpdateLedgePull(uint8_t* item, uint64_t now) {
    // IsLedgeHangState is a visibility group which also includes monkey bars.
    // Native TR4/5 lara_col_hang mounts from AS_HANG (10), animation 96 only.
    bool eligible=item && item==g_headingItem && g_active && g_haveHeading && Gate() &&
        Cfg().gamepadEnabled && (g_boundDll->game==0 || g_boundDll->game==1) &&
        item==*Ptr<uint8_t*>(g_boundDll->laraItem) && LaraWaterStatus()==0 &&
        *reinterpret_cast<int16_t*>(item+off::item_anim_state)==10 &&
        *reinterpret_cast<int16_t*>(item+off::item_anim_number)==96 &&
        !(*reinterpret_cast<uint32_t*>(item+0x1820)&8);
    if (eligible) {
        // Explicit stick movement, jump, roll or native modern Drop wins.
        eligible=(*Ptr<uint64_t>(g_boundDll->input)&
            (locomotion::Directions|0x10|0x100|0x1000))==0;
    }
    vr::HmdMatrix34_t head{},left{},right{};
    eligible=eligible && VR().HeadPose(head) && VR().ControllerPose(0,left) && VR().ControllerPose(1,right);
    const bool wasLatched=g_ledgePull.latched;
    const bool pull=g_ledgePull.Update(eligible,now,head.m[1][3],left.m[1][3],right.m[1][3]);
    if (pull && !wasLatched) Log("firstperson: two-hand ledge pull requested");
    return pull;
}

void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {
    using namespace locomotion;
    g_dragPrevious = g_dragCurrent;
    g_groundMoveAction = 0;
    g_stabilizeRoot=false;
    g_hardStopRoot=false;
    if (item && item == g_headingItem && g_active && g_haveHeading && Gate()) {
        const int state = *reinterpret_cast<int16_t*>(item + off::item_anim_state);
        const bool ground = CanTurnBody(item);
        const bool jump = IsJumpSteeringState(state) && LaraWaterStatus() == 0 &&
            *reinterpret_cast<int16_t*>(item + off::item_hit_points) > 0;
        if (CanSteerMonkeyBars(item) && g_haveManualInput && !g_shifted && Length(g_manualLocal)>0) {
            // Native monkey controls read camTurn even though the rendered VR
            // camera is decoupled from it. Rebuild their inputs AFTER native
            // conversion, from the original LS intent and current VR heading.
            const float head=Wrap(g_headingBase+VR().HeadYawRadians());
            const Vec world=Cfg().firstPersonMoveWithHead ? Rotate(g_manualLocal,head) : g_manualWorld;
            auto* analog=Ptr<int16_t>(g_boundDll->analogInput);
            auto& input=*Ptr<uint64_t>(g_boundDll->input);
            const float magnitude=std::min(32767.f,std::hypot(float(analog[0]),float(analog[1])));
            const Vec decoded=SimulationStick(world,head,magnitude);
            analog[0]=int16_t(std::round(decoded.x));analog[1]=int16_t(std::round(decoded.z));
            analog[2]=analog[3]=Angle(head);
            const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
            // Modern hang2/swing use Forward to request traversal and steer
            // toward atan2(axes)+camTurn. Tank mode selects native body-local
            // traverse/half-turn animations; StepLeft/Right are not turn bits.
            const uint64_t action=NewControls() ? Forward :
                (MovementAction(Rotate(world,-Radians(pos.y_rot)))&Directions);
            input=(input&~Directions)|action;
            // Do not set Lara yaw, move_angle, speed or ground gait: her native
            // animation and ceiling collision retain ownership of the grip.
        }
        if ((ground || jump) && g_haveManualInput) {
            const float head = Wrap(g_headingBase + VR().HeadYawRadians());
            auto* analog = Ptr<int16_t>(g_boundDll->analogInput);
            auto& input = *Ptr<uint64_t>(g_boundDll->input);
            // The LS deadzone has already produced zero manual input. Don't
            // brake keyboard/D-pad movement, shifted controls, jumps or rolls.
            g_hardStopRoot=ground && CanModifyGroundMotion(item) && !g_shifted && !g_jumpPressed &&
                Length(g_manualLocal)<=0.0001f && !(input & (Directions | 0x10 | 0x100));
            // Runs after native input conversion. All subsequent rotation,
            // animation and collision now agree on a single HMD/world heading.
            analog[2] = analog[3] = Angle(head);
            if (Length(g_manualWorld) > 0 && !g_shifted) {
                const Vec directionalWorld = MovementWorld(g_manualLocal, head);
                const float magnitude = std::min(32767.0f, std::hypot(
                    static_cast<float>(analog[0]), static_cast<float>(analog[1])));
                if (magnitude > 0) {
                    const Vec steeringWorld = (ground || state == 15)
                        ? directionalWorld : g_manualWorld;
                    const Vec decoded = SimulationStick(steeringWorld, head, magnitude);
                    analog[0] = static_cast<int16_t>(std::round(decoded.x));
                    analog[1] = static_cast<int16_t>(std::round(decoded.z));
                }
                if (ground || state == 15) {
                    const bool preparingJump = (ground && g_jumpPressed) || state == 15;
                    const uint64_t action = MovementAction(g_manualLocal, preparingJump);
                    auto& pos = *reinterpret_cast<PHD_3DPOS*>(item + off::item_pos);
                    pos.y_rot = Angle(head);
                    *Ptr<int16_t>(g_boundDll->lara + off::lara_turn_rate) = 0;
                    // AnimateLara consumes this BEFORE the collision routine
                    // sets it. Without it, side/back gaits step forward each tick.
                    *Ptr<int16_t>(g_boundDll->lara + off::lara_move_angle) =
                        Angle(MovementYaw(g_manualLocal, head));
                    input = (input & ~Directions) | action;
                    if (ground && !preparingJump)
                        g_groundMoveAction = action;
                    if (ground && !preparingJump && Cfg().firstPersonMovementStabilization) {
                        g_stabilizeRoot=true;
                        g_stabilizeDirection=directionalWorld;
                        g_stabilizeAction=action;
                    }
                }
            }
        }
    }
    if (!g_stabilizeRoot) g_rootMotion.Reset();
    // Physical steps never enter XInput or advance the walk animation.
    if (item && item == g_headingItem && g_active && g_haveHeading && Gate()) {
        const Vec dragged = DragBody(item);
        if (Cfg().firstPersonDriftLog) {
            static uint64_t last = 0;
            static int lastState = -1;
            const uint64_t now = GetTickCount64();
            const int state = *reinterpret_cast<int16_t*>(item + off::item_anim_state);
            if (now - last >= 500 || state != lastState) {
                Vec pending; VR().HeadFloorOffset(pending.x, pending.z);
                const auto& pos = *reinterpret_cast<PHD_3DPOS*>(item + off::item_pos);
                LogF("locomotion: state=%d head=%.1f body=%.1f manual=(%+.2f,%+.2f) "
                     "pending=(%+.3f,%+.3f)m drag=(%+.3f,%+.3f)m input=%016llX",
                     state, Wrap(g_headingBase + VR().HeadYawRadians()) * 180 / kPi,
                     Radians(pos.y_rot) * 180 / kPi, g_manualWorld.x, g_manualWorld.z,
                     pending.x, pending.z, dragged.x, dragged.z,
                     static_cast<unsigned long long>(*Ptr<uint64_t>(g_boundDll->input)));
                last = now;
            }
            lastState = state;
        }
    }
    if (g_groundMoveAction) PrepareGroundDirection(item,g_groundMoveAction);
    const bool ledgePull=UpdateLedgePull(item,GetTickCount64());
    uint64_t savedLedgeInput=0;
    int16_t savedLedgeHeading=0;
    if (ledgePull) {
        auto& input=*Ptr<uint64_t>(g_boundDll->input);
        savedLedgeInput=input;
        input|=locomotion::Forward|0x40; // native Forward + Action/grab
        auto* analog=Ptr<int16_t>(g_boundDll->analogInput);
        savedLedgeHeading=analog[2];
        // lara_as_hang remaps Forward by camTurn in modern controls. Make
        // this request face the ledge regardless of HMD/chase-camera yaw.
        analog[2]=reinterpret_cast<PHD_3DPOS*>(item+off::item_pos)->y_rot;
    }
    g_hLaraAboveWater.Original<Fn_LaraAboveWater>()(item, nativeCollision);
    if (ledgePull) {
        // No synthetic run/Action or camera heading leaks to later handlers.
        auto& input=*Ptr<uint64_t>(g_boundDll->input);
        constexpr uint64_t owned=locomotion::Directions|0x40;
        input=(input&~owned)|(savedLedgeInput&owned);
        Ptr<int16_t>(g_boundDll->analogInput)[2]=savedLedgeHeading;
    }
    g_groundMoveAction = 0;
    g_stabilizeRoot=false;
    g_hardStopRoot=false;
}

void UpdateLocomotion(PHD_3DPOS& pose) {
    using namespace locomotion;
    auto* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    const auto& prev = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos_prev);
    const float t = std::clamp(*Ptr<int32_t>(g_boundDll->frameFrac), 0, 256) / 256.0f;
    const Vec body{prev.x_pos + (pos.x_pos - prev.x_pos) * t,
                   prev.z_pos + (pos.z_pos - prev.z_pos) * t};
    const bool relocated = Length(body - g_previousBody) >
        std::max(1024.0f, LiveWorldUnitsPerMetre() * 2);
    const int level=AppState(drva::app_off::level);
    const bool sameBody=g_headingItem==item && g_headingLevel==level;
    if (g_haveHeading && sameBody && !relocated) {
        // Consume only body drag actually visible at this render fraction.
        // Stick/animation movement must never cancel genuine physical leaning.
        const Vec shown = g_dragPrevious + (g_dragCurrent - g_dragPrevious) * t;
        const Vec used = Rotate(shown - g_dragShown, -g_headingBase);
        VR().ConsumeHeadFloorOffset(used.x, used.z);
        g_dragShown = shown;
    }
    if (!g_haveHeading || !sameBody || relocated) {
        g_ledgePull.Reset();
        g_mountBodyTransition.Reset();
        g_renderTurn.Reset(); g_turnTrace={}; g_rootMotion.Reset();
        const float oldBase=g_headingBase;
        const float bodyTurn=sameBody ? Wrap(Radians(pos.y_rot)-g_lastBodyYaw) : 0;
        const float facing = sameBody && !relocated
            ? Wrap(g_lastHeadWorld+bodyTurn) : Radians(pos.y_rot);
        g_headingBase = Wrap(facing - VR().HeadYawRadians());
        g_groundEye.Resume(sameBody,oldBase,g_headingBase,bodyTurn);
        g_headingLevel=level;
        g_cameraMotionTrace = {};
        g_haveHeading = true;
        g_headingItem = item;
        g_haveManualInput = false;
        g_dragPrevious = g_dragCurrent = g_dragShown = {};
        g_bodyTime = {};
        VR().RecenterFirstPersonHead();
    }
    g_previousBody = body;
    // One shared heading for this rendered camera, body following, culling,
    // eyes and tracked guns. Controller polling never advances it separately.
    AdvanceStickTurn(TurnTime());
    if (!CanTurnBody(item) && !Cfg().firstPersonMovementStabilization) {
        // Legacy animated-camera compensation only. With a stable/raw-tracked
        // view, consuming neck-corrected displacement while hanging invents a
        // head offset and carries it into the next standing frame.
        Vec pending; VR().HeadFloorOffset(pending.x, pending.z);
        VR().ConsumeHeadFloorOffset(pending.x, pending.z);
    }
    TurnBodyToHead(item, Elapsed(g_bodyTime));
    g_lastBodyYaw=Radians(pos.y_rot);
    pose.y_rot = Angle(g_headingBase);
    g_lastHeadWorld = Wrap(g_headingBase + VR().HeadYawRadians());
}

void ClampRenderedHeadToCollision(const uint8_t* item, PHD_3DPOS& pose) {
    if (!g_boundDll->getCollisionInfo) return;
    const bool ground = CanTurnBody(item);
    const int state = *reinterpret_cast<const int16_t*>(item + off::item_anim_state);
    // Ground rolls also need a sweep around their stable eye. Interactions
    // use the retracted anchor instead, and third person never reaches here.
    const bool jump = LaraWaterStatus() == 0 &&
        *reinterpret_cast<const int16_t*>(item + off::item_hit_points) > 0 &&
        (state == 3 || state == 9 || state == 12 || state == 15 ||
         (state >= 25 && state <= 29));
    const bool roll=locomotion::IsGroundRollState(state) && LaraWaterStatus()==0 &&
        *reinterpret_cast<const int16_t*>(item+off::item_hit_points)>0;
    if (!ground && !jump && !roll) return;

    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    const auto& prev = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos_prev);
    const int frac = std::clamp(*Ptr<int32_t>(g_boundDll->frameFrac), 0, 256);
    const auto lerp = [frac](int32_t a, int32_t b) {
        return int32_t(int64_t(a) + (int64_t(b) - a) * frac / 256);
    };
    const int32_t body[3] = {lerp(prev.x_pos, pos.x_pos),
                             lerp(prev.y_pos, pos.y_pos),
                             lerp(prev.z_pos, pos.z_pos)};

    locomotion::Vec tracked{};
    if (Cfg().positionalTracking && Cfg().firstPersonHeadTranslation && !VR().headAtCamera()) {
        VR().FirstPersonViewOffset(tracked.x, tracked.z);
        tracked = locomotion::Rotate(tracked, g_headingBase) * LiveWorldUnitsPerMetre();
    }
    if (!std::isfinite(tracked.x) || !std::isfinite(tracked.z) ||
        std::fabs(tracked.x) > 4096 || std::fabs(tracked.z) > 4096) return;
    const int32_t offsetX = int32_t(std::lround(tracked.x));
    const int32_t offsetZ = int32_t(std::lround(tracked.z));
    const int64_t eyeX = int64_t(pose.x_pos) + offsetX;
    const int64_t eyeZ = int64_t(pose.z_pos) + offsetZ;
    if (eyeX < INT32_MIN || eyeX > INT32_MAX ||
        eyeZ < INT32_MIN || eyeZ > INT32_MAX) return;
    int32_t eye[3] = {int32_t(eyeX), pose.y_pos, int32_t(eyeZ)};

    const int16_t room = *reinterpret_cast<const int16_t*>(item + off::item_room);
    double eyeY=pose.y_pos;
    if (Cfg().positionalTracking && Cfg().firstPersonHeadTranslation && !VR().headAtCamera())
        eyeY+=VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre();
    if (!std::isfinite(eyeY) || std::fabs(eyeY-pose.y_pos)>4096) return;
    firstperson::ClampEyeToWall(body, eye,
        [&](int32_t x, int32_t z, int32_t clearX, int32_t clearZ) {
            RoomCollision coll{};
            coll.radius = 64;
            coll.badPos = 4096;
            coll.badNeg = -4096;
            coll.badCeiling = 0;
            coll.flags = 0; // Eye clearance permits empty space beyond a ledge.
            coll.old[0] = clearX; coll.old[1] = body[1]; coll.old[2] = clearZ;
            coll.facing = Angle(std::atan2(float(x - clearX), float(z - clearZ)));
            reinterpret_cast<Fn_GetCollisionInfo>(g_boundBase + g_boundDll->getCollisionInfo)(
                &coll, x, body[1], z, room, 762);
            return firstperson::EyeBlocked(coll.floorSamples,body[1],eyeY,coll.hitStatic!=0) ||
                coll.type == 8 || coll.type == 16 || coll.type == 32 ||
                coll.shift[0] || coll.shift[2];
        });
    // Stereo adds the physical translation after the scene pose. Undo that
    // amount here so both rendered eyes remain on the clear side of the wall.
    pose.x_pos = eye[0] - offsetX;
    pose.z_pos = eye[2] - offsetZ;
    if (roll || UseLandingCamera(item)) {
        // The rendered headset can be lower than the scene anchor (ducking).
        // Query the final horizontal eye, then keep that rendered centre clear
        // of floor/ceiling without changing Lara, tracking neutral or calibration.
        RoomCollision coll{};
        coll.radius=64; coll.badPos=4096; coll.badNeg=-4096;
        coll.old[0]=eye[0]; coll.old[1]=body[1]; coll.old[2]=eye[2];
        coll.facing=Angle(g_lastHeadWorld);
        reinterpret_cast<Fn_GetCollisionInfo>(g_boundBase+g_boundDll->getCollisionInfo)(
            &coll,eye[0],body[1],eye[2],room,762);
        if (coll.floorSamples[0]!=-32512 && coll.floorSamples[1]!=-32512) {
            const double floor=double(body[1])+coll.floorSamples[0];
            const double ceiling=double(body[1])-762+coll.floorSamples[1];
            if (floor>ceiling) {
                const double margin=std::min(64.0,(floor-ceiling)*0.5);
                const double corrected=std::clamp(eyeY,ceiling+margin,floor-margin);
                const double anchor=pose.y_pos+corrected-eyeY;
                if (anchor>=INT32_MIN && anchor<=INT32_MAX)
                    pose.y_pos=int32_t(std::lround(anchor));
            }
        }
    }
}

// Diagnostic only: distinguish animated lateral sway from actual root motion
// and physical translation. Extrema over a full second avoid aliasing a gait
// cycle with a slow periodic log sample. Never modify the camera or input here.
void TraceForwardCamera(const uint8_t* item, const float body[3],
                        const PHD_VECTOR& head, int frac, uint64_t now,
                        const PHD_3DPOS* rendered = nullptr) {
    const int state = *reinterpret_cast<const int16_t*>(item + off::item_anim_state);
    if (!Cfg().firstPersonDriftLog || !g_haveManualInput || g_shifted ||
        g_manualLocal.z <= 0.2f || std::fabs(g_manualLocal.x) > g_manualLocal.z ||
        (state != 0 && state != 1)) {
        g_cameraMotionTrace = {};
        return;
    }
    using namespace locomotion;
    auto& trace = g_cameraMotionTrace;
    const bool first = trace.samples == 0;
    if (first) trace.start = now;
    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    const auto& prev = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos_prev);
    const float bodyYaw = Radians(prev.y_rot) +
        Wrap(Radians(pos.y_rot) - Radians(prev.y_rot)) * (frac / 256.0f);
    const float headYaw = Wrap(g_headingBase + VR().HeadYawRadians());
    const Vec bodyWorld{body[0], body[2]};
    const Vec headWorld{float(head.x), float(head.z)};
    Vec pending; VR().FirstPersonViewOffset(pending.x, pending.z);
    if (!Cfg().firstPersonHeadTranslation || !Cfg().positionalTracking) pending={};
    const Vec cameraWorld = rendered ? Vec{float(rendered->x_pos),float(rendered->z_pos)} : headWorld;
    const Vec eyeWorld = cameraWorld + Rotate(pending, g_headingBase) * LiveWorldUnitsPerMetre();
    trace.animatedSide.Add(Rotate(headWorld - bodyWorld, -bodyYaw).x, first);
    trace.trackedSide.Add(Rotate(pending, -VR().HeadYawRadians()).x, first);
    trace.yawError.Add(Wrap(headYaw - bodyYaw) * 180 / kPi, first);
    trace.rootSideStep.Add(first ? 0 : Rotate(bodyWorld - trace.previousBody, -headYaw).x, first);
    trace.eyeSideStep.Add(first ? 0 : Rotate(eyeWorld - trace.previousEye, -headYaw).x, first);
    trace.fracMin = std::min(trace.fracMin, frac);
    trace.fracMax = std::max(trace.fracMax, frac);
    trace.previousBody = bodyWorld; trace.previousEye = eyeWorld;
    ++trace.samples;
    if (now - trace.start >= 1000) {
        LogF("fp-forward: game=%d state=%d samples=%u elapsed=%llums frac=%d..%d "
             "animSide=%.2f..%.2fu rootSideStep=%.2f..%.2fu "
             "eyeSideStep=%.2f..%.2fu trackedSide=%.4f..%.4fm bodyYawError=%.2f..%.2fdeg",
             g_boundDll->game + 4, state, trace.samples,
             static_cast<unsigned long long>(now - trace.start), trace.fracMin, trace.fracMax,
             trace.animatedSide.low, trace.animatedSide.high,
             trace.rootSideStep.low, trace.rootSideStep.high,
             trace.eyeSideStep.low, trace.eyeSideStep.high,
             trace.trackedSide.low, trace.trackedSide.high,
             trace.yawError.low, trace.yawError.high);
        trace = {};
    }
}

void FitBodyToRenderedEye(const uint8_t* item,const PHD_3DPOS& pose,const PHD_VECTOR& animatedHead) {
    using namespace locomotion;
    g_bodyVisualOffset={};
    // The landing camera already follows the animated neck in all axes.
    // Standing body fitting would translate that same neck a second time.
    if (UseLandingCamera(item)) { g_mountBodyTransition.Reset(); return; }
    if (!CanTurnBody(item)) {
        const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
        if (IsLedgeMountState(state)) g_mountBodyTransition.Begin();
        else g_mountBodyTransition.Reset();
        return;
    }
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    const auto& prev=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos_prev);
    const int frac=std::clamp(*Ptr<int32_t>(g_boundDll->frameFrac),0,256);
    Vec view{},floor{};
    if (Cfg().positionalTracking && Cfg().firstPersonHeadTranslation && !VR().headAtCamera()) {
        VR().FirstPersonViewOffset(view.x,view.z);
        VR().HeadFloorOffset(floor.x,floor.z);
    }
    const float bodyYaw=Radians(prev.y_rot)+Wrap(Radians(pos.y_rot)-Radians(prev.y_rot))*(frac/256.f);
    const Vec eyeFromRoot{float(pose.x_pos-int32_t(prev.x_pos+(int64_t(pos.x_pos)-prev.x_pos)*frac/256)),
                          float(pose.z_pos-int32_t(prev.z_pos+(int64_t(pos.z_pos)-prev.z_pos)*frac/256))};
    g_bodyVisualOffset=g_groundEye.BodyOffsetAtEye(g_headingBase,bodyYaw,
        view,floor,LiveWorldUnitsPerMetre(),eyeFromRoot);
    if (Cfg().firstPersonMovementStabilization && g_groundEye.valid) {
        const float t=frac/256.f;
        const Vec root{prev.x_pos+(pos.x_pos-prev.x_pos)*t,prev.z_pos+(pos.z_pos-prev.z_pos)*t};
        const Vec animatedFromRoot{animatedHead.x-root.x,animatedHead.z-root.z};
        const Vec expected=Rotate(g_groundEye.bodyLocal,bodyYaw);
        const float height=animatedHead.y-(prev.y_pos+(pos.y_pos-prev.y_pos)*t);
        g_bodyVisualOffset=g_bodyVisualOffset-g_mountBodyTransition.Correction(
            animatedFromRoot-expected,height,GetTickCount64()/1000.0);
    } else g_mountBodyTransition.Reset();
    static uint64_t nextFitLog=0;
    if (Cfg().firstPersonDriftLog && GetTickCount64()>=nextFitLog) {
        nextFitLog=GetTickCount64()+5000;
        LogF("firstperson: resolved body fit yaw=%.1f base=%.1f offset=(%.1f,%.1f) pending=(%.3f,%.3f)m",
             bodyYaw*180/kPi,g_headingBase*180/kPi,g_bodyVisualOffset.x,g_bodyVisualOffset.z,floor.x,floor.z);
    }
}

bool Anchor(PHD_3DPOS& pose) {
    uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (!item) return false;
    const int joint = Cfg().firstPersonJoint;
    if (joint < 0 || joint >= 33) return false;

    int frac = *Ptr<int32_t>(g_boundDll->frameFrac);
    frac = std::clamp(frac, 0, 256);
    const float t = frac / 256.0f;
    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    const auto& old = *reinterpret_cast<const PHD_3DPOS*>(
        item + off::item_pos_prev);
    const float body[3] = {
        old.x_pos + (pos.x_pos - old.x_pos) * t,
        old.y_pos + (pos.y_pos - old.y_pos) * t,
        old.z_pos + (pos.z_pos - old.z_pos) * t
    };
    const int state = *reinterpret_cast<const int16_t*>(item + off::item_anim_state);
    PHD_VECTOR head{ Cfg().firstPersonAnchorX, Cfg().firstPersonAnchorY,
                     locomotion::FirstPersonAnchorZ(state, Cfg().firstPersonAnchorZ,
                                                    Cfg().firstPersonInteractionAnchorZ) };
    // Use the same interpolated, absolute joint query as the native renderer.
    // This routine saves/restores its matrix stack and adds Lara's world origin.
    reinterpret_cast<Fn_GetJointAbsPositionLerp>(
        g_boundBase + g_boundDll->getJointAbsPositionLerp)(item, &head, joint, frac);
    const double dx = double(head.x) - body[0];
    const double dy = double(head.y) - body[1];
    const double dz = double(head.z) - body[2];
    if (dx * dx + dy * dy + dz * dz > 4096.0 * 4096.0) {
        if (!g_loggedLost) {
            LogF("firstperson: joint %d is too far from Lara; not anchoring",
                 joint);
            g_loggedLost = true;
        }
        return false;
    }
    pose.x_pos = head.x;
    pose.y_pos = head.y;
    pose.z_pos = head.z;
    pose.x_rot = 0;
    UpdateLocomotion(pose);
    if (locomotion::IsGroundRollState(state) && LaraWaterStatus()==0) {
        // Keep the horizontal anchor in the new facing direction, but retain
        // the animated head height so the view drops with the roll. Clearance
        // below limits the final tracked eye before it can enter the floor.
        // Never capture a rolling joint into the standing calibration.
        const auto local=g_groundEye.valid ? g_groundEye.local : stabilization::Point{
            float(Cfg().firstPersonAnchorX),0,float(Cfg().firstPersonAnchorZ)};
        const auto flat=locomotion::Rotate({local.x,local.z},g_headingBase);
        pose.x_pos=int32_t(std::lround(body[0]+flat.x));
        pose.z_pos=int32_t(std::lround(body[2]+flat.z));
    } else if (Cfg().firstPersonMovementStabilization && CanTurnBody(item) && !UseLandingCamera(item)) {
        // Physical head yaw rotates Lara, not the scene-camera origin. Only
        // artificial yaw changes this reference frame; HMD pose is applied
        // separately, identically for both eyes, culling and tracked guns.
        const auto eye=g_groundEye.Apply({body[0],body[1],body[2]},g_headingBase,
            {float(head.x),float(head.y),float(head.z)},
            Radians(old.y_rot)+Wrap(Radians(pos.y_rot)-Radians(old.y_rot))*t);
        pose.x_pos=int32_t(std::lround(eye.x));
        pose.y_pos=int32_t(std::lround(eye.y));
        pose.z_pos=int32_t(std::lround(eye.z));
    } else if (!Cfg().firstPersonMovementStabilization) g_groundEye.Reset();
    // Landings and non-ground states use the full native animated eye, while
    // retaining the standing reference. Never pin the camera above/beside a
    // bending neck or save a kneeling pose as normal standing height.
    // On vault/pull-up completion the cached skeleton can still be
    // from the climb while Lara's root has moved onto the crate. Recapturing
    // then locks that transient high/sideways offset into every standing frame.
    // Item/level changes and explicit view toggles discard this calibration.
    ClampRenderedHeadToCollision(item, pose);
    FitBodyToRenderedEye(item,pose,head);
    TraceForwardCamera(item, body, head, frac, GetTickCount64(), &pose);
    pose.z_rot = 0;
    if (!g_loggedFirst) {
        LogF("firstperson: native joint %d anchored; offset from body=(%.0f,%.0f,%.0f)",
             joint, dx, dy, dz);
        g_loggedFirst = true;
    }
    return true;
}

bool DrawingLaraHead(const uint8_t* item, bool includeHeadOnlyGeometry=false) {
    const int16_t object = *reinterpret_cast<const int16_t*>(
        item + off::item_object);
    const GameDllLayout& dll = *g_boundDll;
    if (object < 0) return false;
    if (!dll.objects) return false;
    const uint8_t* objectInfo = Ptr<uint8_t>(dll.objects) +
        static_cast<uint32_t>(object) * off::object_stride;
    const void* mesh = *reinterpret_cast<void* const*>(
        objectInfo + off::object_geom + off::geom_mesh);
    if (!mesh) return false; // Uninitialized geometry is not a face match.
    if (dll.gLaraHeads) {
        const uint8_t* heads = Ptr<uint8_t>(dll.gLaraHeads);
        for (int i = 0; i < kLaraHeadGeoms; ++i) {
            const void* headMesh = *reinterpret_cast<void* const*>(
                heads + i * off::geom_stride + off::geom_mesh);
            if (mesh == headMesh) return true;
        }
    }
    // Alternate head/face draws can use a different mesh descriptor. During
    // the landing kneel, recognize them by their actual skeleton as well.
    // Do not discard a body or hand palette that merely includes joint 14.
    if (includeHeadOnlyGeometry) {
        const auto* geom=objectInfo+off::object_geom;
        const int count=*reinterpret_cast<const int32_t*>(geom+28);
        const auto* mapping=*reinterpret_cast<const int32_t* const*>(geom+48);
        if (count>0 && count<=33 && mapping) {
            for (int i=0;i<count;++i) if (mapping[i]!=14) return false;
            return true;
        }
    }
    return false;
}

// Change only this eye-view hand draw's geometry. The native descriptor is
// restored before DrawLaraHD resumes; simulation, armed grips and shadows keep
// their original hand selection. Copy the whole descriptor, including binding
// and material pointers, rather than mixing a rest mesh with a run skeleton.
class UnarmedRestHandScope {
    uint8_t* geometry=nullptr;
    uint8_t saved[off::geom_stride]{};
public:
    UnarmedRestHandScope(uint8_t* item,int32_t useMeshBits) {
        if (!useMeshBits || !g_unarmedHandDll || !g_unarmedHandDll->hands ||
            DrawingNativeShadow() || !UnarmedIKReady() ||
            item!=*Ptr<uint8_t*>(g_boundDll->laraItem)) return;
        const uint32_t bits=*reinterpret_cast<const uint32_t*>(item+off::item_mesh_bits);
        // Native unarmed hand passes select forearm+hand, singly or together.
        if (!bits || (bits&~0x3600u)) return;
        const int object=*reinterpret_cast<const int16_t*>(item+off::item_object);
        if (object<0 || !g_boundDll->objects) return;
        auto* current=Ptr<uint8_t>(g_boundDll->objects)+object*off::object_stride+off::object_geom;
        const auto mesh=*reinterpret_cast<const void* const*>(current+off::geom_mesh);
        if (!mesh) return;
        const auto* hands=Ptr<uint8_t>(g_unarmedHandDll->hands);
        for (int bank:{0,30,60}) {
            const auto* run=hands+(bank+2)*off::geom_stride;
            const auto* rest=hands+(bank+1)*off::geom_stride;
            if (mesh!=*reinterpret_cast<const void* const*>(run+off::geom_mesh) ||
                std::memcmp(current,run,off::geom_stride) ||
                !*reinterpret_cast<const void* const*>(rest+off::geom_mesh)) continue;
            geometry=current;
            std::memcpy(saved,current,sizeof(saved));
            std::memcpy(current,rest,sizeof(saved));
            break;
        }
    }
    ~UnarmedRestHandScope() { if (geometry) std::memcpy(geometry,saved,sizeof(saved)); }
    UnarmedRestHandScope(const UnarmedRestHandScope&)=delete;
    UnarmedRestHandScope& operator=(const UnarmedRestHandScope&)=delete;
};

void __cdecl Detour_DrawCreatureHD(uint8_t* item, int32_t useMeshBits,
                                   int32_t renderPass) {
    if (DrawingNativeShadow()) {
        // First-person clipping/hand splitting belongs only to the eye view.
        // The light must see the intact native body, head and weapon meshes.
        const int previousArm=g_renderArm, previousJoint=g_renderHandJoint;
        const bool previousScope=g_bodySkinScope;
        g_renderArm=-1;g_renderHandJoint=-1;g_bodySkinScope=false;
        g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item,useMeshBits,renderPass);
        g_renderArm=previousArm;g_renderHandJoint=previousJoint;
        g_bodySkinScope=previousScope;
        return;
    }
    if (g_active && g_rollHidden && g_boundDll && g_boundBase &&
        item == *Ptr<uint8_t*>(g_boundDll->laraItem)) return;
    bool lara = false;
    if (g_active && item && g_boundDll) {
        const GameDllLayout& dll = *g_boundDll;
        lara = item == *Ptr<uint8_t*>(dll.laraItem);
    }
    const bool landingHead=lara && HideLandingHead(item);
    const bool hideHead=Cfg().firstPersonHideHead || landingHead;
    if (lara && (hideHead || g_ledgeArmsOnly || MotionReady()) &&
        DrawingLaraHead(item,landingHead)) {
        ++g_faceSkips;
        return;
    }
    const bool motion = lara && MotionReady();
    const bool fullBody = lara && FullBodyIKReady();
    // Hide crouched/prone/submerged geometry, but retain the existing tracked-hand/gun
    // passes when available. Rolls still suppress everything above.
    if (lara && (g_crouchHidden || g_underwaterHidden) && !motion && !fullBody) return;
    if (motion) {
        // Full-body mode adds the torso/legs/arm chains; the two gun passes
        // still contain only hands and guns, avoiding duplicate forearms.
        // Keep the complete skeleton so wrist pivot and gun placement stay put.
        auto& bits = *reinterpret_cast<uint32_t*>(item + off::item_mesh_bits);
        if (!useMeshBits) {
            if(fullBody) {
                const auto savedBits=bits;const int previousArm=g_renderArm;
                bits=UINT32_MAX & ~firstperson::HeadMeshBit & ~0x2400u;
                g_renderArm=-1;g_bodySkinScope=true;g_bodySkinReady=false;
                g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item,1,renderPass);
                g_bodySkinScope=g_bodySkinReady=false;g_renderArm=previousArm;bits=savedBits;
            }
            return;
        }
        const uint32_t handMask=motiongun::HandOnlyMask(bits);
        if (!handMask) return;
        const uint32_t savedMotionBits=bits;
        const int previousArm=g_renderArm;
        const int previousHandJoint=g_renderHandJoint;
        // Long guns can put both hands in one native mesh pass. Split it
        // before applying the two independent wrist transforms.
        for (int hand=0;hand<2;++hand) {
            const uint32_t mask=hand ? 0x400 : 0x2000;
            vr::HmdMatrix34_t pose{};
            if (!(handMask&mask) || !VR().ControllerPose(hand,pose)) continue;
            bits=mask; g_renderArm=hand; g_renderHandJoint=-1;
            g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item,1,renderPass);
        }
        bits=savedMotionBits; g_renderArm=previousArm;
        g_renderHandJoint=previousHandJoint;
        return;
    }
    const UnarmedRestHandScope restHands(item,lara ? useMeshBits : 0);
    const int previousArm = g_renderArm;
    g_renderArm = -1;
    if (lara && (hideHead || g_ledgeArmsOnly || g_unarmedArmsHidden || UnarmedIKReady() || fullBody)) {
        auto& bits = *reinterpret_cast<uint32_t*>(item + off::item_mesh_bits);
        const uint32_t savedBits = bits;
        // Native HD body passes use useMeshBits=0: their effective mask is ALL
        // joints, even if the item's persistent mask is zero/stale at startup.
        // Apply visibility to that effective mask for this draw alone.
        // Already-masked weapon passes must retain their native restrictions.
        bits = VisibleMeshBits(useMeshBits ? savedBits : UINT32_MAX,
            hideHead,false,false,g_ledgeArmsOnly && !fullBody,false,g_unarmedArmsHidden && !fullBody);
        g_bodySkinScope=true; g_bodySkinReady=false;
        g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item, 1, renderPass);
        g_bodySkinScope=g_bodySkinReady=false;
        bits = savedBits;
    } else {
        g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item, useMeshBits, renderPass);
    }
    g_renderArm = previousArm;
}

void __cdecl Detour_DrawHair(int32_t argument) {
    const auto* item=g_boundDll && g_boundBase ? *Ptr<uint8_t*>(g_boundDll->laraItem) : nullptr;
    if (!DrawingNativeShadow() && g_active && (HideLandingHead(item) || g_headHidden || g_rollHidden || g_crouchHidden || g_underwaterHidden || g_ledgeArmsOnly || MotionReady())) {
        ++g_hairSkips;
        return;
    }
    g_hDrawHair.Original<Fn_DrawHair>()(argument);
}

void UpdateSceneCamera(PHD_3DPOS& pose) {
    const bool wasActive = g_active;
    if (Gate() && Anchor(pose)) {
        g_active = true;
        g_scenePose = pose;
        g_scenePoseValid = true;
        ++g_anchored;
    } else {
        g_active = false;
        g_scenePoseValid = false;
        g_haveHeading = false;
        // Suspend, don't toggle the user's preference. No queued shot or
        // equip gesture may leak out of swimming/cutscenes/menus into resumed play.
        g_gunTriggers.Reset();
        g_gunEquip.Reset();
        g_ledgePull.Reset();
        // Keep item identity and last viewing heading across UI/cameras.
        g_haveManualInput = false;
        const int water=LaraWaterStatus();
        if (water==1 || water==2) {
            // Native swimming can change facing while FP is suspended. On
            // leaving water, anchor to Lara rather than the pre-swim heading.
            g_headingItem=nullptr;
        }
        g_groundMoveAction = 0;
        g_renderTurn.Reset(); g_turnTrace={}; g_rootMotion.Reset();
        if (!g_runtimeEnabled || !Cfg().firstPerson) g_groundEye.Reset();
        g_bodyVisualOffset={};
        g_mountBodyTransition.Reset();
        g_bodyTime = {};
        g_dragPrevious = g_dragCurrent = g_dragShown = {};
        ++g_skipped;
        g_cameraMotionTrace = {};
    }
    if (wasActive && !g_active) VR().RecenterThirdPersonHead();
    const auto* item = g_boundDll && g_boundBase
        ? *Ptr<uint8_t*>(g_boundDll->laraItem) : nullptr;
    const bool unarmedMovement=g_active && item && CanTurnBody(item) &&
        *Ptr<int16_t>(g_boundDll->lara+2)==0 &&
        *Ptr<int16_t>(g_boundDll->lara+4)>=0 && *Ptr<int16_t>(g_boundDll->lara+4)<=6;
    const bool unarmedIK=UnarmedIKReady();
    const bool fullBodyIK=FullBodyIKReady();
    if (!unarmedIK) g_unarmedTwist[0]=g_unarmedTwist[1]={};
    const bool hideUnarmedArms=g_unarmedArmVisibility.Hide(unarmedMovement && !unarmedIK,VR().HeadPitchRadians());
    SetMeshVisibility(g_active && (Cfg().firstPersonHideHead || HideLandingHead(item)),
                      g_active && IsRollState(item),
                      g_active && !fullBodyIK && IsCrouchState(item),
                      g_active && !fullBodyIK && item && (locomotion::IsLedgeHangState(
                          *reinterpret_cast<const int16_t*>(item+off::item_anim_state)) ||
                          locomotion::IsLedgeMountState(*reinterpret_cast<const int16_t*>(item+off::item_anim_state))),
                      g_active && item && LaraWaterStatus()==1,hideUnarmedArms); // UNDERWATER, not SURFACE/WADE/FLYCHEAT
}

void __cdecl Detour_GenerateW2V(PHD_3DPOS* pose) {
    const void* caller = _ReturnAddress();
    if (pose && IsSceneCall(caller)) {
        const PHD_3DPOS native=*pose;
        UpdateSceneCamera(*pose);
        RebaseShadowReceiver(native,*pose);
    }
    if (pose && g_firingHand >= 0 && g_motionDll &&
        reinterpret_cast<uint64_t>(caller) ==
            g_boundBase + g_motionDll->fireViewReturn) {
        const int16_t spreadPitch = int16_t(pose->x_rot - g_firingAim[1]);
        const int16_t spreadYaw = int16_t(pose->y_rot - g_firingAim[0]);
        pose->x_pos = g_firingPose.x_pos;
        pose->y_pos = g_firingPose.y_pos;
        pose->z_pos = g_firingPose.z_pos;
        pose->x_rot = int16_t(g_firingPose.x_rot + spreadPitch);
        pose->y_rot = int16_t(g_firingPose.y_rot + spreadYaw);
        // Native phd_GenerateW2V's third row is the actual shot direction:
        // (sin(yaw)*cos(pitch), -sin(pitch), cos(yaw)*cos(pitch)).
        const float yaw = Radians(pose->y_rot);
        const float pitch = Radians(pose->x_rot);
        g_firingDirection = motiongun::ShotForward(yaw, pitch);
    }
    g_hGenerateW2V.Original<Fn_GenerateW2V>()(pose);
}

void RemoveLongGunHooks() {
    g_longGunHooksReady=false; g_longShot=nullptr; g_opticShot=false;
    g_triggerWeapon=0;
    g_hInitialiseProjectile.Remove(); g_hGunJoint.Remove();
    g_hFireCrossbow.Remove(); g_hFireSpecial.Remove();
    g_hFireShotgun.Remove(); g_hRifleHandler.Remove();
    g_longGunDll=nullptr;
}

bool InstallLongGunHooks(const GameDllLayout& d,uint64_t base) {
    for (const auto& row:kLongGunDlls)
        if (row.timestamp==d.timestamp) g_longGunDll=&row;
    if (!g_longGunDll) return false;
    const auto& r=*g_longGunDll;
    // Optic calls are also verified at runtime, not just in the binary audit.
    for (auto ret:r.opticReturns) {
        const auto* call=reinterpret_cast<const uint8_t*>(base+ret-5);
        int32_t delta=0; std::memcpy(&delta,call+1,4);
        if (*call!=0xe8 || int64_t(ret)+delta!=g_motionDll->getTargetOnLOS)
            return false;
    }
    const uint8_t rifle4[]={0x40,0x53,0x57,0x48,0x81,0xec,0x98,0,0,0};
    const uint8_t rifle5[]={0x40,0x53,0x56,0x57,0x41,0x56};
    const uint8_t shotgun[]={0x48,0x89,0x5c,0x24,0x18};
    const uint8_t grenade[]={0x41,0x55,0x48,0x81,0xec,0xb0,0,0,0};
    const uint8_t hk[]={0x89,0x4c,0x24,0x08,0x48,0x83,0xec,0x38};
    const uint8_t crossbow4[]={0x40,0x57,0x48,0x83,0xec,0x60};
    const uint8_t crossbow5[]={0x48,0x89,0x7c,0x24,0x18};
    const uint8_t helper[]={0x48,0x89,0x5c,0x24,0x08};
    auto install=[&](hook::InlineHook& h,uint32_t address,void* detour,
                     const uint8_t* bytes,size_t size,const char* name) {
        return h.Install(reinterpret_cast<void*>(base+address),detour,size,bytes,size,name);
    };
    const bool tr4=d.game==0;
    return install(g_hRifleHandler,r.rifle,reinterpret_cast<void*>(&Detour_RifleHandler),
            tr4 ? rifle4 : rifle5,tr4 ? sizeof(rifle4) : sizeof(rifle5),"MotionRifleHandler") &&
        install(g_hFireShotgun,r.shotgun,reinterpret_cast<void*>(&Detour_FireShotgun),
            shotgun,sizeof(shotgun),"MotionFireShotgun") &&
        install(g_hFireSpecial,r.special,tr4 ? reinterpret_cast<void*>(&Detour_FireGrenade) :
            reinterpret_cast<void*>(&Detour_FireHK),tr4 ? grenade : hk,
            tr4 ? sizeof(grenade) : sizeof(hk),"MotionFireSpecial") &&
        install(g_hFireCrossbow,r.crossbow,reinterpret_cast<void*>(&Detour_FireCrossbow),
            tr4 ? crossbow4 : crossbow5,tr4 ? sizeof(crossbow4) : sizeof(crossbow5),
            "MotionFireCrossbow") &&
        install(g_hGunJoint,r.joint,reinterpret_cast<void*>(&Detour_GunJoint),
            helper,sizeof(helper),"MotionGunJoint") &&
        install(g_hInitialiseProjectile,r.initialise,reinterpret_cast<void*>(&Detour_InitialiseProjectile),
            helper,sizeof(helper),"MotionInitialiseProjectile");
}

bool Install(const GameDllLayout& d, uint64_t base) {
    if (!d.phdGenerateW2V) return false;
    if (!d.w2vSceneReturn) return false;
    if (!d.frameFrac) return false;
    if (!d.laraItem) return false;
    if (!d.getJointAbsPositionLerp || !d.aimWeapon) return false;
    if (!d.laraAboveWater || !d.animateLara || !d.analogInput || !d.input) return false;
    if (!d.getCollisionInfo || !d.getFloor || !d.getHeight || !d.itemNewRoom ||
        !d.playingCutseq || !d.cutseqNum || !d.cutseqTrigger || !d.useSpotCam ||
        (d.game==0 && !d.vonCroyCutscene)) return false;
    const bool retail = d.timestamp == 0x68C12FDA || d.timestamp == 0x68C12FE9;
    if (std::memcmp(reinterpret_cast<const void*>(base + d.getCollisionInfo),
                    retail ? kCollisionRetail : kCollisionDebug, sizeof(kCollisionDebug)) ||
        std::memcmp(reinterpret_cast<const void*>(base + d.getFloor), kGetFloor, sizeof(kGetFloor)) ||
        std::memcmp(reinterpret_cast<const void*>(base + d.getHeight),
                    d.game == 0 ? kGetHeightTR4 : kGetHeightTR5, sizeof(kGetHeightTR4)) ||
        std::memcmp(reinterpret_cast<const void*>(base + d.itemNewRoom), kItemNewRoom, sizeof(kItemNewRoom))) {
        Log("firstperson: native room-scale helper bytes mismatch; refusing to install");
        return false;
    }
    const uint8_t* movement = d.game == 0 ? kLaraAboveWaterTR4 : kLaraAboveWaterTR5;
    if (!g_hLaraAboveWater.Install(
            reinterpret_cast<void*>(base + d.laraAboveWater),
            reinterpret_cast<void*>(&Detour_LaraAboveWater), 5,
            movement, 5, "LaraAboveWater")) return false;
    const uint8_t* animation = d.game == 0 ? kAnimateLaraTR4 : kAnimateLaraTR5;
    const size_t animationBytes = d.game == 0 ? sizeof(kAnimateLaraTR4) : sizeof(kAnimateLaraTR5);
    if (!g_hAnimateLara.Install(
            reinterpret_cast<void*>(base + d.animateLara),
            reinterpret_cast<void*>(&Detour_AnimateLara), animationBytes,
            animation, animationBytes, "AnimateLara")) return false;
    if (!g_hAimWeapon.Install(
            reinterpret_cast<void*>(base + d.aimWeapon),
            reinterpret_cast<void*>(&Detour_AimWeapon), 5,
            kAimWeaponPrologue, sizeof(kAimWeaponPrologue), "AimWeapon"))
        return false;
    for (const auto& entry : kMotionDlls)
        if (entry.timestamp == d.timestamp) g_motionDll = &entry;
    g_unarmedHandDll=nullptr;
    for (const auto& entry:kUnarmedHandDlls)
        if (entry.timestamp==d.timestamp) g_unarmedHandDll=&entry;
    if (g_motionDll && !g_hGetJoints.Install(
            reinterpret_cast<void*>(base+g_motionDll->getJoints),
            reinterpret_cast<void*>(&Detour_GetJoints),5,
            kGetJointsPrologue,sizeof(kGetJointsPrologue),"GetJoints"))
        Log("firstperson: body palette correction unavailable; native masking retained");
    for (const auto& entry:kShadowDlls) {
        if (entry.timestamp!=d.timestamp) continue;
        g_shadowDll=&entry;
        const uint8_t tr4Debug[]={0x40,0x55,0x53,0x57,0x48,0x8d,0x6c,0x24,0xe0};
        const uint8_t tr4Retail[]={0x40,0x55,0x53,0x57,0x48,0x8d,0x6c,0x24,0xb9};
        const uint8_t tr5[]={0x40,0x55,0x53,0x57,0x41,0x55};
        const auto* bytes=d.game==1 ? tr5 : retail ? tr4Retail : tr4Debug;
        const size_t size=d.game==1 ? sizeof(tr5) : sizeof(tr4Debug);
        if (!g_hDrawToShadow.Install(reinterpret_cast<void*>(base+entry.cast),
                reinterpret_cast<void*>(&Detour_DrawToShadow),size,bytes,size,"FirstPersonDrawToShadow"))
            Log("firstperson: shadow receiver origin correction unavailable");
        Log("firstperson: intact native shadows; eye-view body and hand offsets excluded");
    }
    if (Cfg().firstPersonMotionGuns) {
        if (g_motionDll &&
            !std::memcmp(reinterpret_cast<const void*>(base+g_motionDll->getSpheres),
                kGetJointsPrologue,sizeof(kGetJointsPrologue)) &&
            !std::memcmp(reinterpret_cast<const void*>(base+g_motionDll->findTargetPoint),
                kGetFloor,sizeof(kGetFloor)) &&
            !std::memcmp(reinterpret_cast<const void*>(base+g_motionDll->lineOfSight),
                kGetFloor,sizeof(kGetFloor)) &&
            g_hGetJoints.installed() &&
            g_hAnimatePistols.Install(
                reinterpret_cast<void*>(base+g_motionDll->animatePistols),
                reinterpret_cast<void*>(&Detour_AnimatePistols),5,
                kAnimatePistols,sizeof(kAnimatePistols),"AnimatePistols") &&
            g_hFireWeapon.Install(
                reinterpret_cast<void*>(base + g_motionDll->fireWeapon),
                reinterpret_cast<void*>(&Detour_FireWeapon), 5,
                kFireWeaponPrologue, sizeof(kFireWeaponPrologue), "FireWeapon") &&
            g_hPistolHandler.Install(
                reinterpret_cast<void*>(base + g_motionDll->pistolHandler),
                reinterpret_cast<void*>(&Detour_PistolHandler), 5,
                d.game == 0 ? kPistolHandlerTR4 : kPistolHandlerTR5,
                5, "PistolHandler") &&
            g_hGetTargetOnLOS.Install(
                reinterpret_cast<void*>(base + g_motionDll->getTargetOnLOS),
                reinterpret_cast<void*>(&Detour_GetTargetOnLOS), 6,
                d.game == 0 ? kGetTargetOnLOSTR4 : kGetTargetOnLOSTR5,
                6, "GetTargetOnLOS") &&
            g_hSetGunFlash.Install(
                reinterpret_cast<void*>(base + g_motionDll->setGunFlash),
                reinterpret_cast<void*>(&Detour_SetGunFlash),
                d.game == 0 ? sizeof(kSetGunFlashTR4) : sizeof(kSetGunFlashTR5),
                d.game == 0 ? kSetGunFlashTR4 : kSetGunFlashTR5,
                d.game == 0 ? sizeof(kSetGunFlashTR4) : sizeof(kSetGunFlashTR5),
                "SetGunFlash")) {
            g_motionHooksReady = true;
            Log("firstperson: Touch bind-pose wrist + native muzzle/flash hooks ready");
            if (Cfg().firstPersonUnarmedIK)
                Log("firstperson: unarmed HD arm IK enabled, using shared gun calibration");
        } else {
            g_hSetGunFlash.Remove();
            g_hGetTargetOnLOS.Remove();
            g_hPistolHandler.Remove();
            g_hAnimatePistols.Remove();
            g_hFireWeapon.Remove();
            g_motionDll = nullptr;
            Log("firstperson: Touch dual-gun hooks unavailable; head aim preserved");
        }
    }
    if (!g_hGenerateW2V.Install(
            reinterpret_cast<void*>(base + d.phdGenerateW2V),
            reinterpret_cast<void*>(&Detour_GenerateW2V), 5,
            kGenerateW2VPrologue, sizeof(kGenerateW2VPrologue),
            "phd_GenerateW2V"))
        return false;
    for (const auto& entry:kActionIconDlls) if (entry.timestamp==d.timestamp) {
        static constexpr uint8_t prologue[]={0x4c,0x8b,0xdc,0x41,0x56};
        if (g_hDrawActionIndicators.Install(reinterpret_cast<void*>(base+entry.draw),
                reinterpret_cast<void*>(&Detour_DrawActionIndicators),sizeof(prologue),
                prologue,sizeof(prologue),"DrawActionIndicators")) {
            g_actionIconDll=&entry;
            Log("firstperson: nearby Action icon HUD placement hook ready");
        } else Log("firstperson: Action icon hook unavailable; native prompts preserved");
    }
    if (d.drawCreatureHD) {
        const bool retail4 = d.timestamp == 0x68C12FDA;
        const bool retail5 = d.timestamp == 0x68C12FE9;
        const uint8_t* expected = kDrawCreatureDebug;
        if (retail4) expected = kDrawCreatureRetail;
        if (retail5) expected = kDrawCreatureRetail;
        if (!g_hDrawCreatureHD.Install(
            reinterpret_cast<void*>(base + d.drawCreatureHD),
            reinterpret_cast<void*>(&Detour_DrawCreatureHD), 5,
            expected, 5, "DrawCreatureHD"))
            Log("firstperson: head geometry hook unavailable");
    }
    if (d.drawHair) {
        if (!g_hDrawHair.Install(
            reinterpret_cast<void*>(base + d.drawHair),
            reinterpret_cast<void*>(&Detour_DrawHair), 5,
            kDrawHairPrologue, sizeof(kDrawHairPrologue), "DrawHair"))
            Log("firstperson: hair hook unavailable");
    }
    if (g_motionHooksReady &&
        (!g_hDrawCreatureHD.installed() || !g_hDrawHair.installed())) {
        g_motionHooksReady = false;
        g_hSetGunFlash.Remove();
        g_hGetTargetOnLOS.Remove();
        g_hPistolHandler.Remove();
        g_hAnimatePistols.Remove();
        g_hFireWeapon.Remove();
        g_motionDll = nullptr;
        Log("firstperson: Touch dual-gun mode disabled; body/hair hooks unavailable");
    }
    if (g_motionHooksReady) {
        if (InstallLongGunHooks(d,base)) {
            g_longGunHooksReady=true;
            Log("firstperson: Touch all-weapon hooks ready (single weapons use right controller)");
        } else {
            RemoveLongGunHooks();
            Log("firstperson: Touch additional weapon hooks unavailable; dual guns preserved");
        }
    }
    return true;
}

void Remove() {
    g_hDrawActionIndicators.Remove();
    g_actionIconDll=nullptr;
    g_loggedActionIcon=false;
    ResetMovementStabilization();
    RemoveLongGunHooks();
    g_gunTriggers.Reset();
    g_gunEquip.Reset();
    g_calibrationActive=false;
    g_cameraMotionTrace = {};
    if (g_active) VR().RecenterThirdPersonHead();
    RestoreHeadMesh();
    g_motionHooksReady = false;
    g_firingHand = g_renderArm = g_renderHandJoint = -1;
    g_hkScopeBindItem=nullptr;
    g_scenePoseValid = false;
    g_motionArmDraws[0] = g_motionArmDraws[1] = 0;
    g_motionShots[0] = g_motionShots[1] = 0;
    g_motionReportTime = 0;
    g_headAimFallbacks[0] = g_headAimFallbacks[1] = 0;
    g_gunPoseFailure[0] = g_gunPoseFailure[1] = "not-built";
    g_hSetGunFlash.Remove();
    g_hGetTargetOnLOS.Remove();
    g_hPistolHandler.Remove();
    g_hAnimatePistols.Remove();
    g_hFireWeapon.Remove();
    RestoreShadowReceiver();g_shadowReceiver=nullptr;g_shadowDepthScope=g_loggedShadowReceiver=false;
    g_hDrawToShadow.Remove();g_shadowDll=nullptr;g_unarmedHandDll=nullptr;
    g_hGetJoints.Remove();
    g_motionDll = nullptr;
    g_hDrawHair.Remove();
    g_hDrawCreatureHD.Remove();
    g_hGenerateW2V.Remove();
    g_hAimWeapon.Remove();
    g_hLaraAboveWater.Remove();
    g_hAnimateLara.Remove();
    g_active = false;
    g_haveHeading = false;
    g_headingItem = nullptr;
    g_dragPrevious = g_dragCurrent = g_dragShown = {};
    g_haveManualInput = false;
    g_groundMoveAction = 0;
    g_bodyTime = {};
    g_boundDll = nullptr;
    g_boundBase = 0;
}

bool UpdateGunTriggers(uint8_t& left, uint8_t& right, bool enabled, uint64_t now, bool y=false) {
    if (!enabled) {
        g_gunTriggers.Reset(); g_gunEquip.Reset(); g_triggerWeapon=0;
        return false;
    }
    int weapon=CurrentMotionWeapon();
    if (!weapon) weapon=*Ptr<int16_t>(g_boundDll->lara+8);
    if (weapon!=g_triggerWeapon) {
        if (MotionWeaponSupported(g_triggerWeapon) && MotionWeaponSupported(weapon)) {
            // Native drawing/inventory may change the selected gun during an
            // LT press. Retain draw intent, but never transfer a
            // queued press or repeat to the newly selected gun.
            g_gunTriggers.Clear(0); g_gunTriggers.Clear(1);
        } else {
            g_gunTriggers.Reset();
        }
        g_triggerWeapon=weapon;
    }
    const auto* app=reinterpret_cast<const uint8_t*>(Base()+L().app);
    // LaraGun selects the setting for classic/modern controls, not graphics.
    const unsigned controlScheme=(app[0x9e4]>>1)&1;
    const bool holdMode=app[0x9ec+controlScheme]==0;
    const int status=*Ptr<int16_t>(g_boundDll->lara+2);
    const bool rawLeft=left>30;
    const bool nativeEquip=g_gunEquip.Update(holdMode,status,rawLeft,y);
    const bool motion=MotionTriggerMode();
    g_gunTriggers.Update(motion,motion && MotionReady() && g_gunEquip.desiredArmed && !g_gunEquip.consumeY,
        rawLeft && !g_gunEquip.blockLeft,right>30,now);
    if (!motiongun::DualWeapon(weapon)) g_gunTriggers.Clear(0);
    left=nativeEquip ? 255 : 0;
    // RT also owns native grab/Action while the guns are away or Lara's hands
    // are busy. Keep its analog value AND hold duration in those states.
    if (status==4 && motion) right=g_gunTriggers.WantsShot() ? 255 : 0;
    if ((!g_gunEquip.desiredArmed || g_gunEquip.consumeY) && status==4) right=0;
    return g_gunEquip.consumeY;
}

} // namespace

bool FirstPersonGunTriggers(uint8_t& left, uint8_t& right, bool chordConsumed, bool y) {
    DWORD process=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&process);
    const bool enabled=!chordConsumed && process==GetCurrentProcessId() && WeaponControlMode();
    return UpdateGunTriggers(left,right,enabled,GetTickCount64(),y);
}

bool FirstPersonShadowPass() { return g_shadowDepthScope; }
bool FirstPersonDrawingTrackedHands() { return g_renderArm>=0; }
int FirstPersonTrackedHandJoint() {
    return g_renderArm>=0 && g_renderHandJoint>=0 && g_renderHandJoint<72 ? g_renderHandJoint : -1;
}

bool FirstPersonHKScopePose(motiongun::Frame& lens, motiongun::Frame& camera,float* radiusMetres) {
    if (!Cfg().firstPersonHKScope || !g_boundDll || g_boundDll->game!=1 ||
        CurrentMotionWeapon()!=5 || !MotionReady() || IsOpticsZoomed() ||
        Cfg().monoTracking || !Cfg().duplicateDraws || Cfg().eyeOffsetMode!=3 ||
        !Cfg().perEyeView || Cfg().perEyeProjection!=1) return false;
    GunPose gun{};
    if (!BuildGunPose(1,gun)) return false;
    const float scale=LiveWorldUnitsPerMetre();
    lens={gun.desired,Add(gun.trackedHand,Transform(gun.desired,
        {0,Cfg().firstPersonHKScopeForwardMetres*scale,Cfg().firstPersonHKScopeUpMetres*scale}))};
    float radius=.03f;
    if (Cfg().firstPersonHKScopeMeshFit) {
        // The live object geometry may now be Lara's face. Use the binding
        // copied inside the actual HK draw, with THIS frame's controller pose.
        const auto* item=*Ptr<uint8_t*>(g_boundDll->laraItem);
        if (!item || g_hkScopeBindItem!=item) {
            static uint64_t lastWait=0;
            const auto now=GetTickCount64();
            if (now-lastWait>=5000) {
                Log("HK scope: waiting for right-hand HK mesh binding");
                lastWait=now;
            }
            return false;
        }
        float radiusUnits=0;
        if (!hkscope::MeshLens({gun.desired,gun.trackedHand},g_hkScopeInverseBind,lens,radiusUnits)) return false;
        radius=radiusUnits/scale;
        if (!std::isfinite(radius) || radius<.001f || radius>.15f) return false;
    }
    if (radiusMetres) *radiusMetres=radius;
    // Optical axis uses the same calibrated barrel frame and muzzle as shots.
    camera={gun.desired,gun.muzzle};
    return true;
}

bool FirstPersonHKScopeAiming() {
    motiongun::Frame lens{},camera{};
    float rot[3][3]{},pos[3]{};
    if (!FirstPersonHKScopePose(lens,camera) || !CameraViewFrame(rot,pos)) return false;
    const auto world=hkscope::WorldView(rot,pos);
    for (int eye=0;eye<2;++eye) {
        const auto eyeWorld=InvertRigid(Mul(VR().EyeView(Eye(eye)),world));
        if (hkscope::EyeBox(lens,{eyeWorld.r[0][3],eyeWorld.r[1][3],eyeWorld.r[2][3]},LiveWorldUnitsPerMetre())) return true;
    }
    return false;
}

void FirstPersonUpdate() {
    if (!MotionReady() || CurrentMotionWeapon()!=5) g_hkScopeBindItem=nullptr;
    if (g_gunTriggers.active && !MotionTriggerMode()) {
        g_gunTriggers.Reset();
    }
    if (!WeaponControlMode()) g_gunEquip.Reset();
    PollMotionGunCalibration();
    ReportMotionActivity();
    static bool recenterHeld = false;
    const bool recenterDown = Cfg().firstPersonRecenterKey &&
        (GetAsyncKeyState(Cfg().firstPersonRecenterKey) & 0x8000) != 0;
    if (recenterDown && !recenterHeld && FirstPersonActive()) FirstPersonRecenter();
    recenterHeld = recenterDown;
    if (!g_runtimeInitialized) {
        g_runtimeInitialized = true;
        g_runtimeEnabled = false;
        Log("firstperson: startup is third person; Y+LT switches views");
    }
    const GameDllLayout* dll = GameDllBound();
    const uint64_t base = GameDllBase();
    bool want = Cfg().enabled && Cfg().firstPerson;
    if (!dll) want = false;
    if (!base) want = false;
    if (want) want = (*dll).phdGenerateW2V != 0;
    if (g_boundDll) {
        bool changed = !want;
        if (dll != g_boundDll) changed = true;
        if (base != g_boundBase) changed = true;
        if (changed) Remove();
    }
    if (!want) return;
    if (g_boundDll) return;
    if (base == g_failedBase) return;
    g_boundDll = dll;
    g_boundBase = base;
    if (!Install(*dll, base)) {
        Log("firstperson: required camera/aim/locomotion hook failed; mode remains third person");
        Remove();
        g_failedBase = base;
        return;
    }
    LogF("firstperson: %s camera + head-aim + directional/room-scale locomotion ready; joint %d",
         (*dll).name, Cfg().firstPersonJoint);
}

void FirstPersonRecenter() {
    g_ledgePull.Reset();
    g_renderTurn.Reset();
    g_turnTrace={};
    g_rootMotion.Reset();
    g_cameraMotionTrace = {};
    VR().RecenterFirstPersonHead();
    g_dragPrevious = g_dragCurrent = g_dragShown = {};
    Log("firstperson: head position recentered; world heading preserved");
}

void FirstPersonToggle() {
    g_unarmedArmVisibility={};
    g_unarmedTwist[0]=g_unarmedTwist[1]={};
    ResetMovementStabilization();
    g_calibrationActive=false;
    g_cameraMotionTrace = {};
    const bool wasActive = g_active;
    if (!g_runtimeInitialized) {
        g_runtimeEnabled = false;
        g_runtimeInitialized = true;
    }
    g_runtimeEnabled = !g_runtimeEnabled;
    g_active = false;
    g_haveHeading = false;
    g_headingItem = nullptr;
    g_dragPrevious = g_dragCurrent = g_dragShown = {};
    g_shifted = g_jumpPressed = false;
    g_bodyTime = {};
    g_loggedFirst = false;
    g_haveManualInput = false;
    g_groundMoveAction = 0;
    RestoreHeadMesh();
    if (wasActive && !g_runtimeEnabled) VR().RecenterThirdPersonHead();
    else VR().RecentreOffset();
    LogF("firstperson: switched to %s",
         g_runtimeEnabled ? "FIRST PERSON" : "third person");
}

void FirstPersonShutdown() {
    g_calibrationActive=false;
    if (g_calibrationMessageHook) UnhookWindowsHookEx(g_calibrationMessageHook);
    g_calibrationMessageHook=nullptr;
    g_calibrationWindow=nullptr;
    g_calibrationKeys={};
    g_calibrationCommandCount=0;
    if (g_boundDll) Remove();
    g_failedBase = 0;
    g_runtimeEnabled = false;
    g_runtimeInitialized = false;
    g_loggedFirst = false;
    g_loggedLost = false;
    g_anchored = 0;
    g_skipped = 0;
    g_faceSkips = 0;
    g_hairSkips = 0;
}

bool FirstPersonCalibrationKeyReserved(int key) {
    if (!g_calibrationMessageHook || key<VK_F1 || key>VK_F7) return false;
    if (g_calibrationKeys.captured[key-VK_F1]) return true;
    return g_calibrationActive && CalibrationFocused() &&
        (GetAsyncKeyState(VK_CONTROL)&0x8000) && !(GetAsyncKeyState(VK_MENU)&0x8000);
}

const float* FirstPersonBodyPalette(uint64_t& mask,int& count) {
    mask=g_bodySkinMask; count=g_bodySkinCount;
    return g_bodySkinScope && g_bodySkinReady && g_renderArm<0 ? g_bodySkinPalette : nullptr;
}
const float* FirstPersonRenderBodyPalette(uint64_t& mask,int& count) {
    return FirstPersonBodyPalette(mask,count) ? g_bodyRenderPalette : nullptr;
}

bool FirstPersonActive() {
    return g_active;
}

void FirstPersonInput(float& leftX, float& leftY, float& rightX, bool shifted,
                      bool jumpPressed) {
    using namespace locomotion;
    g_haveManualInput = false;
    g_shifted = shifted;
    g_jumpPressed = jumpPressed;
    if (!g_active || !g_haveHeading || !Gate()) {
        g_renderTurn.Reset();
        return;
    }
    auto* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (!item || item != g_headingItem) { g_renderTurn.Reset(); return; }
    const float dead =
        std::clamp(Cfg().firstPersonTurnDeadzone, 0.0f, 0.95f);
    const float magnitude = std::min(1.0f, std::fabs(rightX));
    float turnRate=0;
    if (magnitude > dead) {
        const float strength = (magnitude - dead) / (1.0f - dead);
        turnRate = std::copysign(strength, rightX) *
            std::clamp(Cfg().firstPersonTurnDegreesPerSecond, 0.0f, 720.0f) *
            (kPi / 180.0f);
    }
    g_renderTurn.Sample(turnRate,TurnTime());
    if (Cfg().firstPersonDriftLog) ++g_turnTrace.polls;
    g_lastHeadWorld = Wrap(g_headingBase + VR().HeadYawRadians());
    const int state = *reinterpret_cast<int16_t*>(item + off::item_anim_state);
    const bool jump = IsJumpSteeringState(state) && LaraWaterStatus() == 0 &&
        *reinterpret_cast<int16_t*>(item + off::item_hit_points) > 0;
    if (!CanTurnBody(item) && !jump && !CanSteerMonkeyBars(item)) return;
    rightX = 0.0f;
    const float headWorld = Wrap(g_headingBase + VR().HeadYawRadians());
    g_lastHeadWorld = headWorld;
    Vec manual{leftX, leftY};
    if (Length(manual) < 0.20f || shifted) manual = {};
    g_manualLocal = manual;
    // Input is camera-relative to analogInput.camTurn, which is NOT the
    // rendered camera yaw. Using the latter fed native steering back into FP.
    const float inputYaw = Radians(*Ptr<int16_t>(g_boundDll->analogInput + 4));
    g_manualWorld = Rotate(manual, Cfg().firstPersonMoveWithHead ? headWorld : inputYaw);
    g_haveManualInput = true;
    if (NewControls()) {
        const Vec result = Limit(Rotate(g_manualWorld, -inputYaw));
        leftX = result.x;
        leftY = result.z;
    } else {
        leftX = manual.x;
        leftY = manual.z;
    }
}

} // namespace tr
