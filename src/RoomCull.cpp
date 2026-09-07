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

namespace tr {

// Published by Hooks.cpp: the world -> eye matrix actually injected last frame.
// This is what makes the head test possible; the engine's own view matrix is
// the game camera's and does not rotate with your head.
bool LastHeadView(Affine& out);

namespace {

struct GameAddrs {
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
};

// TR4: mov rax,rsp + push r12.   TR5: mov [rsp+10h],rbx.
const uint8_t kPrologueTR4[] = { 0x48, 0x8B, 0xC4, 0x41, 0x54 };
const uint8_t kPrologueTR5[] = { 0x48, 0x89, 0x5C, 0x24, 0x10 };

// Indexed by CurrentGame(): 0 = TR4, 1 = TR5.
const GameAddrs kGames[2] = {
    { L"tomb4.dll", "TR4!DrawRoomList",
      0x000C5160, kPrologueTR4, sizeof(kPrologueTR4),
      0x0063DC60, 0x0063DDFC, 200, 0x00660810, 0x00663FC8,
      0x004947D0, 0x004947CC, 0x0049484C, 0x0049485C, 0x0049486C },
    { L"tomb5.dll", "TR5!DrawRoomList",
      0x000B9CA0, kPrologueTR5, sizeof(kPrologueTR5),
      0x0063D6C0, 0x0063D860, 200, 0x0065B2F0, 0x0065EC28,
      0x004EF8CC, 0x004EF8C8, 0x004EF94C, 0x004EF95C, 0x004EF96C },
};

constexpr size_t kRoomStride = 0x130;

// ROOM_INFO fields, confirmed against GetFloor (tomb5.dll!FUN_1800101f0):
//   0x08  portal list: int16 count, then 32 bytes per portal
//   0x28  world X      0x30  world Z
//   0x4C..0x52  render clip rect (left, right, top, bottom) as shorts
//   0x68  flipped_room, -1 when the room is not half of a flip pair
constexpr size_t kOffPortals  = 0x08;
constexpr size_t kOffX        = 0x28;
constexpr size_t kOffY        = 0x2C;
constexpr size_t kOffZ        = 0x30;
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

void ForceAllRooms(int g) {
    uint8_t* base = g_base[g];
    const GameAddrs& a = kGames[g];
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
                    for (int e = 0; e < Cfg().excludeCount; ++e)
                        if (Cfg().excludeRooms[e] == adj) { excluded = true; break; }
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
        for (int i = 0; i < n && count < kMaxRooms; ++i)
            list[count++] = (int16_t)cand[i].room;
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

        HMODULE h = GetModuleHandleW(kGames[g].module);
        if (!h) continue;                     // not loaded yet; try next frame

        uint8_t* base = reinterpret_cast<uint8_t*>(h);
        if (!g_hook[g].Install(base + kGames[g].renderRooms, kDetours[g], 5,
                               kGames[g].prologue, kGames[g].prologueLen,
                               kGames[g].hookName)) {
            // Install logs the reason and refuses on a prologue mismatch, so a
            // different game build degrades to stock culling rather than a
            // corrupted instruction stream. Do not retry every frame.
            g_warned[g] = true;
            LogF("rooms: could not hook %S room renderer -- TR%d culling unchanged",
                 kGames[g].module, g + 4);
            continue;
        }

        g_base[g] = base;
        if (!g_logged[g]) {
            g_logged[g] = true;
            LogF("rooms: portal culling extended in %S (list holds %d rooms, "
                 "hops %d, head test %s)",
                 kGames[g].module, kGames[g].drawListMax, Cfg().portalHops,
                 Cfg().portalHeadTest ? "on" : "off");
        }
    }
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
