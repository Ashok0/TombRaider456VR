from pathlib import Path
import sys

root = Path('C:/dev/TombRaider123VR')
updates = {}
def edit(relative, old, new):
    p = root / relative
    text = updates.get(p, p.read_text(encoding='utf-8-sig'))
    assert text.count(old) == 1, (relative, old[:100], text.count(old))
    updates[p] = text.replace(old, new)

edit('src/FirstPerson.h', 'bool jumpPressed);', 'bool jumpPressed, bool walkPressed = false);')
edit('src/Gamepad.cpp', 'FirstPersonInput(lx, ly, rx, shifted, jumpPressed);',
     'const bool walkPressed = (mine.Gamepad.wButtons & XB_X) != 0;\n    FirstPersonInput(lx, ly, rx, shifted, jumpPressed, walkPressed);')
edit('src/FirstPerson.cpp', 'bool g_jumpPressed = false;',
     'bool g_jumpPressed = false;\nbool g_walkPressed = false; // Effective RG/Walk, before synthetic backward walking.')
edit('src/FirstPerson.cpp', 'bool jumpPressed) {', 'bool jumpPressed, bool walkPressed) {')
edit('src/FirstPerson.cpp', 'g_jumpPressed = jumpPressed;', 'g_jumpPressed = jumpPressed;\n    g_walkPressed = walkPressed;')
edit('src/FirstPerson.cpp', '    g_jumpPressed = false;', '    g_jumpPressed = g_walkPressed = false;')
edit('src/FirstPerson.cpp', 'explicit DirectionalJumpAxesScope(uint8_t* item) {',
     'explicit DirectionalJumpAxesScope(uint8_t* item, bool backHop = false) {\n        lockHeading=backHop; // Keep native STOP facing the HMD during a back-hop.')
edit('src/FirstPerson.cpp', 'void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {\n    using namespace locomotion;',
     'void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {\n    using namespace locomotion;\n    bool backHopRequested=false;')
edit('src/FirstPerson.cpp', '        const bool ground = CanWalk(item);',
     '        // FASTBACK is a committed native hop, not a continuous ground gait.\n        const bool ground = CanWalk(item) && state!=5;')
mapping = Path('C:/dev/TombRaider456VR/src/FirstPerson.cpp').read_text()
start = mapping.index('                    uint64_t action = MovementAction(g_manualLocal, preparingJump);')
end = mapping.index('                    auto& pos =', start)
replacement = mapping[start:end].rstrip().replace('uint64_t action', 'uint32_t action').replace('PhysicalCrouchHoldScope', 'PhysicalCrouchInputScope')
edit('src/FirstPerson.cpp', '                    const uint32_t action = MovementAction(g_manualLocal, preparingJump);', replacement)
edit('src/FirstPerson.cpp', 'if (ground && !preparingJump)\n                        g_groundMoveAction = action;',
     'if (ground && !preparingJump && !backHopRequested)\n                        g_groundMoveAction = action;')
edit('src/FirstPerson.cpp', 'if (ground && !preparingJump &&\n                        Cfg().firstPersonMovementStabilization)',
     'if (ground && !preparingJump && !backHopRequested &&\n                        Cfg().firstPersonMovementStabilization)')
edit('src/FirstPerson.cpp', '    if (g_groundMoveAction) PrepareGroundDirection(item,g_groundMoveAction);',
     '    if (g_groundMoveAction) PrepareGroundDirection(item,g_groundMoveAction);\n    // Native STOP owns rear-floor clearance; the hop keeps its native motion.\n    if (backHopRequested) PrepareGroundDirection(item,Back);')
edit('src/FirstPerson.cpp', 'DirectionalJumpAxesScope jumpAxes(item);', 'DirectionalJumpAxesScope jumpAxes(item,backHopRequested);')
edit('README.md', '# ', '# ') if False else None
readme = root/'README.md'
updates[readme] = readme.read_text(encoding='utf-8-sig') + '\n### First-person backward hop\n\nRight grip + LS down requests Lara\'s native backward hop while standing in TR1–3.\nLS down alone keeps smooth backward walking. Vent crawling, Jump and interaction\ninputs retain their existing behavior; native floor and wall collision remain in control.\n'
for p, text in updates.items():
    if '--apply' in sys.argv:
        p.write_text(text, encoding='utf-8', newline='\r\n')
    print(('Updated ' if '--apply' in sys.argv else 'Validated ') + str(p))
