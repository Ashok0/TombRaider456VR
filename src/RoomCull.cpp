// RoomCull.cpp -- extend the engine's visible set so a head-tracked view is not
// looking at nothing.
//
// The engine builds its draw list by walking portals from the camera's room,
// carrying a screen-space rectangle clipped at every doorway. That set is
// correct for the FLAT screen and for the GAME CAMERA's facing, and VR breaks
// both assumptions: the headset sees wider, and it can look somewhere the game
// camera is not pointing. Rooms that would be visible are never submitted.
//
// What is done here, and what was tried and rejected, in order:
//
//   * Append rooms by DISTANCE from the camera (DrawAllRooms). Works, but
//     proximity is the wrong criterion -- it happily adds a stacked room that
//     shares world space with the one you are standing in and no portal reaches,
//     which draws foreign geometry over your own. Kept only for A/B.
//
//   * Widen the portal rectangle. MEASURED INERT: swept to 200% of screen each
//     way, it changed neither geometry nor frame rate, and 20x on TR6 was the
//     same. A portal beside or behind the camera fails the NEAR-PLANE test
//     inside the clipper before the rectangle is consulted, so no rectangle can
//     rescue it. Removed.
//
//   * Expand along PORTAL CONNECTIVITY (PortalHops). This is the mechanism that
//     works: a room through the door behind you is one hop away whichever way
//     the game camera faces. Orientation bias gone.
//
//   * Reject hop rooms whose world bounding box overlaps a room already drawn.
//     MEASURED WRONG: a TR room box is a rectangle around an irregular interior
//     and neighbours share a border of wall sectors, so ordinary adjacency shows
//     a 2048x2048 overlap. It rejected six legitimate pairs at once. Removed.
//
//   * Test the connecting portal against the ACTUAL HEADSET FRUSTUM before
//     adding a hop room (PortalHeadTest). This is the current attempt at the
//     room-215 class of bug: hop expansion adds any portal-connected room and
//     draws it unclipped, so a room you cannot actually see through that portal
//     still gets drawn. The engine already does this test -- just from the game
//     camera. Doing it from the head is the missing piece.
#include "RoomCull.h"
#include "Engine.h"
#include "Config.h"
#include "InlineHook.h"
#include "StereoMath.h"
#include "Log.h"

#include <windows.h>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace tr {

// Published by Hooks.cpp: the world -> eye matrix actually injected last frame.
// This is what makes the head test possible; the engine's own view matrix is
// the game camera's and does not rotate with your head.
bool LastHeadView(Affine& out);

namespace {

struct GameAddrs {
    // Which game this row is for (0 = TR4, 1 = TR5) and the PE TimeDateStamp of
    // the DLL build it was read from. Rows are searched by the pair, so a patch
    // that swaps the game DLLs is recognised rather than silently landing every
    // hook in the middle of unrelated code.
    //
    // Not hypothetical: installing the HD pack's own game DLLs replaced the
    // 2026-01-17 set with a 2025-09-10 set, every prologue check failed, and the
    // result looked exactly like the culling bug this file exists to fix --
    // because with no hooks installed the game simply culls normally.
    int      game;
    uint32_t timestamp;

    const wchar_t* module;
    const char*    hookName;

    // The room renderer -- the CONSUMER of the draw list. Hooked here rather
    // than at the traversal (GetRoomBounds) deliberately: appending at the
    // producer leaves a second traversal pass to run over the enlarged list,
    // and that pass overran it. At the consumer the list is final.
    uint32_t renderRooms;
    const uint8_t* prologue;
    size_t         prologueLen;

    uint32_t drawList;      // int16[] room indices
    uint32_t drawCount;     // int32

    // How many entries drawList actually holds. NOT (drawCount - drawList)/2:
    // that arithmetic gives 206, and it is wrong. Scanning both DLLs for
    // RIP-relative references landing between the two shows unrelated globals
    // in the gap (tomb4 from 0x18063DDF0, tomb5 from 0x18063D850), so the array
    // ends at 200 in both. TR5 levels have 242 rooms, so the difference is not
    // academic -- the old bound wrote over live engine state every frame.
    int      drawListMax;

    uint32_t roomCount;
    uint32_t roomsPtr;
    uint32_t screenRight;
    uint32_t screenBottom;
    uint32_t camX, camY, camZ;

    // lara.water_status, or 0 when not yet found for this build.
    //
    // ABOVE_WATER 0, UNDERWATER 1, SURFACE 2, FLYCHEAT 3, WADE 4 -- the classic
    // LARA_WATER_STATUS enum. This is the right signal precisely because it is
    // LARA's state and not the camera's: it is true the moment she is swimming
    // and false the moment she is not, with no dependence on which room the
    // camera happens to be trailing in.
    //
    // FOUND BY MEMORY DIFF, after disassembly guessed wrong twice. The whole
    // writable data section was snapshotted on dry land, diffed while swimming
    // for bytes that went 0 -> 1..4, and the 96 survivors read back in three
    // states. Exactly one followed the enum:
    //
    //     address     surface  underwater  land
    //     0x4EE74C       2          1        0     <- water_status
    //     0x4EE720       4          4        0
    //     0x623AE4       2          1        2
    //     0x4B79B0       2          2        2
    //
    // Do NOT "correct" this to a nearby address from a classic struct layout.
    // 0x4EE820 was exactly such a guess -- reached down a plausible-looking
    // disassembly trail -- and it reads 0 forever. The 2/1/0 sequence above is
    // the only evidence here that means anything.
    //
    // TR4's was PORTED rather than re-diffed, by the method this file already
    // uses across builds. In TR5 the field is written ten times, as a WORD, by
    // the one function that also tests room+0x6C bit 0. Exactly one TR4
    // function matches that shape -- FUN_180060fc0, ten word writes to
    // 0x1804F2E4C -- and that global carries 97 xrefs (TR5's has 106) whose
    // readers compare it against 0, 1 and 4. Structure, arity and use all
    // agree, so it is the same field.
    //
    // The 2025-09-10 rows are the documented per-DLL shifts applied to those
    // two (-0xC0 in tomb5, +0xF40 in tomb4) and are UNVERIFIED. All of these
    // are reads, so a wrong address misbehaves rather than corrupts, and the
    // pad log prints whatever value it finds.
    uint32_t laraWaterStatus;
};

// TR4: mov rax,rsp + push r12.   TR5: mov [rsp+10h],rbx.
const uint8_t kPrologueTR4[] = { 0x48, 0x8B, 0xC4, 0x41, 0x54 };
const uint8_t kPrologueTR5[] = { 0x48, 0x89, 0x5C, 0x24, 0x10 };

// Searched by (game, PE timestamp) -- see GameAddrs.
//
// The 2025-09-10 rows were ported from the 2026-01-17 ones by the same method
// used for the exe: signature-match the renderer, then derive each global from
// the instructions that reference it. Every global moved by a uniform amount
// within a DLL (+0xF40 in tomb4, -0xC0 in tomb5), and the ROOM_INFO layout was
// confirmed unchanged first -- GetFloor matched at identical length with every
// struct offset appearing the same number of times -- so the box maths and the
// portal walk still hold.
const GameAddrs kGames[] = {
    // --- TR4 ---------------------------------------------------------------
    { 0, 0x696B4999, L"tomb4.dll", "TR4!DrawRoomList",
      0x000C5160, kPrologueTR4, sizeof(kPrologueTR4),
      0x0063DC60, 0x0063DDFC, 200, 0x00660810, 0x00663FC8,
      0x004947D0, 0x004947CC, 0x0049484C, 0x0049485C, 0x0049486C, 0x004F2E4C },
    { 0, 0x68C12FDA, L"tomb4.dll", "TR4!DrawRoomList",
      0x000C5DC0, kPrologueTR4, sizeof(kPrologueTR4),
      0x0063EBA0, 0x0063ED3C, 200, 0x00661750, 0x00664F08,
      0x00495710, 0x0049570C, 0x0049578C, 0x0049579C, 0x004957AC, 0x004F3D8C },

    // --- TR5 ---------------------------------------------------------------
    { 1, 0x696B499C, L"tomb5.dll", "TR5!DrawRoomList",
      0x000B9CA0, kPrologueTR5, sizeof(kPrologueTR5),
      0x0063D6C0, 0x0063D860, 200, 0x0065B2F0, 0x0065EC28,
      0x004EF8CC, 0x004EF8C8, 0x004EF94C, 0x004EF95C, 0x004EF96C, 0x004EE74C },
    { 1, 0x68C12FE9, L"tomb5.dll", "TR5!DrawRoomList",
      0x000BA040, kPrologueTR5, sizeof(kPrologueTR5),
      0x0063D600, 0x0063D7A0, 200, 0x0065B230, 0x0065EB68,
      0x004EF80C, 0x004EF808, 0x004EF88C, 0x004EF89C, 0x004EF8AC, 0x004EE68C },
};

// Whichever row matches the DLL actually loaded. Null until the hook installs.
const GameAddrs* g_addr[2] = { nullptr, nullptr };

const GameAddrs& A(int g) { return *g_addr[g]; }

// Distance in world units from the game camera up to the ceiling of the room it
// stands in, republished every frame the room renderer runs. Not per game: only
// one of TR4/TR5 renders at a time, and the reader wants the latest.
float g_headroom      = 0.0f;
bool  g_headroomValid = false;

// The PE TimeDateStamp of a loaded module.
uint32_t ModuleStamp(uint8_t* base) {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->FileHeader.TimeDateStamp;
}

constexpr size_t kRoomStride = 0x130;

// ROOM_INFO fields, confirmed against GetFloor (tomb5.dll!FUN_1800101f0) and the
// per-room light setup (tomb5.dll!FUN_1800f6a80), which does a full AABB test
// and so names every bound at once:
//
//     local_dc = *(int *)(room + 0x28);                        // x min
//     local_c4 = *(int *)(room + 0x38);                        // y min
//     local_c8 = *(int *)(room + 0x34);                        // y max
//     local_e0 = *(short *)(room + 0x3e) * 0x400 + local_dc;   // x max
//     local_c0 = *(int *)(room + 0x30);                        // z min
//     local_e4 = *(short *)(room + 0x3c) * 0x400 + local_c0;   // z max
//
//   0x08  portal list: int16 count, then 32 bytes per portal
//   0x28  world X      0x2C  world Y      0x30  world Z
//   0x34  y max (floor, larger Y -- TR is Y-down)
//   0x38  y min (ceiling)
//   0x3C  z size in sectors    0x3E  x size in sectors
//   0x4C..0x52  render clip rect (left, right, top, bottom) as shorts
//   0x68  flipped_room, -1 when the room is not half of a flip pair
//
// 0x2C is a real field -- the shake path in FUN_1800b9ca0 touches 0x28, 0x2C and
// 0x30 as one position triple -- but it is ZERO on every room measured, because
// TR carries absolute Y in the vertex data rather than a room origin. It is
// therefore useless for telling two rooms apart vertically; 0x34/0x38 are what
// carry a room's height, and they are what the dump reports.
constexpr size_t kOffPortals  = 0x08;
constexpr size_t kOffX        = 0x28;
constexpr size_t kOffY        = 0x2C;
constexpr size_t kOffZ        = 0x30;
constexpr size_t kOffYMax     = 0x34;
constexpr size_t kOffYMin     = 0x38;
constexpr size_t kOffZSize    = 0x3C;
constexpr size_t kOffXSize    = 0x3E;
constexpr size_t kOffClipL    = 0x4C;
constexpr size_t kOffFlipRoom = 0x68;

hook::InlineHook g_hook[2];
uint8_t*         g_base[2]      = { nullptr, nullptr };
bool             g_logged[2]    = { false, false };
bool             g_warned[2]    = { false, false };
int              g_lastFound[2] = { -1, -1 };
int              g_lastDrawn[2] = { -1, -1 };
unsigned         g_headTestRejects[2] = { 0, 0 };
unsigned         g_headTestPasses[2]  = { 0, 0 };
bool             g_headTestLogged[2]  = { false, false };

// --- draw list dump (RoomDumpKey) -------------------------------------------
//
// One block per keypress. The per-frame lines above report counts only, and a
// count cannot tell you which index belongs in DrawAllRoomsExclude.
bool     g_dumpKeyDown = false;
bool     g_dumpArmed   = false;

// Where each appended room came from, indexed by its slot in the draw list, so
// the dump can name the portal chain rather than only the result. drawListMax
// is 200 in both builds; 256 covers it with room to spare.
constexpr int kMaxSlots = 256;
int16_t  g_addedFrom[kMaxSlots] = {};
uint8_t  g_addedWave[kMaxSlots] = {};

typedef void (__cdecl* Fn_RenderRooms)(void);

// --- the head-oriented portal test -----------------------------------------
//
// Transform a portal's four corners world -> eye -> clip and ask whether the
// quad lands anywhere inside the frustum. Going all the way to clip space means
// the projection supplies the handedness and the field of view, so there is no
// sign convention to guess at and no FOV to keep in step by hand.
//
// Deliberately conservative: a portal straddling the near plane counts as
// visible, and the frustum is expanded by a margin. Losing geometry is the
// worse failure, so every uncertain case resolves toward drawing the room.
bool PortalFacesHead(const Affine& V, const mat4& P, const float cam[3],
                     const float quad[4][3], float margin) {
    float minx =  1e30f, maxx = -1e30f;
    float miny =  1e30f, maxy = -1e30f;
    int   inFront = 0;

    for (int i = 0; i < 4; ++i) {
        // Relative to the camera FIRST, then rotate.
        //
        // Only the rotation of the view matrix is used. Its translation column
        // is not the -R*p a textbook view matrix carries -- the engine packs
        // something camera-position-shaped there instead -- and using it as
        // though it were cost a whole test session: the error scales with world
        // coordinates, so the test behaved on a small level and rejected
        // essentially every portal on a large one. The camera position from the
        // engine's own globals is unambiguous, so supply the translation from
        // there and let the matrix contribute only orientation.
        const float wx = quad[i][0] - cam[0];
        const float wy = quad[i][1] - cam[1];
        const float wz = quad[i][2] - cam[2];

        const float vx = V.r[0][0]*wx + V.r[0][1]*wy + V.r[0][2]*wz;
        const float vy = V.r[1][0]*wx + V.r[1][1]*wy + V.r[1][2]*wz;
        const float vz = V.r[2][0]*wx + V.r[2][1]*wy + V.r[2][2]*wz;

        // clip = P * view    (P is column-major: element(row, col) = m[col*4+row])
        const float cx = P.m[0]*vx + P.m[4]*vy + P.m[ 8]*vz + P.m[12];
        const float cy = P.m[1]*vx + P.m[5]*vy + P.m[ 9]*vz + P.m[13];
        const float cw = P.m[3]*vx + P.m[7]*vy + P.m[11]*vz + P.m[15];

        if (cw <= 1e-4f) continue;          // at or behind the eye plane
        ++inFront;
        const float nx = cx / cw, ny = cy / cw;
        minx = std::min(minx, nx); maxx = std::max(maxx, nx);
        miny = std::min(miny, ny); maxy = std::max(maxy, ny);
    }

    if (inFront == 0) return false;         // wholly behind: cannot be seen through
    if (inFront < 4) return true;           // crosses the near plane: assume visible

    const float b = 1.0f + margin;
    return !(maxx < -b || minx > b || maxy < -b || miny > b);
}

// Edge-triggered, not level: held down it would dump every frame and bury the
// block you actually wanted in a thousand identical ones.
void PollRoomDumpKey() {
    const int key = Cfg().roomDumpKey;
    if (key == 0) { g_dumpKeyDown = false; return; }
    const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
    if (down && !g_dumpKeyDown) g_dumpArmed = true;
    g_dumpKeyDown = down;
}

// A room's world bounding box, as the engine's own per-room AABB test builds it
// (tomb5.dll!FUN_1800f6a80): x and z from the origin plus the sector counts, y
// straight out of 0x34/0x38. TR is Y-down, so y0 is the CEILING and y1 the
// floor.
struct Box { int32_t x0, x1, y0, y1, z0, z1; };

// Everything printed here is read back out of the engine's structures AFTER
// expansion has finished, so the block describes the list about to be drawn
// rather than the one we meant to build.
void DumpDrawList(int g, const int16_t* list, int count, int before,
                  const uint8_t* rooms, int numRooms, const float cam[3]) {
    auto RoomBox = [rooms](int r, Box& b) {
        const uint8_t* rm = rooms + (size_t)r * kRoomStride;
        b.x0 = *reinterpret_cast<const int32_t*>(rm + kOffX);
        b.z0 = *reinterpret_cast<const int32_t*>(rm + kOffZ);
        b.x1 = b.x0 + *reinterpret_cast<const int16_t*>(rm + kOffXSize) * 1024;
        b.z1 = b.z0 + *reinterpret_cast<const int16_t*>(rm + kOffZSize) * 1024;
        b.y0 = *reinterpret_cast<const int32_t*>(rm + kOffYMin);
        b.y1 = *reinterpret_cast<const int32_t*>(rm + kOffYMax);
    };

    LogF("rooms[TR%d]: ---- draw list dump ----------------------------", g + 4);

    // list[0] is where the engine's traversal started, which is the room the
    // GAME CAMERA is in -- not necessarily the room your HEAD is in, which is
    // the whole reason this file exists. It is still the reference everything
    // below is measured against.
    const int camRoom = (before > 0) ? (int)(uint16_t)list[0] : -1;
    if (camRoom >= 0 && camRoom < numRooms) {
        Box cb;
        RoomBox(camRoom, cb);
        LogF("rooms[TR%d]: camera at (%d, %d, %d), in room %d -- x %d..%d  "
             "z %d..%d  y %d..%d",
             g + 4, (int)cam[0], (int)cam[1], (int)cam[2], camRoom,
             cb.x0, cb.x1, cb.z0, cb.z1, cb.y0, cb.y1);
    } else {
        LogF("rooms[TR%d]: camera room unknown -- the traversal list was empty",
             g + 4);
    }

    // The engine's own set. Every one of these was reached through a real
    // portal chain and is drawn with a proper clip rect, so none of them is
    // ever the answer. Printed to bound the search, not to be searched.
    for (int i = 0; i < before; i += 20) {
        char line[512];
        line[0] = 0;
        const int last = std::min(i + 19, before - 1);
        for (int j = i; j <= last; ++j) {
            char tmp[16];
            _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, " %d", (int)(uint16_t)list[j]);
            strcat_s(line, sizeof(line), tmp);
        }
        LogF("rooms[TR%d]: traversal reached [%d-%d]:%s", g + 4, i, last, line);
    }

    if (count == before) {
        LogF("rooms[TR%d]: nothing appended this frame -- if the geometry is "
             "wrong here, expansion is not what put it there", g + 4);
    } else {
        LogF("rooms[TR%d]: appended %d -- these, and only these, are the "
             "DrawAllRoomsExclude candidates:", g + 4, count - before);
    }

    // Nearest first, because that is the order suspicion falls in: foreign
    // geometry laid over your own comes from a room sharing your space, and a
    // room sharing your space is a near one. Measured from the box CENTRE, not
    // the origin -- the origin is a corner, so a large room you are standing in
    // can sort behind a small one across the map.
    struct Row {
        int64_t d2;
        int     room, from, wave;
        Box     b;
    };
    Row row[kMaxSlots];
    int n = 0;
    for (int i = before; i < count && n < kMaxSlots; ++i) {
        const int r = (int)(uint16_t)list[i];
        if (r < 0 || r >= numRooms) continue;
        Row& e = row[n];
        e.room = r;
        e.from = (i < kMaxSlots) ? (int)g_addedFrom[i] : -1;
        e.wave = (i < kMaxSlots) ? (int)g_addedWave[i] :  0;
        RoomBox(r, e.b);
        const int64_t dx = (e.b.x0 + e.b.x1) / 2 - (int64_t)cam[0];
        const int64_t dy = (e.b.y0 + e.b.y1) / 2 - (int64_t)cam[1];
        const int64_t dz = (e.b.z0 + e.b.z1) / 2 - (int64_t)cam[2];
        e.d2 = dx*dx + dy*dy + dz*dz;
        ++n;
    }
    std::sort(row, row + n, [](const Row& l, const Row& r) { return l.d2 < r.d2; });

    const int32_t cx = (int32_t)cam[0], cy = (int32_t)cam[1], cz = (int32_t)cam[2];

    // Distances are reported in metres because that is the unit you can judge
    // by eye in a headset. Taken from config rather than hardcoded: the whole
    // point of WorldUnitsPerMetre is that it gets retuned.
    const double upm = (Cfg().worldUnitsPerMetre > 1.0f)
                     ? (double)Cfg().worldUnitsPerMetre : 423.0;

    for (int i = 0; i < n; ++i) {
        const Row& e = row[i];

        // Where the camera sits relative to this room's actual box. The old
        // version of this compared ORIGINS against the camera's room and could
        // never fire: room origin Y is zero on every room, so "same XZ,
        // different Y" was unsatisfiable. These use the bounds the engine's own
        // AABB test uses, so they mean what they say.
        const bool insideXZ = (cx >= e.b.x0 && cx <= e.b.x1 &&
                               cz >= e.b.z0 && cz <= e.b.z1);
        const bool insideY  = (cy >= e.b.y0 && cy <= e.b.y1);

        // TR is Y-down: y0 is the ceiling, y1 the floor. Camera above the
        // ceiling means the room is below you.
        const char* flag = "";
        if (insideXZ && insideY) {
            // A room whose box contains the camera, that the engine's own
            // traversal did not reach. It is drawn with a full-screen clip
            // rect, so its walls land across your whole view. This is the
            // strongest phantom signal there is.
            flag = "  <-- CONTAINS THE CAMERA";
        } else if (insideXZ) {
            flag = (cy < e.b.y0) ? "  <-- STACKED UNDERFOOT"
                                 : "  <-- STACKED OVERHEAD";
        }

        char via[48];
        if (e.wave > 0)
            _snprintf_s(via, sizeof(via), _TRUNCATE, "via room %3d on hop %d",
                        e.from, e.wave);
        else
            _snprintf_s(via, sizeof(via), _TRUNCATE, "via distance forcing  ");

        LogF("rooms[TR%d]:   room %3d  %s  x %d..%d  z %d..%d  y %d..%d  "
             "%dm away%s",
             g + 4, e.room, via,
             e.b.x0, e.b.x1, e.b.z0, e.b.z1, e.b.y0, e.b.y1,
             (int)(std::sqrt((double)e.d2) / upm + 0.5), flag);
    }

    // Echoed so a dump taken with a half-finished exclude list cannot be
    // misread later as one taken with none.
    if (Cfg().excludeCount > 0) {
        char line[512];
        line[0] = 0;
        for (int i = 0; i < Cfg().excludeCount; ++i) {
            char tmp[16];
            _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, " %d", Cfg().excludeRooms[i]);
            strcat_s(line, sizeof(line), tmp);
        }
        LogF("rooms[TR%d]: DrawAllRoomsExclude already holds:%s", g + 4, line);
    }

    LogF("rooms[TR%d]: ---- end dump ----------------------------------", g + 4);
}

void ForceAllRooms(int g) {
    // First thing, ahead of every early return below: a press must not be
    // swallowed by a frame that happened to bail out before reaching the dump.
    PollRoomDumpKey();

    uint8_t* base = g_base[g];
    const GameAddrs& a = A(g);
    auto At = [base](uint32_t rva) { return base + rva; };

    int32_t&  count = *reinterpret_cast<int32_t*>(At(a.drawCount));
    int16_t*  list  =  reinterpret_cast<int16_t*>(At(a.drawList));
    uint8_t*  rooms = *reinterpret_cast<uint8_t**>(At(a.roomsPtr));
    if (!rooms) return;

    const int kMaxRooms = a.drawListMax;

    int32_t numRooms = *reinterpret_cast<int32_t*>(At(a.roomCount));
    if (numRooms <= 0 || numRooms > 1024) {
        numRooms = (int32_t)*reinterpret_cast<uint16_t*>(At(a.roomCount));
    }
    if (numRooms <= 0 || numRooms > 1024) return;
    if (count < 0) return;
    if (count > kMaxRooms) count = kMaxRooms;

    const int32_t right  = *reinterpret_cast<int32_t*>(At(a.screenRight));
    const int32_t bottom = *reinterpret_cast<int32_t*>(At(a.screenBottom));
    if (right <= 0 || bottom <= 0) return;

    const int before = count;

    if (count != g_lastFound[g]) {
        g_lastFound[g] = count;
        LogF("rooms[TR%d]: traversal found %d of %d rooms (hops %d, headtest %s)",
             g + 4, count, numRooms, Cfg().portalHops,
             Cfg().portalHeadTest ? "on" : "off");
    }

    // --- flip storage rooms -------------------------------------------------
    //
    // A flip room is a second copy of a piece of level holding its other state.
    // DoFlipMap (tomb4.dll!FUN_1800a5340) swaps the two structs whole and fixes
    // the link, so the invariant always holds: the LIVE room names its storage
    // copy at 0x68, and the copy holds -1. Portal traversal can never reach a
    // storage slot, so nothing should add one.
    uint8_t isFlipStorage[1024] = {};
    for (int r = 0; r < numRooms; ++r) {
        const int16_t f = *reinterpret_cast<const int16_t*>(
            rooms + (size_t)r * kRoomStride + kOffFlipRoom);
        if (f >= 0 && f < numRooms) isFlipStorage[f] = 1;
    }

    // --- expand along portal connectivity -----------------------------------
    Affine headView;
    mat4*  proj = Proj();
    const bool haveHead = Cfg().portalHeadTest && LastHeadView(headView) && proj;

    const float camPos[3] = {
        (float)*reinterpret_cast<int32_t*>(At(a.camX)),
        (float)*reinterpret_cast<int32_t*>(At(a.camY)),
        (float)*reinterpret_cast<int32_t*>(At(a.camZ)),
    };

    // --- ceiling headroom, for the VR head clamp ----------------------------
    //
    // list[0] is the room the GAME CAMERA is in, and TR's Y is down-positive,
    // so a room's YMin is its CEILING and the headroom is camY - YMin.
    //
    // This is the room's BOUNDING BOX -- the highest ceiling anywhere in the
    // room. In a low tunnel or a crawlspace, which is where a head clips
    // through in the first place, the room is uniformly low and the box is
    // exact. In a low alcove off a tall hall it reports the hall, and the clamp
    // simply does not engage. That is wrong in the safe direction: it can fail
    // to clamp, but it can never clamp somewhere roomy.
    if (before > 0) {
        const int camRoom = (int)(uint16_t)list[0];
        if (camRoom >= 0 && camRoom < numRooms) {
            const uint8_t* rm = rooms + (size_t)camRoom * kRoomStride;
            const int32_t ceilY = *reinterpret_cast<const int32_t*>(rm + kOffYMin);
            g_headroom      = camPos[1] - (float)ceilY;
            g_headroomValid = true;
        }
    }

    const int hops = Cfg().portalHops;
    if (hops > 0) {
        int waveStart = 0;
        int waveEnd   = count;

        for (int h = 0; h < hops && count < kMaxRooms; ++h) {
            for (int i = waveStart; i < waveEnd && count < kMaxRooms; ++i) {
                const int32_t parent = (int32_t)(uint16_t)list[i];
                if (parent < 0 || parent >= numRooms) continue;

                const uint8_t* prm = rooms + (size_t)parent * kRoomStride;
                const int16_t* portals =
                    *reinterpret_cast<const int16_t* const*>(prm + kOffPortals);
                if (!portals) continue;

                const int np = portals[0];
                if (np <= 0 || np > 1024) continue;

                const float px = (float)*reinterpret_cast<const int32_t*>(prm + kOffX);
                const float py = (float)*reinterpret_cast<const int32_t*>(prm + kOffY);
                const float pz = (float)*reinterpret_cast<const int32_t*>(prm + kOffZ);

                for (int p = 0; p < np && count < kMaxRooms; ++p) {
                    const int16_t* e = portals + 1 + p * 16;
                    const int32_t adj = (int32_t)e[0];
                    if (adj < 0 || adj >= numRooms) continue;
                    if (isFlipStorage[adj]) continue;

                    // Per-index escape hatch. Not a fix -- what makes room 215
                    // special has never been identified -- but it is one line
                    // and it demonstrably works while the general mechanisms
                    // are still being tried.
                    bool excluded = false;
                    for (int x = 0; x < Cfg().excludeCount; ++x)
                        if (Cfg().excludeRooms[x] == adj) { excluded = true; break; }
                    if (excluded) continue;

                    bool present = false;
                    for (int k = 0; k < count; ++k)
                        if (list[k] == (int16_t)adj) { present = true; break; }
                    if (present) continue;

                    // Portal vertices are room-local shorts; the four corners
                    // start six bytes past the normal.
                    if (haveHead) {
                        float quad[4][3];
                        for (int v = 0; v < 4; ++v) {
                            quad[v][0] = px + (float)e[4 + v * 3 + 0];
                            quad[v][1] = py + (float)e[4 + v * 3 + 1];
                            quad[v][2] = pz + (float)e[4 + v * 3 + 2];
                        }
                        if (!PortalFacesHead(headView, proj[1], camPos, quad,
                                             Cfg().portalHeadMargin)) {
                            ++g_headTestRejects[g];
                            continue;
                        }
                        ++g_headTestPasses[g];
                    }

                    // Provenance for the dump. Recorded here rather than
                    // reconstructed later: once the wave loop moves on there
                    // is no way back from a room index to the portal that
                    // reached it.
                    if (count < kMaxSlots) {
                        g_addedFrom[count] = (int16_t)parent;
                        g_addedWave[count] = (uint8_t)(h + 1);
                    }
                    list[count++] = (int16_t)adj;
                }
            }
            waveStart = waveEnd;
            waveEnd   = count;
            if (waveStart == waveEnd) break;      // nothing new
        }
    }

    // --- distance-based forcing (legacy, off by default) --------------------
    if (Cfg().drawAllRooms) {
        const int64_t cx = *reinterpret_cast<int32_t*>(At(a.camX));
        const int64_t cy = *reinterpret_cast<int32_t*>(At(a.camY));
        const int64_t cz = *reinterpret_cast<int32_t*>(At(a.camZ));

        struct Cand { int64_t d2; int32_t room; };
        Cand cand[1024];
        int  n = 0;
        for (int r = 0; r < numRooms; ++r) {
            bool present = false;
            for (int i = 0; i < count; ++i)
                if (list[i] == (int16_t)r) { present = true; break; }
            if (present || isFlipStorage[r]) continue;
            const uint8_t* rm = rooms + (size_t)r * kRoomStride;
            const int64_t dx = *reinterpret_cast<const int32_t*>(rm + kOffX) - cx;
            const int64_t dy = *reinterpret_cast<const int32_t*>(rm + kOffY) - cy;
            const int64_t dz = *reinterpret_cast<const int32_t*>(rm + kOffZ) - cz;
            cand[n].d2 = dx*dx + dy*dy + dz*dz;
            cand[n].room = r;
            ++n;
        }
        std::sort(cand, cand + n,
                  [](const Cand& l, const Cand& r) { return l.d2 < r.d2; });
        for (int i = 0; i < n && count < kMaxRooms; ++i) {
            if (count < kMaxSlots) {
                g_addedFrom[count] = -1;        // wave 0 = not a hop room
                g_addedWave[count] = 0;
            }
            list[count++] = (int16_t)cand[i].room;
        }
    }

    // --- clip rects ---------------------------------------------------------
    //
    // Every listed room gets a FULL-SCREEN rect, and that is required rather
    // than lazy. The engine's portal-clipped rects are screen boxes computed
    // for the GAME CAMERA's view; we render from the HMD's, so under head
    // rotation they scissor the wrong region entirely. Rooms we appended also
    // still carry the inverted "empty" bounds the previous frame reset them to.
    //
    // NOTE: nothing here writes room+0x4B. That byte is not a render flag --
    // it feeds a pass that relocates two-room objects with ItemNewRoom, which
    // moves items out of the room you are standing in and stops them blocking
    // you. Writing it is what originally let Lara walk through a wall.
    if (Cfg().drawAllRoomsClip) {
        for (int i = 0; i < count; ++i) {
            uint8_t* room = rooms + (size_t)(uint16_t)list[i] * kRoomStride;
            int16_t* rect = reinterpret_cast<int16_t*>(room + kOffClipL);
            rect[0] = 0;                    // left
            rect[1] = (int16_t)right;       // right
            rect[2] = 0;                    // top
            rect[3] = (int16_t)bottom;      // bottom
        }
    }

    if (count != g_lastDrawn[g]) {
        g_lastDrawn[g] = count;
        LogF("rooms[TR%d]: %d hop(s) added %d -> drawing %d of %d",
             g + 4, hops, count - before, count, numRooms);
    }

    // Serviced last, so the block reports the list as the engine will actually
    // draw it -- clip rects widened and all -- and not an intermediate state.
    if (g_dumpArmed) {
        g_dumpArmed = false;
        DumpDrawList(g, list, count, before, rooms, numRooms, camPos);
    }

    // One-shot proof that the head test is doing something, with the ratio it
    // is doing it at. A test that never rejects is not protecting anything; one
    // that rejects nearly everything is eating the geometry it should add.
    // Reported every 2000 decisions, NOT once. A single early sample is what
    // hid this test rejecting almost everything: the one-shot fired in a small
    // level where it behaved, and never re-measured on the large one where it
    // did not.
    if (haveHead && g_headTestRejects[g] + g_headTestPasses[g] >= 2000) {
        const unsigned tot = g_headTestRejects[g] + g_headTestPasses[g];
        LogF("rooms[TR%d]: head test -- %u passed, %u rejected (%u%% rejected) "
             "of the last %u portals",
             g + 4, g_headTestPasses[g], g_headTestRejects[g],
             (g_headTestRejects[g] * 100) / tot, tot);
        g_headTestRejects[g] = 0;
        g_headTestPasses[g]  = 0;
    }
}

// One detour per game: each has to call its own trampoline.
void __cdecl Detour_TR4() {
    if (g_base[0]) ForceAllRooms(0);
    g_hook[0].Original<Fn_RenderRooms>()();
}

void __cdecl Detour_TR5() {
    if (g_base[1]) ForceAllRooms(1);
    g_hook[1].Original<Fn_RenderRooms>()();
}

void* const kDetours[2] = {
    reinterpret_cast<void*>(&Detour_TR4),
    reinterpret_cast<void*>(&Detour_TR5),
};

} // namespace

void RoomCullUpdate() {
    // Nothing to do unless one of the two mechanisms is on.
    if (Cfg().portalHops <= 0 && !Cfg().drawAllRooms) return;

    for (int g = 0; g < 2; ++g) {
        if (g_base[g] || g_warned[g]) continue;

        const wchar_t* module = (g == 0) ? L"tomb4.dll" : L"tomb5.dll";
        HMODULE h = GetModuleHandleW(module);
        if (!h) continue;                     // not loaded yet; try next frame

        uint8_t* base = reinterpret_cast<uint8_t*>(h);

        // Pick the row for the build actually loaded. Without this the hook is
        // attempted at an address belonging to a different build: the prologue
        // check refuses it, correctly, but the log says only "wrong game build"
        // without saying which, and culling silently reverts to stock.
        const uint32_t stamp = ModuleStamp(base);
        g_addr[g] = nullptr;
        for (const GameAddrs& row : kGames)
            if (row.game == g && row.timestamp == stamp) { g_addr[g] = &row; break; }

        if (!g_addr[g]) {
            g_warned[g] = true;
            LogF("rooms: %S is build 0x%08X, which has no address table -- "
                 "TR%d culling unchanged. Known builds:", module, stamp, g + 4);
            for (const GameAddrs& row : kGames)
                if (row.game == g) LogF("rooms:   0x%08X", row.timestamp);
            continue;
        }

        if (!g_hook[g].Install(base + A(g).renderRooms, kDetours[g], 5,
                               A(g).prologue, A(g).prologueLen,
                               A(g).hookName)) {
            // Install logs the reason and refuses on a prologue mismatch, so a
            // different game build degrades to stock culling rather than a
            // corrupted instruction stream. Do not retry every frame.
            g_warned[g] = true;
            LogF("rooms: could not hook %S room renderer -- TR%d culling unchanged",
                 module, g + 4);
            continue;
        }

        g_base[g] = base;
        if (!g_logged[g]) {
            g_logged[g] = true;
            LogF("rooms: portal culling extended in %S build 0x%08X (list holds "
                 "%d rooms, hops %d, head test %s, dump key 0x%02X)",
                 module, A(g).timestamp, A(g).drawListMax, Cfg().portalHops,
                 Cfg().portalHeadTest ? "on" : "off", Cfg().roomDumpKey);
        }
    }
}

int LaraWaterStatus() {
    // Resolved independently of the culling hook: this must work with
    // PortalHops=0, and it is a plain read of a global rather than a patch, so
    // there is nothing to install.
    //
    // Selected by CurrentGame(), NOT by "whichever DLL is loaded". Both
    // tomb4.dll and tomb5.dll are mapped for the whole session -- the log shows
    // the room hook installing in both -- so taking the first module that
    // resolves would read TR4's global while TR5 is the game being played, and
    // report a water state belonging to nobody.
    //
    // Cached per game, since one process can run either.
    static const uint8_t* s_addr[2]  = { nullptr, nullptr };
    static bool           s_looked[2] = { false, false };

    const int g = CurrentGame();          // 0 = TR4, 1 = TR5, 2 = TR6
    if (g != 0 && g != 1) return -1;      // TR6 has no entry

    if (!s_looked[g]) {
        for (const GameAddrs& e : kGames) {
            if (e.game != g || e.laraWaterStatus == 0) continue;
            HMODULE h = GetModuleHandleW(e.module);
            if (!h) continue;
            uint8_t* base = reinterpret_cast<uint8_t*>(h);
            if (ModuleStamp(base) != e.timestamp) continue;
            s_addr[g] = base + e.laraWaterStatus;
            LogF("lara: water_status at %S+0x%06X (build 0x%08X)",
                 e.module, e.laraWaterStatus, e.timestamp);
            break;
        }
        // Only give up once this game's DLL is actually loaded, or an early
        // call would latch "unknown" for the whole session.
        if (s_addr[g] || GetModuleHandleW(g == 0 ? L"tomb4.dll" : L"tomb5.dll")) {
            s_looked[g] = true;
            if (!s_addr[g]) {
                LogF("lara: water_status address not known for TR%d on this "
                     "build -- the swimming exception is inactive", g + 4);
            }
        }
    }

    if (!s_addr[g]) return -1;
    return (int)*reinterpret_cast<const uint16_t*>(s_addr[g]);
}

bool CameraHeadroom(float& units) {
    if (!g_headroomValid) return false;
    units = g_headroom;
    return true;
}

void RoomCullShutdown() {
    for (int g = 0; g < 2; ++g) {
        if (g_base[g]) {
            g_hook[g].Remove();
            g_base[g] = nullptr;
        }
    }
}

} // namespace tr
