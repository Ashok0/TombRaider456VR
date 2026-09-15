#include "GameDll.h"
#include "Engine.h"
#include "Log.h"

#include <windows.h>
#include <cmath>
#include <cstdint>

namespace tr {
namespace {

// --- struct offsets, identical in tomb4.dll and tomb5.dll -------------------
//
// From tools\typedump.py. Only the offsets actually read are named; the structs
// are not redeclared, because a partial C++ struct that has to stay in step with
// two DLLs is a liability and an offset with the tool that produced it written
// beside it is not.
namespace off {
// lara_info. water_status is at +12, the same place TR1-3's PDBs put it -- which
// is the confirmation that the old memory-diff hunt found the right field.
constexpr uint32_t lara_water_status = 12;   // int16

// camera_info, 112 bytes. camera.pos is a GAME_VECTOR at +0:
//   +0 x (int32)  +4 y (int32)  +8 z (int32)  +12 room_number (int16)
constexpr uint32_t camera_pos_y    = 4;    // int32
constexpr uint32_t camera_pos_room = 12;   // int16

// ROOM_INFO, 304 bytes
constexpr uint32_t room_stride     = 304;
constexpr uint32_t room_maxceiling = 56;   // int32, the ceiling's Y
} // namespace off

// PrintRoomsList's prologue. Different in the two DLLs, which is why it travels
// in the row rather than sitting beside the hook:
//
//   TR4  48 8B C4     mov rax, rsp
//        41 54        push r12          -> 5 bytes, both position-independent
//   TR5  48 89 5C 24 10  mov [rsp+0x10], rbx  -> 5 bytes, position-independent
//
// Verified against the real .text by tools\verify_addresses.py, which also
// checks that each window ends on an instruction boundary and contains no
// RIP-relative operand.
const uint8_t kPrintRoomsListTR4[] = { 0x48, 0x8B, 0xC4, 0x41, 0x54 };
const uint8_t kPrintRoomsListTR5[] = { 0x48, 0x89, 0x5C, 0x24, 0x10 };

// Every RVA read out of the matching PDB, by name.
//
//   python tools\pdbdump.py <dir>\tomb4.dll draw_rooms w2v_matrix PrintRoomsList DrawSkyHD
//
// ONLY BUILDS WITH A PDB APPEAR HERE. The old table carried a second pair of
// rows for the 2025-09-10 build, derived by applying a uniform per-DLL shift to
// the addresses of the newer one and marked UNVERIFIED. That was a reasonable
// thing to do when the alternative was nothing; it is not reasonable now that
// the alternative is running pdbdump against that build's own PDB. An unknown
// build is reported and the culling stands down, which is the honest failure.
constexpr GameDllLayout kDlls[] = {
    { 0, 0x696B4999, L"tomb4.dll", "Tomb Raider IV",
      /* lara            */ 0x004F2E40,
      /* camera          */ 0x00663E40,
      /* room            */ 0x00663FC8,
      /* number_rooms    */ 0x00660810,
      /* draw_rooms      */ 0x0063DC60,
      /* number_draw_..  */ 0x0063DDFC,
      /* w2v_matrix      */ 0x00494840,
      /* phd_mxptr       */ 0x001B6D10,
      /* phd_winxmax     */ 0x004947D0,
      /* phd_winymax     */ 0x004947CC,
      /* outside         */ 0x0063DC20,
      /* outside_left    */ 0x0084E170,
      /* outside_right   */ 0x0084E160,
      /* outside_top     */ 0x0084E168,
      /* outside_bottom  */ 0x0084E164,
      /* BinocularOn     */ 0x001BB244,
      /* BinocularRange  */ 0x001BB24C,
      /* PrintRoomsList  */ 0x000C5160,
      /* S_GetObjectB..  */ 0x000B8210,
      /* DrawSkyHD       */ 0x000C4CA0,
      /* DrawNormalBin.. */ 0x000D1B00,
      /* DrawVCIHeadset  */ 0x000D17B0,
      /* DrawLabyrinth.. */ 0x000D1440,
      /* DrawNormalLas.. */ 0x000D1E40,
      /* DoInfraRedQuad  */ 0x000D21B0,
      kPrintRoomsListTR4, sizeof(kPrintRoomsListTR4) },

    { 1, 0x696B499C, L"tomb5.dll", "Tomb Raider V",
      /* lara            */ 0x004EE740,
      /* camera          */ 0x0066D0E0,
      /* room            */ 0x0065EC28,
      /* number_rooms    */ 0x0065B2F0,
      /* draw_rooms      */ 0x0063D6C0,
      /* number_draw_..  */ 0x0063D860,
      /* w2v_matrix      */ 0x004EF940,
      /* phd_mxptr       */ 0x001B2AD0,
      /* phd_winxmax     */ 0x004EF8CC,
      /* phd_winymax     */ 0x004EF8C8,
      /* outside         */ 0x0063D6A8,
      /* outside_left    */ 0x00847A64,
      /* outside_right   */ 0x00847A40,
      /* outside_top     */ 0x00847A5C,
      /* outside_bottom  */ 0x00847A58,
      /* BinocularOn     */ 0x001B6C90,
      /* BinocularRange  */ 0x001B6C98,
      /* PrintRoomsList  */ 0x000B9CA0,
      /* S_GetObjectB..  */ 0x000ABB20,
      /* DrawSkyHD       */ 0x000B97F0,
      /* DrawNormalBin.. */ 0x000C4DD0,
      /* DrawVCIHeadset  */ 0x000C4A80,
      /* DrawLabyrinth.. */ 0x000C4710,
      /* DrawNormalLas.. */ 0x000C5110,
      /* DoInfraRedQuad  */ 0x000C5480,
      kPrintRoomsListTR5, sizeof(kPrintRoomsListTR5) },
};

// TR6 is a different engine, so it cannot share GameDllLayout. Its embedded
// AMX native table names the function at +0x14E180 `IsPointInWater`, and
// mapGetPlayerPosition at +0x14E880 independently shows that the live player
// pointer is stored at +0xCB42D8 and that its position begins at +0x40. The
// water query itself reads gmapGMXCur from +0x4CC7238. Keep these together and
// build-gated: unlike the TR4/TR5 fields above, no matching private PDB is
// available for tomb6.dll.
constexpr uint32_t kTr6DllTimestamp       = 0x696B49A4;
constexpr uint32_t kTr6PlayerPointerRva   = 0x00CB42D8;
constexpr uint32_t kTr6PlayerPositionOff  = 0x00000040;
constexpr uint32_t kTr6CurrentGmxRva      = 0x04CC7238;
constexpr uint32_t kTr6IsPointInWaterRva  = 0x0014E180;

// TR6's ceiling reader uses the rendered chase-camera matrix and the GMX room
// headers because TR6 has no TR4/5 w2v_matrix or ROOM_INFO layout. Using the
// matrix position for both the world offset and the room query is important:
// gcamCamera is a separate legacy camera that can temporarily describe another
// point during TR6's offscreen render chain. These offsets are for the same
// timestamp-gated build above.
constexpr uint32_t kTr6CameraMatrixRva    = 0x0029DCC0;
constexpr uint32_t kTr6GmxRoomsOff        = 0x000001A0;
constexpr uint32_t kTr6GmxRoomCountOff    = 0x000007A0;
constexpr uint32_t kTr6RoomBoundsMinOff   = 0x000000A0;
constexpr uint32_t kTr6RoomBoundsMaxOff   = 0x000000B0;
constexpr uint32_t kTr6RoomIsFlipOff      = 0x00000218;
constexpr int32_t  kTr6MaxRooms           = 192;

using FnTr6IsPointInWater = int(__fastcall*)(const float* position,
                                             float* waterHeight);

const GameDllLayout* g_dll  = nullptr;
uint64_t             g_base = 0;
bool                 g_loggedNoRooms = false;
uint32_t             g_warnedStamp[2] = { 0, 0 };
bool                 g_loggedTr6WaterBinding = false;
bool                 g_warnedTr6WaterBuild = false;
bool                 g_loggedTr6CameraFrame = false;

template <typename T>
T Read(uint32_t rva) {
    return *reinterpret_cast<T*>(g_base + rva);
}

uint32_t ModuleStamp(uint64_t base) {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->FileHeader.TimeDateStamp;
}

int Tr6LaraWaterStatus() {
    HMODULE module = GetModuleHandleW(L"tomb6.dll");
    if (!module) return -1;

    const uint64_t base = reinterpret_cast<uint64_t>(module);
    const uint32_t stamp = ModuleStamp(base);
    if (stamp != kTr6DllTimestamp) {
        if (!g_warnedTr6WaterBuild) {
            g_warnedTr6WaterBuild = true;
            LogF("tr6 water: tomb6.dll build 0x%08X is unsupported (expected "
                 "0x%08X); automatic stick-pitch restore stands down",
                 stamp, kTr6DllTimestamp);
        }
        return -1;
    }

    // IsPointInWater assumes a level exists and immediately dereferences
    // gmapGMXCur. Its AMX wrapper normally guarantees that precondition; the
    // XInput hook can run at the title screen too, so enforce it here. A null
    // player is equally normal while a level is loading.
    const auto* gmx = *reinterpret_cast<const void* const*>(
        base + kTr6CurrentGmxRva);
    const auto* player = *reinterpret_cast<const uint8_t* const*>(
        base + kTr6PlayerPointerRva);
    if (!gmx || !player) return -1;

    // Copy the position before entering game code. The query only reads XYZ;
    // the fourth float in the player's position vector is not part of its API.
    const float* livePosition = reinterpret_cast<const float*>(
        player + kTr6PlayerPositionOff);
    const float position[3] = {
        livePosition[0], livePosition[1], livePosition[2]
    };
    float waterHeight = 0.0f;
    const auto isPointInWater = reinterpret_cast<FnTr6IsPointInWater>(
        base + kTr6IsPointInWaterRva);

    if (!g_loggedTr6WaterBinding) {
        g_loggedTr6WaterBinding = true;
        LogF("tr6 water: bound Lara position and IsPointInWater "
             "(build 0x%08X, player +0x%X, query +0x%X)",
             stamp, kTr6PlayerPointerRva, kTr6IsPointInWaterRva);
    }

    // TR4/TR5 expose a five-value water_status enum. TR6 exposes a geometric
    // predicate instead, so normalise it to the only distinction the input
    // policy needs: 0 = dry, 1 = in water.
    return isPointInWater(position, &waterHeight) ? 1 : 0;
}

bool Tr6CameraViewFrame(float rot[3][3], float pos[3]);

bool Tr6CameraHeadroom(float& units) {
    HMODULE module = GetModuleHandleW(L"tomb6.dll");
    if (!module) return false;

    const uint64_t base = reinterpret_cast<uint64_t>(module);
    if (ModuleStamp(base) != kTr6DllTimestamp) return false;

    auto** currentGmx = reinterpret_cast<uint8_t**>(
        base + kTr6CurrentGmxRva);
    if (!currentGmx || !*currentGmx) return false;

    const int32_t roomCount = *reinterpret_cast<int32_t*>(
        *currentGmx + kTr6GmxRoomCountOff);
    if (roomCount <= 0 || roomCount > kTr6MaxRooms) return false;

    auto** rooms = reinterpret_cast<uint8_t**>(
        *currentGmx + kTr6GmxRoomsOff);

    // WorldLockOffset has just read this same frame. Read it again instead of
    // mixing its position with gcamCamera: even a small disagreement makes the
    // headroom refer to a different room than the eye displacement.
    float cameraRot[3][3]{};
    float camera[3]{};
    if (!Tr6CameraViewFrame(cameraRot, camera)) return false;

    float availableHeadroom = 0.0f;
    for (int32_t room = 0; room < roomCount; ++room) {
        const uint8_t* header = rooms[room];
        if (!header || *reinterpret_cast<const int32_t*>(
                           header + kTr6RoomIsFlipOff) != 0) {
            continue;
        }

        const float* boundsMin = reinterpret_cast<const float*>(
            header + kTr6RoomBoundsMinOff);
        const float* boundsMax = reinterpret_cast<const float*>(
            header + kTr6RoomBoundsMaxOff);
        if (camera[0] < boundsMin[0] || camera[0] > boundsMax[0]
            || camera[1] < boundsMin[1] || camera[1] > boundsMax[1]
            || camera[2] < boundsMin[2] || camera[2] > boundsMax[2]) {
            continue;
        }

        // TR coordinates are Y-down, so BoundsMin.y is the room ceiling.
        const float headroom = camera[1] - boundsMin[1];
        if (headroom > availableHeadroom && headroom <= 32768.0f) {
            availableHeadroom = headroom;
        }
    }

    if (!(availableHeadroom > 0.0f)) return false;
    units = availableHeadroom;
    return true;
}

bool Tr6CameraViewFrame(float rot[3][3], float pos[3]) {
    HMODULE module = GetModuleHandleW(L"tomb6.dll");
    if (!module) return false;

    const uint64_t base = reinterpret_cast<uint64_t>(module);
    if (ModuleStamp(base) != kTr6DllTimestamp) return false;

    // TR6's camera is a conventional column-major world-to-view matrix. The
    // game renders down -Z, while WorldLockOffset and the TR4/TR5 reader use
    // phd's +Z-forward camera frame. Negating row 2 converts only that frame
    // convention; the camera position is recovered from the original rigid
    // view as -R^T*t before the conversion.
    const float* view = reinterpret_cast<const float*>(
        base + kTr6CameraMatrixRva);
    const float tx = view[12];
    const float ty = view[13];
    const float tz = view[14];
    pos[0] = -(view[0] * tx + view[1] * ty + view[2]  * tz);
    pos[1] = -(view[4] * tx + view[5] * ty + view[6]  * tz);
    pos[2] = -(view[8] * tx + view[9] * ty + view[10] * tz);

    for (int row = 0; row < 3; ++row) {
        const float sign = (row == 2) ? -1.0f : 1.0f;
        for (int col = 0; col < 3; ++col) {
            rot[row][col] = sign * view[col * 4 + row];
        }
    }

    // The global is zero before the first gameplay camera update. Check all
    // three rows because a partially written or non-rigid matrix would turn
    // head translation into an arbitrary world displacement.
    for (int row = 0; row < 3; ++row) {
        const float len2 = rot[row][0] * rot[row][0]
                         + rot[row][1] * rot[row][1]
                         + rot[row][2] * rot[row][2];
        if (!(len2 > 0.9f && len2 < 1.1f)) return false;
    }
    if (!std::isfinite(pos[0]) || !std::isfinite(pos[1])
        || !std::isfinite(pos[2])) {
        return false;
    }

    if (!g_loggedTr6CameraFrame) {
        g_loggedTr6CameraFrame = true;
        LogF("tr6 camera: world frame bound at tomb6.dll+0x%X; "
             "world-space head offset and ceiling clamp active",
             kTr6CameraMatrixRva);
    }
    return true;
}

} // namespace

bool GameDllUpdate() {
    // WHICH DLL, AND WHY NOT "THE ONE THAT IS LOADED"
    //
    // Both game DLLs are resident for the whole session, so GetModuleHandleW
    // succeeds for both at all times and "the first one that resolves" always
    // means tomb4.dll. CurrentGame() is the selector the engine itself uses:
    // 0 = TR4, 1 = TR5, 2 = TR6.
    //
    // TR6 has no row. It is a different engine -- different room structures,
    // different renderer -- and its culling is not addressed by any of this.
    const int game = CurrentGame();
    if (game != 0 && game != 1) {
        g_dll  = nullptr;
        g_base = 0;
        return false;
    }

    // Already bound to the right one, and it has not moved.
    if (g_dll && g_dll->game == game &&
        GetModuleHandleW(g_dll->module) == reinterpret_cast<HMODULE>(g_base))
        return true;

    g_dll  = nullptr;
    g_base = 0;

    const wchar_t* module = (game == 0) ? L"tomb4.dll" : L"tomb5.dll";
    HMODULE h = GetModuleHandleW(module);
    if (!h) return false;

    const uint64_t base  = reinterpret_cast<uint64_t>(h);
    const uint32_t stamp = ModuleStamp(base);

    for (const GameDllLayout& d : kDlls) {
        if (d.game != game || d.timestamp != stamp) continue;
        g_dll  = &d;
        g_base = base;
        // Logged on every rebind, not once: the player can switch games without
        // restarting, and which DLL we are reading is exactly the thing that
        // goes wrong silently. The game index is printed alongside so the two
        // can be checked against each other at a glance.
        LogF("gamedll: bound to %S (%s, build 0x%08X, game=%d) at %p",
             d.module, d.name, stamp, game, (void*)base);
        return true;
    }

    // Unknown build. Report it once per stamp, name what IS known, and stand
    // down -- every address below would otherwise be a guess, and one of them
    // is a hook target.
    if (g_warnedStamp[game] != stamp) {
        g_warnedStamp[game] = stamp;
        LogF("gamedll: %S is build 0x%08X, which has no address table. Room "
             "culling, the ceiling clamp and the swimming exception are all "
             "inactive for it. Known builds:", module, stamp);
        for (const GameDllLayout& d : kDlls)
            if (d.game == game) LogF("gamedll:   0x%08X  (%s)", d.timestamp, d.name);
        Log("gamedll: to add one, run tools\\pdbdump.py against that build's PDB "
            "and tools\\verify_addresses.py to check the result.");
    }
    return false;
}

const GameDllLayout* GameDllBound() { return g_dll; }
uint64_t             GameDllBase()  { return g_base; }

int LaraWaterStatus() {
    if (CurrentGame() == 2) return Tr6LaraWaterStatus();
    if (!g_dll) return -1;
    return Read<int16_t>(g_dll->lara + off::lara_water_status);
}

bool IsOpticsZoomed() {
    if (!g_dll) return false;

    // The exact two fields ProcessLooking itself checks before deciding whether
    // the right stick drives ordinary look-around or the zoom camera --
    // decompiled and confirmed, not guessed. BinocularRange is a ramp counter
    // nonzero for the whole entering/leaving transition; BinocularOn goes
    // negative on the way out. Either being nonzero means the zoom camera, not
    // the normal one, currently owns the stick.
    return Read<int32_t>(g_dll->binocularOn)    != 0
        || Read<int32_t>(g_dll->binocularRange) != 0;
}

bool CameraHeadroom(float& units) {
    units = 0.0f;
    if (CurrentGame() == 2) return Tr6CameraHeadroom(units);
    if (!g_dll) return false;

    // `room` is a POINTER to the rooms array, null until a level is loaded.
    auto* rooms = Read<uint8_t*>(g_dll->room);
    const int16_t numRooms = Read<int16_t>(g_dll->numberRooms);
    if (!rooms || numRooms <= 0) {
        if (!g_loggedNoRooms) {
            g_loggedNoRooms = true;
            Log("gamedll: no level loaded yet -- ceiling clamp stands down until "
                "there is one (this is normal at the title screen)");
        }
        return false;
    }

    const int16_t roomNo = Read<int16_t>(g_dll->camera + off::camera_pos_room);
    if (roomNo < 0 || roomNo >= numRooms) return false;

    const int32_t camY = Read<int32_t>(g_dll->camera + off::camera_pos_y);
    const int32_t ceil =
        *reinterpret_cast<int32_t*>(rooms + static_cast<uint32_t>(roomNo) * off::room_stride
                                          + off::room_maxceiling);

    // TR world space is Y-DOWN: the ceiling has a SMALLER Y than anything below
    // it, so headroom is (camera Y - ceiling Y) and is positive when the camera
    // is below the ceiling. Getting this backwards would produce a negative
    // number that the check below rejects, which is the intended failure.
    const int32_t headroom = camY - ceil;

    // Sanity. A sector is 1024 units and Lara is about 762 tall, so a real
    // headroom is a few hundred to a few thousand units. Anything outside that
    // means we are reading a room the camera is not really in -- a cutscene
    // camera, a level transition, a stale index -- so report "unknown" rather
    // than clamp the player's head on a bad number.
    if (headroom <= 0 || headroom > 32768) return false;

    units = static_cast<float>(headroom);
    return true;
}

bool CameraViewFrame(float rot[3][3], float pos[3]) {
    if (CurrentGame() == 2) return Tr6CameraViewFrame(rot, pos);
    if (!g_dll) return false;

    const int32_t* m = reinterpret_cast<const int32_t*>(g_base + g_dll->w2vMatrix);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            rot[i][j] = static_cast<float>(m[i * 4 + j]) * (1.0f / 16384.0f);
        }
        pos[i] = static_cast<float>(m[i * 4 + 3]);
    }

    // Before the first camera update the matrix is zeroed, and a zero rotation
    // would send every offset to the camera. One row of unit length tells "ready"
    // from "not yet"; the tolerance only has to cover 1/16384 quantisation.
    const float len2 = rot[0][0] * rot[0][0] + rot[0][1] * rot[0][1]
                     + rot[0][2] * rot[0][2];
    return len2 > 0.9f && len2 < 1.1f;
}

void GameDllShutdown() {
    g_dll  = nullptr;
    g_base = 0;
    g_loggedTr6WaterBinding = false;
    g_warnedTr6WaterBuild = false;
    g_loggedTr6CameraFrame = false;
}

} // namespace tr
