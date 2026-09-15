// GameDll.h -- the game state that lives in tomb4.dll / tomb5.dll rather than in
// tomb456.exe, and the address table that reaches it.
//
// This file owns the BINDING (which DLL, where it is loaded, which build, and
// every RVA inside it). Two small readers live here as well -- Lara's water
// status and the camera's headroom -- because they are field reads and nothing
// more. The room culling that uses the rest of the table is in PortalCull.cpp.
//
// THIS REPLACES RoomCull.h, AND THE REASON IS THE PDBs
//
// Everything here used to be reverse engineered: `FUN_18002ea30` for the portal
// traversal, `DAT_18063dc60` for the draw list, a whole-data-section memory diff
// to find lara.water_status. tomb4.dll and tomb5.dll ship PRIVATE PDBs, so all
// of it now comes out of dbghelp by name:
//
//   python tools\pdbdump.py  <dir>\tomb4.dll  draw_rooms w2v_matrix PrintRoomsList
//   python tools\typedump.py <dir>\tomb4.dll  ROOM_INFO camera_info
//   python tools\verify_addresses.py          re-derives and diffs all of them
//
// Every address the old table had came back identical, which is the cross-check
// that says the reverse engineering was right -- including the two that were
// hardest to trust:
//
//   * `camX/camY/camZ` = 0x0049484C/5C/6C are w2v_matrix[3], [7] and [11].
//     They were three separate globals in the old table; they are one matrix.
//   * `laraWaterStatus` = 0x004F2E4C is `lara` + 12, exactly where TR1-3's PDBs
//     put lara_info::water_status. The memory diff found the right field.
//
// TR4 and TR5 have IDENTICAL struct layouts -- 304-byte ROOM_INFO with
// door/x/y/z/maxceiling/bound_active at +8/+40/+44/+48/+56/+75 and the clip rect
// at +76..+82, 112-byte camera_info -- so there is one struct description and a
// per-build address table. TR6 is a different engine and has no row.
#pragma once

#include <cstdint>

namespace tr {

// One row per (game, build). Rows are searched by BOTH, because tomb4.dll and
// tomb5.dll are both resident all session and because the HD pack ships its own
// DLLs: matching on the module name alone would land every address in the
// middle of unrelated code.
struct GameDllLayout {
    int            game;        // 0 = TR4, 1 = TR5
    uint32_t       timestamp;   // PE TimeDateStamp
    const wchar_t* module;
    const char*    name;

    // --- state read by LaraWaterStatus / CameraHeadroom ---------------------
    uint32_t lara;              // lara_info
    uint32_t camera;            // camera_info
    uint32_t room;              // ROOM_INFO*  (null until a level is loaded)
    uint32_t numberRooms;       // int16

    // --- the visible-set state the culling works on ------------------------
    uint32_t drawRooms;         // int16[200]  -- the draw list
    uint32_t numberDrawRooms;   // int32       -- how many of it are in use
    uint32_t w2vMatrix;         // int32[12]   -- world -> view, see PortalCull
    uint32_t phdMxptr;          // int32**     -- top of the matrix stack
    uint32_t phdWinXmax;        // int32       -- screen width  - 1
    uint32_t phdWinYmax;        // int32       -- screen height - 1

    // --- the sky/horizon clip rect ------------------------------------------
    uint32_t outside;           // int32, non-zero when an outdoor room is visible
    uint32_t outsideLeft;
    uint32_t outsideRight;
    uint32_t outsideTop;
    uint32_t outsideBottom;

    // --- state read by IsOpticsZoomed ----------------------------------------
    uint32_t binocularOn;       // int32  0 = off, transitions through here
    uint32_t binocularRange;    // int32  ramp counter, nonzero while entering/leaving

    // --- hook targets -------------------------------------------------------
    uint32_t printRoomsList;    // void PrintRoomsList(void)
    uint32_t sGetObjectBounds;  // int  S_GetObjectBounds(int16* bounds)
    uint32_t drawSkyHD;         // void DrawSkyHD(void) -- HD sky/horizon dome

    // --- the optic overlays, stubbed rather than hooked (see Overlay.h) -------
    //
    // All five are void, all five are called only from DrawBinoculars, and no
    // caller reads a return value, so each is switched off with one 0xC3 at its
    // entry. The aiming dot is NOT among them -- DrawBinoculars draws it inline
    // as a sprite after DrawGameInfo, so it survives all five being stubbed.
    uint32_t drawNormalBinocs;      // the binocular vignette
    uint32_t drawVCIHeadset;        // TR5's VCI visor overlay
    uint32_t drawLabyrinthFishEye;  // the Labyrinth fisheye overlay
    uint32_t drawNormalLaserSight;  // the scope frame and its reticle lines
    uint32_t doInfraRedQuad;        // the full-screen red wash behind both

    // PrintRoomsList's prologue differs between TR4 and TR5, so it travels with
    // the row. S_GetObjectBounds' is identical in both and lives in
    // PortalCull.cpp beside the hook that uses it.
    const uint8_t* printRoomsListPrologue;
    uint32_t       printRoomsListStolen;
};

// Resolve tomb4.dll / tomb5.dll for the game currently selected. Cheap and
// idempotent; call once per frame. Returns true once a supported build is bound.
bool GameDllUpdate();

// The row for the DLL currently bound, or null. The pointer is stable for the
// life of the process; the BINDING is not, so anything derived from it must be
// re-checked every frame.
const GameDllLayout* GameDllBound();

// Load address of that DLL, or 0.
uint64_t GameDllBase();

// Lara's own water state, or -1 when it cannot be known.
//
//   0 ABOVE_WATER   1 UNDERWATER   2 SURFACE   3 FLYCHEAT   4 WADE
//
// TR4/TR5 return the enum above, read straight from the DLL's `lara` global.
// TR6 returns a normalised 0/1 by asking its own IsPointInWater routine about
// the live player's position. Both paths follow Lara rather than the camera --
// which is the whole point. The camera trails behind and above her and sits in
// air during a surface swim, so camera-derived water tests fail exactly when
// stick pitch is needed most.
int LaraWaterStatus();

// True while Lara is looking through an optic -- binoculars, or a weapon
// combined with a laser sight -- and the game's own zoom camera
// (BinocularCamera_TR4/TR5) has taken over the view instead of the normal one.
//
// Read from BinocularOn and BinocularRange in the game DLL, the same two
// fields ProcessLooking itself checks (decompiled and confirmed, not guessed)
// before deciding whether the right stick drives ordinary look-around or the
// zoom camera: BinocularRange is a ramp counter that is nonzero while entering
// or leaving the zoomed view, and BinocularOn goes negative on the way out.
// Either being nonzero means the zoom camera owns the stick, so this reports
// true for the whole lifetime of the zoom -- entering, steady, and leaving --
// not only the fully-settled middle of it.
//
// False (rather than -1, since there is no "unknown" state here the way there
// is for water) when no game DLL is bound.
bool IsOpticsZoomed();

// World units from the game camera up to its room's ceiling.
//
// Returns false when it cannot be known -- nothing bound, no level loaded, an
// out-of-range room index, or a result that fails its own sanity check. Callers
// MUST treat false as "unknown" and never as "no headroom": clamping the head to
// the floor because a menu was open would be worse than not clamping at all.
//
// Derived from the room's bounding box (ROOM_INFO::maxceiling, +56) rather than
// from the floor data under the camera, so it is exact in a uniformly low room
// -- tunnels, crawlspaces, the places the clamp exists for -- and over-generous
// in a room with one tall section. Over-generous is the safe direction.
//
// NOTE: this used to be republished as a side effect of the culling hook, so it
// went stale the moment culling was off. It is now computed on demand from the
// camera's own room, like everything else here.
bool CameraHeadroom(float& units);

// The game camera's world-to-view frame: `rot` is w2v_matrix's rotation with its
// rows unscaled to unit length (phd view space -- X right, Y down, +Z FORWARD),
// and `pos` is the camera's world position.
//
// The translation columns are the camera's WORLD POSITION, unscaled -- not the
// -R*p a textbook view matrix carries. That is the same reading PortalCull.cpp
// works from, and getting it wrong is what made the first head-frustum test
// scale its error with world coordinates.
//
// False until the matrix holds a real rotation, which it does not before the
// first camera update of a level. A caller MUST honour that: a zeroed rotation
// would collapse every offset onto the camera itself.
bool CameraViewFrame(float rot[3][3], float pos[3]);

// Drop the binding. Called from RemoveHooks.
void GameDllShutdown();

} // namespace tr
