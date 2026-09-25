#include "FirstPerson.h"
#include "Config.h"
#include "Engine.h"
#include "GameDll.h"
#include "InlineHook.h"
#include "Log.h"
#include "VRSystem.h"
#include "LocomotionMath.h"
#include "FirstPersonClearance.h"

#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace tr {
namespace {

struct PHD_3DPOS {
    int32_t x_pos, y_pos, z_pos;
    int16_t x_rot, y_rot, z_rot;
    int16_t padding;
};

struct PHD_VECTOR { int32_t x, y, z; };
static_assert(sizeof(PHD_3DPOS) == 20);
static_assert(sizeof(PHD_VECTOR) == 12);

namespace off {
constexpr uint32_t item_mesh_bits   = 12;
constexpr uint32_t item_floor       = 0;
constexpr uint32_t item_floor_prev  = 4;
constexpr uint32_t item_object      = 16;
constexpr uint32_t item_anim_state  = 18;
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
constexpr uint32_t lara_turn_rate   = 220;
constexpr uint32_t lara_move_angle  = 222;
constexpr uint32_t lara_vehicle     = 38;
constexpr uint32_t camera_type      = 32;
constexpr uint32_t object_stride    = 3792;
constexpr uint32_t object_geom      = 112;
constexpr uint32_t geom_stride      = 120;
constexpr uint32_t geom_mesh        = 16;
} // namespace off

constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kHeadMeshBit = 1u << 14;
constexpr int kLaraHeadGeoms = 4;
constexpr int32_t kCamFixed = 1;
constexpr int32_t kCamCinematic = 4;

using Fn_GenerateW2V = void (__cdecl*)(PHD_3DPOS*);
using Fn_DrawCreatureHD = void (__cdecl*)(uint8_t*, int32_t, int32_t);
using Fn_DrawHair = void (__cdecl*)(int32_t);
using Fn_GetJointAbsPositionLerp = void (__cdecl*)(uint8_t*, PHD_VECTOR*, int32_t, int32_t);
using Fn_AimWeapon = void (__cdecl*)(void*, uint8_t*);
using Fn_LaraAboveWater = void (__cdecl*)(uint8_t*, void*);
using Fn_AnimateLara = void (__cdecl*)(uint8_t*);

hook::InlineHook g_hGenerateW2V;
hook::InlineHook g_hDrawCreatureHD;
hook::InlineHook g_hDrawHair;
hook::InlineHook g_hAimWeapon;
hook::InlineHook g_hLaraAboveWater;
hook::InlineHook g_hAnimateLara;
const GameDllLayout* g_boundDll = nullptr;
uint64_t g_boundBase = 0;
uint64_t g_failedBase = 0;

bool g_runtimeEnabled = false;
bool g_runtimeInitialized = false;
bool g_active = false;
bool g_haveHeading = false;
float g_headingBase = 0.0f;
float g_lastHeadWorld = 0.0f;
LARGE_INTEGER g_inputTime{};
LARGE_INTEGER g_bodyTime{};
uint8_t* g_headingItem = nullptr;
locomotion::Vec g_previousBody{};
locomotion::Vec g_dragPrevious{}, g_dragCurrent{}, g_dragShown{};
locomotion::Vec g_manualLocal{}, g_manualWorld{};
bool g_haveManualInput = false;
bool g_shifted = false;
bool g_jumpPressed = false;
int g_directionalRootScale = 1;

bool g_meshOverride = false;
bool g_headHidden = false;
bool g_rollHidden = false;
uint8_t* g_meshItem = nullptr;
uint32_t g_meshBaseBits = 0;
unsigned g_anchored = 0;
unsigned g_skipped = 0;
unsigned g_faceSkips = 0;
unsigned g_hairSkips = 0;
bool g_loggedFirst = false;
bool g_loggedLost = false;

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

bool IsSceneCall(const void* returnAddress) {
    return g_boundDll && g_boundBase &&
        reinterpret_cast<uint64_t>(returnAddress) ==
            g_boundBase + g_boundDll->w2vSceneReturn;
}

bool Gate() {
    if (!g_boundDll || !g_boundBase || !Cfg().enabled ||
        !Cfg().firstPerson || !g_runtimeEnabled)
        return false;
    if (!VR().active() || !VR().poseValid()) return false;
    if (InInventory() || InFMV() || InTitle()) return false;
    if (g_boundDll->playingCutseq && *Ptr<int32_t>(g_boundDll->playingCutseq)) return false;
    const GameDllLayout& dll = *g_boundDll;
    if (!*Ptr<uint8_t*>(dll.laraItem)) return false;
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
    g_headHidden = g_rollHidden = false;
}

void SetMeshVisibility(bool hideHead, bool hideRoll) {
    if ((!hideHead && !hideRoll) || !g_boundDll || !g_boundBase) {
        RestoreHeadMesh();
        return;
    }
    uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (!item) {
        g_meshOverride = false;
        g_meshItem = nullptr;
        return;
    }
    if (!g_meshOverride || item != g_meshItem) {
        g_meshOverride = true;
        g_meshItem = item;
        g_meshBaseBits = *reinterpret_cast<uint32_t*>(
            item + off::item_mesh_bits);
    }
    auto& bits = *reinterpret_cast<uint32_t*>(item + off::item_mesh_bits);
    const uint32_t expected = g_rollHidden ? 0 :
        (g_headHidden ? g_meshBaseBits & ~kHeadMeshBit : g_meshBaseBits);
    if (g_meshOverride && bits != expected) {
        // Preserve native visibility changes outside our owned mask. During
        // a roll we own the full mask and must retain the pre-roll snapshot.
        if (!g_rollHidden)
            g_meshBaseBits = (bits & ~kHeadMeshBit) | (g_meshBaseBits & kHeadMeshBit);
    }
    g_headHidden = hideHead;
    g_rollHidden = hideRoll;
    bits = hideRoll ? 0 : (hideHead ? g_meshBaseBits & ~kHeadMeshBit : g_meshBaseBits);
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
    // Same ground states as TR1-3. Ladders, pickups, jumps and scripted
    // interactions own their facing and must not be rotated out of alignment.
    switch (*reinterpret_cast<const int16_t*>(item + off::item_anim_state)) {
    case 0: case 1: case 2: case 5: case 6: case 7:
    case 16: case 20: case 21: case 22: return true;
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

void __cdecl Detour_AimWeapon(void* weapon, uint8_t* arm) {
    g_hAimWeapon.Original<Fn_AimWeapon>()(weapon, arm);
    if (!Cfg().firstPersonHeadAim || !g_active || !g_haveHeading || !Gate()) return;
    uint8_t* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (item != g_headingItem) return;
    uint8_t* lara = Ptr<uint8_t>(g_boundDll->lara);
    if (arm != lara + off::lara_left_arm && arm != lara + off::lara_right_arm) return;
    const auto& pos = *reinterpret_cast<const PHD_3DPOS*>(item + off::item_pos);
    const int16_t yaw = Angle(g_headingBase + VR().HeadYawRadians() - Radians(pos.y_rot));
    const int16_t pitch = Angle(VR().HeadPitchRadians());
    // PistolHandler and RifleHandler both call here BEFORE animating/firing.
    // Their native code uses these angles for the visible arms, torso and shot
    // direction. Lock=1 prevents long guns from adding torso rotation twice.
    // Set both arms: the revolver path aims the left arm but fires the right.
    for (const uint32_t offset : { off::lara_left_arm, off::lara_right_arm }) {
        uint8_t* a = lara + offset;
        *reinterpret_cast<int16_t*>(a + off::arm_lock) = 1;
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
        if (Length(Vec{float(acceptedX), float(acceptedZ)}) > 64) break;
        pos.x_pos += acceptedX; pos.z_pos += acceptedZ;
        UpdateLaraRoom(item);
        if (!acceptedX && !acceptedZ) break;
    }
    const Vec actual = Vec{float(pos.x_pos), float(pos.z_pos)} - initial;
    g_dragCurrent = g_dragCurrent + actual * (1 / scale);
    return actual * (1 / scale);
}

// Match TR1-3: one animation tick, scaling only horizontal root displacement.
// Native collision still runs afterwards, and vertical/gravity motion is intact.
void __cdecl Detour_AnimateLara(uint8_t* item) {
    auto original = g_hAnimateLara.Original<Fn_AnimateLara>();
    const int scale = item && item == g_headingItem
        ? std::clamp(g_directionalRootScale, 1, 3) : 1;
    if (scale == 1) {
        original(item);
        return;
    }
    auto& pos = *reinterpret_cast<PHD_3DPOS*>(item + off::item_pos);
    const int32_t oldX = pos.x_pos, oldZ = pos.z_pos;
    original(item);
    pos.x_pos = oldX + (pos.x_pos - oldX) * scale;
    pos.z_pos = oldZ + (pos.z_pos - oldZ) * scale;
    auto& speed = *reinterpret_cast<int16_t*>(item + off::item_speed);
    speed = static_cast<int16_t>(std::clamp<int>(speed * scale, -32768, 32767));
}

void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {
    using namespace locomotion;
    g_dragPrevious = g_dragCurrent;
    g_directionalRootScale = 1;
    if (item && item == g_headingItem && g_active && g_haveHeading && Gate()) {
        const int state = *reinterpret_cast<int16_t*>(item + off::item_anim_state);
        const bool ground = CanTurnBody(item);
        const bool jump = IsJumpSteeringState(state) && LaraWaterStatus() == 0 &&
            *reinterpret_cast<int16_t*>(item + off::item_hit_points) > 0;
        if ((ground || jump) && g_haveManualInput) {
            const float head = Wrap(g_headingBase + VR().HeadYawRadians());
            auto* analog = Ptr<int16_t>(g_boundDll->analogInput);
            auto& input = *Ptr<uint64_t>(g_boundDll->input);
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
                    if (ground)
                        g_directionalRootScale = DirectionalRootScale(action, preparingJump);
                }
            }
        }
    }
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
    g_hLaraAboveWater.Original<Fn_LaraAboveWater>()(item, nativeCollision);
    g_directionalRootScale = 1;
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
    if (g_haveHeading && g_headingItem == item && !relocated) {
        // Consume only body drag actually visible at this render fraction.
        // Stick/animation movement must never cancel genuine physical leaning.
        const Vec shown = g_dragPrevious + (g_dragCurrent - g_dragPrevious) * t;
        const Vec used = Rotate(shown - g_dragShown, -g_headingBase);
        VR().ConsumeHeadFloorOffset(used.x, used.z);
        g_dragShown = shown;
    }
    if (!g_haveHeading || g_headingItem != item || relocated) {
        const float facing = g_headingItem == item && !relocated
            ? g_lastHeadWorld : Radians(pos.y_rot);
        g_headingBase = Wrap(facing - VR().HeadYawRadians());
        g_cameraMotionTrace = {};
        g_haveHeading = true;
        g_headingItem = item;
        g_haveManualInput = false;
        g_dragPrevious = g_dragCurrent = g_dragShown = {};
        g_inputTime = g_bodyTime = {};
        VR().RecenterFirstPersonHead();
    }
    g_previousBody = body;
    if (!CanTurnBody(item)) {
        Vec pending; VR().HeadFloorOffset(pending.x, pending.z);
        VR().ConsumeHeadFloorOffset(pending.x, pending.z);
    }
    TurnBodyToHead(item, Elapsed(g_bodyTime));
    pose.y_rot = Angle(g_headingBase);
    g_lastHeadWorld = Wrap(g_headingBase + VR().HeadYawRadians());
}

void ClampRenderedHeadToCollision(const uint8_t* item, PHD_3DPOS& pose) {
    if (!g_boundDll->getCollisionInfo) return;
    const bool ground = CanTurnBody(item);
    const int state = *reinterpret_cast<const int16_t*>(item + off::item_anim_state);
    // Match TR1-3's ground, jump, fall and wall-impact coverage. Interactions
    // use the retracted anchor instead, and third person never reaches here.
    const bool jump = LaraWaterStatus() == 0 &&
        *reinterpret_cast<const int16_t*>(item + off::item_hit_points) > 0 &&
        (state == 3 || state == 9 || state == 12 || state == 15 ||
         (state >= 25 && state <= 29));
    if (!ground && !jump) return;

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
    if (Cfg().positionalTracking && Cfg().firstPersonHeadTranslation) {
        VR().HeadFloorOffset(tracked.x, tracked.z);
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
    const bool airborne = !ground && state != 15;
    firstperson::ClampEyeToWall(body, eye,
        [&](int32_t x, int32_t z, int32_t clearX, int32_t clearZ) {
            RoomCollision coll{};
            coll.radius = 64;
            coll.badPos = airborne ? 4096 : 384;
            coll.badNeg = airborne ? -4096 : -384;
            coll.badCeiling = 0;
            coll.flags = 5;
            coll.old[0] = clearX; coll.old[1] = body[1]; coll.old[2] = clearZ;
            coll.facing = Angle(std::atan2(float(x - clearX), float(z - clearZ)));
            reinterpret_cast<Fn_GetCollisionInfo>(g_boundBase + g_boundDll->getCollisionInfo)(
                &coll, x, body[1], z, room, 762);
            return (!airborne && (coll.floorSamples[0] < -384 ||
                                  coll.floorSamples[0] > 384 ||
                                  coll.floorSamples[1] >= 0)) ||
                coll.type == 8 || coll.type == 16 || coll.type == 32 ||
                coll.shift[0] || coll.shift[2];
        });
    // Stereo adds the physical translation after the scene pose. Undo that
    // amount here so both rendered eyes remain on the clear side of the wall.
    pose.x_pos = eye[0] - offsetX;
    pose.z_pos = eye[2] - offsetZ;
}

// Diagnostic only: distinguish animated lateral sway from actual root motion
// and physical translation. Extrema over a full second avoid aliasing a gait
// cycle with a slow periodic log sample. Never modify the camera or input here.
void TraceForwardCamera(const uint8_t* item, const float body[3],
                        const PHD_VECTOR& head, int frac, uint64_t now) {
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
    Vec pending; VR().HeadFloorOffset(pending.x, pending.z);
    const Vec eyeWorld = headWorld + Rotate(pending, g_headingBase) * LiveWorldUnitsPerMetre();
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
    ClampRenderedHeadToCollision(item, pose);
    TraceForwardCamera(item, body, head, frac, GetTickCount64());
    pose.z_rot = 0;
    if (!g_loggedFirst) {
        LogF("firstperson: native joint %d anchored; offset from body=(%.0f,%.0f,%.0f)",
             joint, dx, dy, dz);
        g_loggedFirst = true;
    }
    return true;
}

bool DrawingLaraHead(const uint8_t* item) {
    const int16_t object = *reinterpret_cast<const int16_t*>(
        item + off::item_object);
    const GameDllLayout& dll = *g_boundDll;
    if (object < 0) return false;
    if (!dll.objects) return false;
    if (!dll.gLaraHeads) return false;
    const uint8_t* objectInfo = Ptr<uint8_t>(dll.objects) +
        static_cast<uint32_t>(object) * off::object_stride;
    const void* mesh = *reinterpret_cast<void* const*>(
        objectInfo + off::object_geom + off::geom_mesh);
    const uint8_t* heads = Ptr<uint8_t>(dll.gLaraHeads);
    for (int i = 0; i < kLaraHeadGeoms; ++i) {
        const void* headMesh = *reinterpret_cast<void* const*>(
            heads + i * off::geom_stride + off::geom_mesh);
        if (mesh == headMesh) return true;
    }
    return false;
}

void __cdecl Detour_DrawCreatureHD(uint8_t* item, int32_t useMeshBits,
                                   int32_t renderPass) {
    if (g_active && g_rollHidden && g_boundDll && g_boundBase &&
        item == *Ptr<uint8_t*>(g_boundDll->laraItem)) return;
    bool lara = false;
    if (g_active && Cfg().firstPersonHideHead && item && g_boundDll) {
        const GameDllLayout& dll = *g_boundDll;
        lara = item == *Ptr<uint8_t*>(dll.laraItem);
    }
    if (lara && DrawingLaraHead(item)) {
        ++g_faceSkips;
        return;
    }
    g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(
        item, lara ? 1 : useMeshBits, renderPass);
}

void __cdecl Detour_DrawHair(int32_t argument) {
    if (g_active && (g_headHidden || g_rollHidden)) {
        ++g_hairSkips;
        return;
    }
    g_hDrawHair.Original<Fn_DrawHair>()(argument);
}

void UpdateSceneCamera(PHD_3DPOS& pose) {
    const bool wasActive = g_active;
    if (Gate() && Anchor(pose)) {
        g_active = true;
        ++g_anchored;
    } else {
        g_active = false;
        g_haveHeading = false;
        // Keep item identity and last viewing heading across UI/cameras.
        g_haveManualInput = false;
        g_directionalRootScale = 1;
        g_inputTime = g_bodyTime = {};
        g_dragPrevious = g_dragCurrent = g_dragShown = {};
        ++g_skipped;
        g_cameraMotionTrace = {};
    }
    if (wasActive && !g_active) VR().RecenterThirdPersonHead();
    const auto* item = g_boundDll && g_boundBase
        ? *Ptr<uint8_t*>(g_boundDll->laraItem) : nullptr;
    SetMeshVisibility(g_active && Cfg().firstPersonHideHead,
                      g_active && IsRollState(item));
}

void __cdecl Detour_GenerateW2V(PHD_3DPOS* pose) {
    if (pose && IsSceneCall(_ReturnAddress())) UpdateSceneCamera(*pose);
    g_hGenerateW2V.Original<Fn_GenerateW2V>()(pose);
}

bool Install(const GameDllLayout& d, uint64_t base) {
    if (!d.phdGenerateW2V) return false;
    if (!d.w2vSceneReturn) return false;
    if (!d.frameFrac) return false;
    if (!d.laraItem) return false;
    if (!d.getJointAbsPositionLerp || !d.aimWeapon) return false;
    if (!d.laraAboveWater || !d.animateLara || !d.analogInput || !d.input) return false;
    if (!d.getCollisionInfo || !d.getFloor || !d.getHeight || !d.itemNewRoom ||
        !d.playingCutseq) return false;
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
    if (!g_hGenerateW2V.Install(
            reinterpret_cast<void*>(base + d.phdGenerateW2V),
            reinterpret_cast<void*>(&Detour_GenerateW2V), 5,
            kGenerateW2VPrologue, sizeof(kGenerateW2VPrologue),
            "phd_GenerateW2V"))
        return false;
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
    return true;
}

void Remove() {
    g_cameraMotionTrace = {};
    if (g_active) VR().RecenterThirdPersonHead();
    RestoreHeadMesh();
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
    g_directionalRootScale = 1;
    g_inputTime = g_bodyTime = {};
    g_boundDll = nullptr;
    g_boundBase = 0;
}

} // namespace

void FirstPersonUpdate() {
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
    g_cameraMotionTrace = {};
    VR().RecenterFirstPersonHead();
    g_dragPrevious = g_dragCurrent = g_dragShown = {};
    Log("firstperson: head position recentered; world heading preserved");
}

void FirstPersonToggle() {
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
    g_inputTime = g_bodyTime = {};
    g_loggedFirst = false;
    g_haveManualInput = false;
    g_directionalRootScale = 1;
    RestoreHeadMesh();
    if (wasActive && !g_runtimeEnabled) VR().RecenterThirdPersonHead();
    else VR().RecentreOffset();
    LogF("firstperson: switched to %s",
         g_runtimeEnabled ? "FIRST PERSON" : "third person");
}

void FirstPersonShutdown() {
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
        g_inputTime = {};
        return;
    }
    auto* item = *Ptr<uint8_t*>(g_boundDll->laraItem);
    if (!item || item != g_headingItem) return;
    const float dead =
        std::clamp(Cfg().firstPersonTurnDeadzone, 0.0f, 0.95f);
    const float magnitude = std::min(1.0f, std::fabs(rightX));
    if (magnitude > dead) {
        const float strength = (magnitude - dead) / (1.0f - dead);
        const float turn = std::copysign(strength, rightX) *
            std::clamp(Cfg().firstPersonTurnDegreesPerSecond, 0.0f, 720.0f) *
            (kPi / 180.0f) * Elapsed(g_inputTime);
        g_headingBase = Wrap(g_headingBase + turn);
        VR().PivotHeadFloorOffset(turn);
    } else {
        Elapsed(g_inputTime);
    }
    g_lastHeadWorld = Wrap(g_headingBase + VR().HeadYawRadians());
    const int state = *reinterpret_cast<int16_t*>(item + off::item_anim_state);
    const bool jump = IsJumpSteeringState(state) && LaraWaterStatus() == 0 &&
        *reinterpret_cast<int16_t*>(item + off::item_hit_points) > 0;
    if (!CanTurnBody(item) && !jump) return;
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
