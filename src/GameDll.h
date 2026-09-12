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

    // --- hook targets -------------------------------------------------------
    uint32_t printRoomsList;    // void PrintRoomsList(void)
    uint32_t sGetObjectBounds;  // int  S_GetObjectBounds(int16* bounds)

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
// Read straight from the DLL's `lara` global, so it is Lara's state rather than
// the camera's -- which is the whole point. The camera trails behind and above
// her and sits in the air room during a surface swim, so anything derived from
// the camera's room reports dry exactly when it matters most.
int LaraWaterStatus();

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

// Drop the binding. Called from RemoveHooks.
void GameDllShutdown();

} // namespace tr
