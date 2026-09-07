#include "Gamepad.h"
#include "Engine.h"
#include "Config.h"
#include "VRSystem.h"
#include "Log.h"

#include <windows.h>
#include <cmath>
#include <cstring>

namespace tr {
namespace {

// --- XInput, declared locally so no SDK header is needed ---------------------
#pragma pack(push, 1)
struct XGamepad {
    uint16_t wButtons;
    uint8_t  bLeftTrigger;
    uint8_t  bRightTrigger;
    int16_t  sThumbLX;
    int16_t  sThumbLY;
    int16_t  sThumbRX;
    int16_t  sThumbRY;
};
struct XState {
    uint32_t dwPacketNumber;
    XGamepad Gamepad;
};
#pragma pack(pop)

// Standard XInput button bits, exactly as inputUpdate() decodes them.
enum : uint16_t {
    XB_DPAD_UP        = 0x0001,
    XB_DPAD_DOWN      = 0x0002,
    XB_DPAD_LEFT      = 0x0004,
    XB_DPAD_RIGHT     = 0x0008,
    XB_START          = 0x0010,
    XB_BACK           = 0x0020,
    XB_LEFT_THUMB     = 0x0040,
    XB_RIGHT_THUMB    = 0x0080,
    XB_LEFT_SHOULDER  = 0x0100,
    XB_RIGHT_SHOULDER = 0x0200,
    XB_A              = 0x1000,
    XB_B              = 0x2000,
    XB_X              = 0x4000,
    XB_Y              = 0x8000,
};

typedef uint32_t (__stdcall* Fn_XInputGetState)(uint32_t, XState*);

Fn_XInputGetState* g_slot     = nullptr;   // the engine's _XInputGetState global
Fn_XInputGetState  g_original = nullptr;   // the real one, for a physical pad
bool               g_installed = false;
uint32_t           g_packet    = 0;
uint64_t           g_lastRaw[2] = { 0, 0 };
bool               g_loggedOnce = false;

int16_t Axis(float v) {
    if (v >  1.0f) v =  1.0f;
    if (v < -1.0f) v = -1.0f;
    const float s = v * 32767.0f;
    return static_cast<int16_t>(s);
}

uint8_t Trig(float v) {
    if (v > 1.0f) v = 1.0f;
    if (v < 0.0f) v = 0.0f;
    return static_cast<uint8_t>(v * 255.0f);
}

// Build the pad state from the two controllers.
//
// The mapping targets the scheme the game expects:
//   Move        left stick        Jump    A      Roll   B
//   Look        right stick       Action  Y      Shoot  RT
//   Duck        LB                Equip   LT     Sprint L3
//   Walk        LS + RB           System  X      Photo  LB + RB
//
// Two of these are deliberately not the flat-screen defaults, because they suit
// VR hands better:
//
//   Walk is the RIGHT GRIP rather than a face button, so it can be held while
//   the left thumb keeps moving. The game binds Walk to XInput X, so the grip
//   emits X. It emits RIGHT_SHOULDER as well, because Photo Mode is LB + RB and
//   that chord has to keep working; X and RB never collide in practice.
//
//   System is the left hand's lower face button, sending BACK. Touch has no
//   Start or Back of its own, and putting either on a chord made it awkward to
//   reach mid-play.
void BuildState(XState& out) {
    VRSystem::HandState h[2];
    VR().ReadControllers(h);

    std::memset(&out, 0, sizeof(out));
    out.dwPacketNumber = ++g_packet;

    const VRSystem::HandState& L = h[0];
    const VRSystem::HandState& R = h[1];

    uint16_t b = 0;
    if (R.btnLower)   b |= XB_A;               // Jump
    if (R.btnUpper)   b |= XB_B;               // Roll
    if (L.btnUpper)   b |= XB_Y;               // Action
    if (L.stickClick) b |= XB_LEFT_THUMB;      // Sprint
    if (R.stickClick) b |= XB_RIGHT_THUMB;

    // System (BACK) on the left hand's lower face button, or START if the ini
    // asks for the pause menu there instead.
    if (L.btnLower) {
        b |= Cfg().gamepadMenuUsesBack ? XB_BACK : XB_START;
    }

    // Duck, and the left half of the Photo Mode chord.
    if (L.grip > 0.5f) b |= XB_LEFT_SHOULDER;

    // Walk modifier. X is what the game binds Walk to; RIGHT_SHOULDER is also
    // set so that LB + RB still reaches Photo Mode.
    if (R.grip > 0.5f) b |= static_cast<uint16_t>(XB_X | XB_RIGHT_SHOULDER);

    out.Gamepad.wButtons      = b;
    out.Gamepad.bLeftTrigger  = Trig(L.trigger);   // Equip weapon
    out.Gamepad.bRightTrigger = Trig(R.trigger);   // Shoot
    out.Gamepad.sThumbLX      = Axis(L.stickX);
    out.Gamepad.sThumbLY      = Axis(L.stickY);
    out.Gamepad.sThumbRX      = Axis(R.stickX);
    out.Gamepad.sThumbRY      = Axis(R.stickY);

    // One-shot dump of the raw legacy masks. Touch's button ids vary between
    // runtimes, so if a button lands in the wrong place this says which mask it
    // actually set rather than costing a play session to find out.
    if (Cfg().gamepadLogButtons) {
        for (int i = 0; i < 2; ++i) {
            if (h[i].rawPressed != g_lastRaw[i]) {
                g_lastRaw[i] = h[i].rawPressed;
                LogF("pad: %s raw=0x%016llX stick=(%+.2f,%+.2f) trig=%.2f grip=%.2f",
                     i == 0 ? "L" : "R",
                     static_cast<unsigned long long>(h[i].rawPressed),
                     h[i].stickX, h[i].stickY, h[i].trigger, h[i].grip);
            }
        }
    }
}

uint32_t __stdcall Detour_XInputGetState(uint32_t userIndex, XState* state) {
    if (!state) return ERROR_BAD_ARGUMENTS;

    // Only pad 0 is ours. Let the engine see the other three as absent, or as
    // whatever real hardware reports.
    if (userIndex != 0) {
        return g_original ? g_original(userIndex, state)
                          : ERROR_DEVICE_NOT_CONNECTED;
    }

    if (!Cfg().gamepadEnabled || !VR().active()) {
        return g_original ? g_original(userIndex, state)
                          : ERROR_DEVICE_NOT_CONNECTED;
    }

    XState mine{};
    BuildState(mine);

    // Merge a physical pad if one is plugged in, so it keeps working.
    if (g_original) {
        XState real{};
        if (g_original(0, &real) == ERROR_SUCCESS) {
            mine.Gamepad.wButtons = static_cast<uint16_t>(
                mine.Gamepad.wButtons | real.Gamepad.wButtons);
            if (real.Gamepad.bLeftTrigger  > mine.Gamepad.bLeftTrigger)
                mine.Gamepad.bLeftTrigger  = real.Gamepad.bLeftTrigger;
            if (real.Gamepad.bRightTrigger > mine.Gamepad.bRightTrigger)
                mine.Gamepad.bRightTrigger = real.Gamepad.bRightTrigger;
            if (std::abs((int)real.Gamepad.sThumbLX) > std::abs((int)mine.Gamepad.sThumbLX))
                mine.Gamepad.sThumbLX = real.Gamepad.sThumbLX;
            if (std::abs((int)real.Gamepad.sThumbLY) > std::abs((int)mine.Gamepad.sThumbLY))
                mine.Gamepad.sThumbLY = real.Gamepad.sThumbLY;
            if (std::abs((int)real.Gamepad.sThumbRX) > std::abs((int)mine.Gamepad.sThumbRX))
                mine.Gamepad.sThumbRX = real.Gamepad.sThumbRX;
            if (std::abs((int)real.Gamepad.sThumbRY) > std::abs((int)mine.Gamepad.sThumbRY))
                mine.Gamepad.sThumbRY = real.Gamepad.sThumbRY;
        }
    }

    *state = mine;
    return ERROR_SUCCESS;
}

} // namespace

void GamepadUpdate() {
    if (!Cfg().gamepadEnabled) return;

    if (!g_slot) {
        g_slot = reinterpret_cast<Fn_XInputGetState*>(XInputGetStateSlot());
        if (!g_slot) return;
    }

    // WinMain resolves XInput before the render loop starts, but install
    // defensively: if the slot is still null we simply try again next frame.
    Fn_XInputGetState cur = *g_slot;
    if (cur == &Detour_XInputGetState) return;          // already ours

    if (cur) g_original = cur;                          // remember the real one
    *g_slot = &Detour_XInputGetState;

    if (!g_installed) {
        g_installed = true;
        LogF("pad: Touch controllers presented as an Xbox pad "
             "(_XInputGetState %p -> %p, real %p)",
             (void*)g_slot, (void*)&Detour_XInputGetState, (void*)g_original);
    }
    if (!g_loggedOnce) {
        g_loggedOnce = true;
        LogF("pad: move=Lstick look=Rstick jump=A(R lower) roll=B(R upper) "
             "action=Y(L upper) system=%s(L lower) walk=LS+RB(R grip) "
             "duck=LB(L grip) equip=LT shoot=RT sprint=L3 photo=LB+RB",
             Cfg().gamepadMenuUsesBack ? "BACK" : "START");
    }
}

void GamepadShutdown() {
    if (g_slot && *g_slot == &Detour_XInputGetState) {
        *g_slot = g_original;
        Log("pad: _XInputGetState restored");
    }
    g_slot = nullptr;
    g_original = nullptr;
    g_installed = false;
}

} // namespace tr
