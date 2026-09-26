## Tomb Raider IV-VI Remastered VR Mod
VR mod for Tomb Raider IV-VI Remastered. Tomb Raider IV: The Last Revelation, Tomb Raider V: Chronicles, and Tomb Raider VI: Angel of Darkness work in native stereo with 6DOF. TR6 uses a separate full-scene replay path for its offscreen renderer; AER remains the automatic fallback. TR6 also has its own correctness-first geometry, effects and small-object visibility paths, documented in [Phase 17](#phase-17-tr6-culling), [Phase 19](#phase-19-tr6-effects-visibility) and [Phase 20](#phase-20-tr6-pickup-and-scene-object-retention).

## AI Usage
Claude Code was used heavily in the development of this mod.  AI was used to reverse engineer the game with Ghidra, explore strategies for porting the game to VR, and write code, and iterate on failures.  I used the AI to probe the game logic so I could debug the game in real-time and make architectural decisions when Claude was otherwise determined to make incorrect decisions.   

OpenAI Codex was used for Phases 16 through 19 to inspect the newly supplied `tomb6.pdb`
and matching DLL, identify and verify the complete TR6 scene-render boundary,
implement its guarded per-eye replay, trace TR6's separate room-culling pipeline,
isolate projected-shadow cameras and trace its room-attached effects pipeline,
build and deploy the mod, and document the results after in-headset validation.

Codex also worked on the TR1–3 first-person port to TR4/5 and its subsequent
regression fixes and diagnostics. That work is documented in
[Phase 23](#phase-23-tr45-first-person); its automated tests are not a substitute
for in-headset validation, and the remaining motion issues are listed there.

## VR Mod Features
* Native stereo with 6DOF (TR4/5/6)
* Culling fixes for VR
* Camera fixes for tight collision areas
* UI fixes
* FMV fixes
* Gamepad and VR controller support
* Dpad input support
* Decoupled pitch
* Sky fix — the HD sky dome sits at optical infinity instead of a few metres away
* Chest physics for Lara in TR4/5, ported from TR6's dynamic bones
* First person for TR4/5, ported from TR1–3: directional movement, headset-driven
  arm aiming, body-follow rotation, collision-checked room-scale movement and
  eye-room portal culling. Rendered-eye wall clearance is deployed but still needs
  headset testing. Visible vertical gun tilt and motion polish remain under
  investigation; TR6 first person is not implemented.

## Installation
## Tomb Raider IV-VI Remastered VR — Installation

**1. Install the VR mod**
Download `TombRaider456VR.zip` and extract its contents into your game folder:
```
C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered
```

**2. Install New Effects TR4-5 *(optional, highly recommended)***
This significantly improves visuals in VR. Download it from NexusMods and extract into the same game folder:
🔗 [New Effects TR4-5 — NexusMods](https://www.nexusmods.com/tombraidertrilogy2remastered/mods/141?tab=description)

**3. Launch the game**
Start Tomb Raider IV-VI Remastered through Steam as normal.

## Controls

| Action | Control |
|---|---|
| Move | Left Stick (LS) |
| Walk | RB + LS |
| Sneak | RB + Y |
| Dash | L3 |
| Look | Right Stick (RS) |
| Zoom | R3 |
| Jump | A |
| Action | Y or both grips (LB+RB) during gameplay |
| Equip Weapon | LT (Hold) |
| Shoot | RT |
| Roll | B |
| Duck | LB |
| Photo Mode | L3 + R3 |
| Photo Mode Select | R3 (Dpad) + LS |
| Ledge Grab | RT |
| Ledge Drop | B |
| Side Backflip | Equip Weapon (LT) + Jump (A) + Move (LS) |
| 180 Frontflip/Backflip | Equip Weapon (LT) + Jump (A) + Move (LS) + Roll (B) |
| Swan Dive | Jump (A) + Move (LS) + Roll (B) |
| Toggle First Person (TR4/5, `FirstPerson=1`) | Y + LT |
| Toggle Classic Graphics (TR4/5, `FirstPerson=1`) | Y + RT |
| Recenter First-Person Position | End; Numpad 5 also works by default |
| Adjust Camera Pitch | RT + RB + RS |
| Menu | X |
| Sneak (TR6 only) | RB + Y |

With `FirstPerson=1` (the new default), Y + LT selects first/third person in
gameplay, inventory and title menus; it never doubles as the graphics toggle.
Release and press the chord again to switch back. Selecting first person in a
menu takes effect when normal gameplay resumes. Startup is still third person:
the INI setting enables the feature, not an automatic first-person startup.
TR6, or `FirstPerson=0`, retains the legacy Y + LT hold chord for graphics.

In first person, LS up moves forward, LS left/right sidesteps, and LS down
backpedals instead of turning Lara to run toward the camera. The dominant stick
axis selects the directional gait. RS turns your view; physical head rotation
also changes your view, and Lara follows during supported ground movement.
R3's D-pad shift remains available. See [Phase 23](#phase-23-tr45-first-person)
for settings, gameplay exceptions and the current motion limitations.

## Development Notes

**A VR mod for Tomb Raider IV–VI Remastered** (`tomb456.exe`, v1.0.2a),
driving an OpenVR runtime. It supports the retail Steam release, the HD
Definitive Patch and the 2026-01-17 build every address here was read out of;
see [Phase 22](#phase-22-retail-and-definitive-edition-builds).

The mod loads into the game, reads the head pose from an OpenVR runtime, and
composes it onto the game camera. Its development phases share one binary and
one set of hooks; settings select optional paths at runtime.

| | | State |
|---|---|---|
| **Phase 1** | **Mono head tracking** — one image to the monitor, engine projection, nothing submitted to the compositor | The bring-up test. `Mode=mono` |
| **Phase 2** | **Native stereo with 6DOF** — per-eye matrices, double-wide target, positional tracking, compositor submit | Working. `Mode=stereo` |
| **Phase 3** | **UI fixes** — the flat 2D layer placed on a world-locked panel, and video cutscenes made fusable | Working. On by default in stereo |
| **Phase 4** | **FMV fixes** — cutscenes captured offscreen and replayed as real world geometry | Working. On by default in stereo |
| **Phase 5** | **VR controller support** — Touch controllers presented to the game as an Xbox pad | Working. On by default |
| **Phase 6** | **TR6 support** — alternate-eye fallback for Angel of Darkness's offscreen renderer | Working. Used when native stereo is disabled or unavailable |
| **Phase 7** | **Culling fix** — the missing geometry behind Lara, fixed inside the game DLLs | Working. TR4 / TR5 |
| **Phase 8** | **D-pad input** — hold R3 and the left stick becomes a D-pad | Working. On by default |
| **Phase 9** | **Inventory fix** — items no longer stack, by preserving the engine's own projection shear | Working. On by default |
| **Phase 10** | **Decoupled pitch** — the headset owns pitch; the right stick turns only, except when Lara needs vertical swim control | Working. On by default. TR6 swimming confirmed in-headset |
| **Phase 11** | **Ceiling clamp** — caps the tracked head so it cannot rise through low ceilings | Working. On by default |
| **Phase 12** | **Sky fix** — the HD sky dome drawn at optical infinity instead of its mesh radius | Working. TR4 / TR5 |
| **Phase 13** | **Laser sight fix** — the dot and the bullet made to agree, by putting the head centre back on the aim line | Working. On by default |
| **Phase 14** | **Hide vignettes** — the binocular, scope and infra-red overlays stubbed out, keeping the aiming dot | Working. On by default |
| **Phase 15** | **The stick stops displacing you** — the head's offset integrated in world space, so only the headset moves your eye | Working. On by default |
| **Phase 16** | **TR6 Native Stereo Support** — the complete Angel of Darkness scene and postprocess chain rendered once per eye | Working. Confirmed in-headset |
| **Phase 17** | **TR6 Culling** — rooms, props and distant geometry retained independently of the third-person camera | Working. Confirmed in-headset. TR6 only |
| **Phase 18** | **TR6 projected-shadow fix** — character shadow maps kept on their light camera instead of inheriting headset-eye transforms | Working. Confirmed in-headset. TR6 only |
| **Phase 19** | **TR6 effects visibility** — dust particles and street-lamp lighting retained independently of the orbiting right-stick camera | Working. Confirmed in-headset. TR6 only |
| **Phase 20** | **TR6 pickup and scene-object retention** — candy bars, pickups, barrels and props survive preparation and the final paired bounds tests | Built. Awaiting in-headset revalidation. TR6 only |
| **Phase 21** | **TR4/5 chest physics** — TR6's dynamic bones reimplemented: a damped spring driven by Lara's own airborne state, applied per vertex in the HD skinning shader | Working. On by default. TR4 / TR5 |
| **Phase 22** | **Retail and Definitive Edition builds** — every game-DLL address set made per build, so the retail Steam release and the HD Definitive Patch get the DLL-side fixes too | Working. Confirmed in-headset on both ported builds |
| **Phase 23** | **TR4/5 first person** — head anchor, directional locomotion, head aiming, room-scale movement, camera handoff and eye-room culling, ported from TR1–3 | Implemented and regression-tested; the reported disappearing-room case is fixed in headset. Visible vertical gun tilt, sideways forward-motion wobble and subtle stick-turn judder remain open. No TR6 first person |

**Phase 1** is not a lesser version of Phase 2; it is the instrument that makes
Phase 2 debuggable. One image, the engine's own field of view, no compositor —
which isolates the view maths (tracking, handedness, scale) from everything
else. Those are the things hardest to get right and easiest to misdiagnose once
stereo rendering is in the way, and every difficult bug in Phase 2 was diagnosed
by first proving the same thing in Phase 1.

**Phase 2** is where it becomes VR: both eyes rendered natively by the engine,
positional head tracking, and live scale tuning in the headset. The mechanism
— including three plausible implementations that fail in instructive ways — is
in [Phase 2: native stereo with 6DOF](#phase-2-native-stereo-with-6dof).

**Phase 3** fixes the flat layer stereo leaves behind — HUD, menus, subtitles
and pre-rendered video, which double-vision until they are given the frustum
shear the 3D layer gets for free. See
[Phase 3: UI fixes for VR](#phase-3-ui-fixes-for-vr).

**Phase 4** finishes the cutscenes. Phase 3 made them fuse by shifting the
viewport, which is a fake transform; Phase 4 captures each video frame offscreen
and replays it as a real quad, so it keystones and rolls like anything else in
the room. See [Phase 4: FMV fixes](#phase-4-fmv-fixes).

**Phase 5** adds the controllers. The game resolves XInput through a function
pointer, so the Touch controllers can be presented to it as pad 0 with no driver
and no code patching — and the engine adopts the Xbox control scheme and prompts
by itself. See
[Phase 5: VR controller support](#phase-5-vr-controller-support).

**Phase 6** adds the original TR6 fallback. Angel of Darkness renders its scene
into offscreen textures and composites at the end, so per-draw duplication
cannot reach it. Alternate-eye rendering made that pipeline stereo-correct at
half the update rate per eye. See [Phase 6: TR6 support](#phase-6-tr6-support).

**Phase 7** fixes the void behind Lara. The engine only draws rooms its portal
traversal reaches from the game camera, so looking away from it finds nothing —
and the traversal is in the game DLLs, not the exe. See
[Phase 7: culling fix](#phase-7-culling-fix).

**Phase 8** gives the controllers a D-pad they do not physically have, as a
shift layer on R3. See
[Phase 8: D-pad input support](#phase-8-d-pad-input-support).

**Phase 9** unstacks the inventory. The engine lays that screen out through the
projection's shear terms, which Phase 2 was overwriting wholesale. See
[Phase 9: inventory fix](#phase-9-inventory-fix).

**Phase 10** is comfort: the headset already owns pitch, so the right stick is
reduced to yaw and the conflicting second source of vertical rotation goes away.
See [Phase 10: decoupled pitch](#phase-10-decoupled-pitch).

**Phase 11** stops the tracked head rising through low ceilings in tunnels and
crawlspaces, by capping its height rather than moving the camera. See
[Phase 11: ceiling clamp](#phase-11-ceiling-clamp).

**Phase 12** fixes the sky. `DrawSkyHD` in the game DLL already draws the dome
through a matrix it has zeroed the translation of, which is monitor-correct;
the stereo path then adds the per-eye translation back on top, and a dome of
finite mesh radius gets finite stereo depth. See
[Phase 12: sky fix](#phase-12-sky-fix).

**Phase 16** replaces TR6's AER path during normal play. The newly supplied
symbols identify a render-only function around Angel of Darkness's complete
offscreen scene pipeline, so that function can run once for each eye while the
game loop still advances once. See
[Phase 16: TR6 Native Stereo Support](#phase-16-tr6-native-stereo-support).

**Phase 17** addresses TR6's separate culling pipeline. Angel of Darkness was
still discarding rooms and individual room runs according to its third-person
right-stick camera, even though the headset rendered in another direction. The
final path submits every active room and keeps its render runs available to the
VR view. See [Phase 17: TR6 Culling](#phase-17-tr6-culling).

**Phase 18** fixes TR6's projected character shadows. Their depth atlas uses a
light camera, so allowing the generic offscreen stereo injector to treat it as
a player-camera pass distorted Lara's shadow differently for each eye. The
light-camera pass is now isolated while final shadow projection remains stereo.
See [Phase 18: TR6 Projected-Shadow Fix](#phase-18-tr6-projected-shadow-fix).

**Phase 19** fixes TR6's camera-dependent particle and local-light popping.
Room-attached effects had their own primary off-camera flag, emitter-distance
test and local-light bounds test after Phase 17's room geometry was already
accepted. Native stereo now keeps those effect decisions independent of the
orbiting right-stick camera. See
[Phase 19: TR6 Effects Visibility](#phase-19-tr6-effects-visibility).

**Phase 20** fixes small TR6 objects that could still vanish after their room
was retained. The first Parisian Backstreets candy bar exposed two causes:
over-broad Phase 17 room preparation disturbed the engine's object state, and
the final renderer could reject a prepared object at an AABB test before its
later OBB test. The clean fix leaves object preparation stock, forces only the
final room descriptors, and bypasses both bounds stages at five exact
render-only paths. See
[Phase 20: TR6 Pickup and Scene-Object Retention](#phase-20-tr6-pickup-and-scene-object-retention).

**Phase 21** adds chest physics to TR4 and TR5, reimplementing the dynamic
bones TR6 uses. A damped spring driven by Lara's own airborne state bounces her
chest on jumps and landings, and a patch to the HD skinning shader applies it
per vertex, so the back, backpack and shoulders stay still. See
[Phase 21: TR4/5 Physics Update](#phase-21-tr45-physics-update).

**Phase 22** makes the mod work on the builds people actually have. Everything
before it was reverse engineered against the one build that ships private PDBs,
and on the retail Steam release or the HD Definitive Patch every hook that lives
in a game DLL recognised no timestamp and switched itself off. The address
tables are now per build, ported structurally from the symbolised binaries and
checked against the shipped ones. See
[Phase 22: Retail and Definitive Edition Builds](#phase-22-retail-and-definitive-edition-builds).

**Phase 23** ports first person from TR1–3 to TR4/5, including its movement,
head/body alignment, aiming and room-scale behavior. It also records the TR5
vehicle-field regression, the first-to-third-person translation fix, the
headset-confirmed room-culling fix and remaining aiming and motion gaps. See
[Phase 23: TR4/5 First Person](#phase-23-tr45-first-person).

**The repository's `TombRaiderVR.ini` is the configuration template.** The DLL
embeds it through `src/DefaultIni.h`, and the mod writes
`TombRaiderVR.ini` beside itself on first launch — ready to play, with
`Mode=stereo`, `EyeOffsetMode=3`, and every option from every phase documented
in place. The write is `CREATE_NEW`, so an existing file is never overwritten
and tuned settings are safe. To reset to defaults, back up or rename the installed
INI and relaunch. Updating the DLL does not add keys to an existing INI; merge
new settings into its `[VR]` section to retain your tuning.

The renderer was reverse-engineered from the shipped binary and its PDB. Every
address in `src/Engine.h` was read out of Ghidra and re-verified against the
decompiler before being written down. `docs/engine-map.html` is the full map of
how the renderer works and why the hooks sit where they do; `trace.txt` is the
session that produced it, summarised in
[Where this came from: the Ghidra trace](#where-this-came-from-the-ghidra-trace).

---

## Phase 1: what mono mode actually does

`Mode=mono` in `TombRaiderVR.ini`:

- **No stereo target, no `FBO_default` redirection, no GL objects created at
  all.** Mono allocates nothing on the GPU; it only needs poses.
- **No draw duplication.** Each draw is issued once, as the game intended.
- **No compositor submission.** OpenVR is initialised as a **Background** app,
  so SteamVR only has to be running and tracking — the game never becomes the VR
  scene application, and the runtime never waits on a frame from it.
- **The engine keeps its own projection.** Field of view is untouched, so
  anything that looks wrong is the view maths and nothing else.

The head pose is folded in at the one place every uniform reaches the GPU:

```
finalView = eyeView * gameView      (both row-major 3x4 affines)
```

`eyeView` is the centred head pose converted into engine space — OpenVR's Y-up
metres become TR's Y-down world units — with no per-eye offset, because there is
only one image and shifting it by half an IPD would just move the whole picture.

Mono also injects on **every** world-space pass rather than only
backbuffer-targeted ones. That is deliberate: the engine's frame graph is not
fully traced, and if the scene renders offscreen before compositing, a
backbuffer-only gate would produce no head-tracking at all and tell you nothing
about why.

`PositionalTracking=0` is the shipped default and the safe first test: the
translation column of the head pose is zeroed, so the camera can pivot but can
never be displaced. A wrong `WorldUnitsPerMetre` cannot put you inside a wall.
Turn positional on once looking around behaves.

`SeatedOrigin=1` matters more than it looks. Standing space is absolute room
coordinates: a head 1.6 m off the floor is ~680 TR units of camera offset at 423
units/metre, applied before the player has moved at all.

---

## Installing

Copy three files into the game folder, next to `tomb456.exe`:

| File | From |
|---|---|
| `winmm.dll` | `build\x64\Release\winmm.dll` (the proxy) |
| `TombRaiderVR.dll` | `build\x64\Release\` |
| `openvr_api.dll` | `SteamVR\bin\win64\openvr_api.dll` |

`TombRaiderVR.ini` is not copied — the mod writes it there itself on first
launch.

Then launch the game normally — from Steam, from a shortcut, however you like.
No injector. Put the headset on and load an actual level; a menu is not a test.

To uninstall, delete `winmm.dll`.

If OpenVR is unavailable or no headset is present, every detour early-outs and
the game runs exactly as it would without the mod. Two logs are written beside
the DLL: `TombRaiderVR-proxy.log` (did the shim load?) and `TombRaiderVR.log`
(everything else).

### Reading the log

```
config: mode=MONO head-tracking ...
vr: initialised as a Background app
vr: head pose ACQUIRED
present: MONO head-tracking active (no stereo target, no submit)
inject: MONO -- view only, engine projection untouched
vr: head @ x=+0.031 y=+1.612 z=-0.204 m  (fwd -0.02 -0.11 -0.99)
```

The `head @` line prints every 120 frames. **If those numbers do not change when
you move, the problem is tracking, not the injection** — which is the one
distinction that is otherwise painful to make.

| Symptom | Try |
|---|---|
| View does not move at all | Check for `head pose ACQUIRED`; check the `head @` numbers change |
| Looking up looks down | `FlipViewY` |
| Turning your head turns the wrong way | `FlipViewY` (it conjugates rotation, not just translation) |
| World feels giant / tiny (positional on) | `WorldUnitsPerMetre` |

`WorldUnitsPerMetre` defaults to **423** — Lara is ~762 TR units tall and reads
as roughly 1.8 m. Raise it to shrink the world.

Almost every "it looks wrong" is one of two sign conventions, which is why they
are ini toggles rather than a recompile. The engine renders **Y-flipped**
(`ogl_setPerspAngles` writes `e11 = -1/tanY`, and `ogl_setScissor` flips Y
against `gTargetHeight`) and TR world space is **Y-down**, while OpenVR is Y-up
in metres. That is two independent flips.

### OpenVR comes up on the first frame, not at load

`DllMain` starts a thread that installs the hooks and returns; OpenVR
initialisation happens inside `ogl_present` and retries every 120 frames, up to
ten times. So SteamVR can be started *after* the game, and a runtime that is
still coming up gets another chance instead of losing the session.

If a **Background** init fails with `VRInitError_Init_NoServerForBackgroundApp`,
the mod retries as an **Overlay** app. Background apps deliberately will not
start SteamVR — they attach to a server that is already running — whereas an
Overlay app will bring the runtime up and still does not own the scene.

---

## What it hooks

Six inline hooks, all installed together — all-or-nothing. In Phase 1 three are
active and the rest sit inert.

| Function | RVA | Role |
|---|---|---|
| `vid_setPass` | `0x0000C4C0` | **Phase 1.** Classifies each pass as world-space or 2D |
| `validate_draw` | `0x00011CC0` | **Phase 1.** Composes the head pose into the view matrix |
| `ogl_present` | `0x00012600` | **Phase 1.** Frame boundary; samples the next head pose |
| `ogl_draw` | `0x00012BD0` | Phase 2 — per-eye duplication |
| `ogl_drawVB` | `0x00012CF0` | Phase 2 — the other draw path |
| `fmvShow` | `0x00011350` | Phase 6 — an exact "a video is on screen now" signal, which TR6 needs to tell its cutscenes from its scene composite |

More hooks land **outside the exe**, in `tomb4.dll` / `tomb5.dll`. They are
installed lazily, because the game DLLs load after the mod does. `PrintRoomsList`
and `S_GetObjectBounds` are what [Phase 7](#phase-7-culling-fix) uses to extend
the visible set; `DrawSkyHD` is what [Phase 12](#phase-12-sky-fix) uses to mark
every sky draw so stereo can put the dome at optical infinity. `DrawSkyHD` is
independent of `PortalCulling` — it installs whenever `SkyAtInfinity=1`.

`validate_draw` is the single choke point where every uniform reaches the GPU,
which makes it the right place to substitute matrices. The substitution is
snapshot → write → let the original upload → restore, so the engine never
observes it and the next pass composes from pristine state rather than
compounding. The upload is forced by hand (`vid_state.consts |= kView`), because
the engine's own dirty bit is set on a *pointer* comparison in `vid_setPass` —
writing new contents into the same `mView_packed` would not trip it.

`vid_setPass` is the only thing in the engine that distinguishes "3D scene" from
"HUD". It decides per pass whether `vid_state.proj` points at `mProj[0]` (the 2D
ortho layer: logos, menus, subtitles, fades) or `mProj[1]` (world space). Reading
that pointer *after* the original runs is the only correct way to classify a
pass — several cases never assign `proj` at all and inherit the previous pass's
value, so a static table of shader ids would get the sticky ones wrong.

### Frame-graph tracer

`TraceFrames` plus `TraceKey` (F10 by default) log every render-target
transition and the world-space vs 2D draw counts against each. No extra hook is
needed — `validate_draw` already runs before every draw. Set `TraceFrames=2`,
get into **actual gameplay** (a menu frame has a handful of draws, gameplay has
hundreds, and the log says so if it looks like a menu), then press the key.

This is what answers the question Phase 2 depends on: where is the 3D scene
actually drawn? It is a Phase 1 tool for Phase 2's benefit.

---

## How the loader hook works

`tomb456.exe` statically imports `WINMM.dll` and uses exactly ten functions from
it (`timeGetTime`, `timeBeginPeriod`, `timeEndPeriod`, `timeGetDevCaps`, and six
`waveOut*`). A `winmm.dll` in the application directory is found before
System32's, so the loader maps ours and the game's own import table pulls us in
before it executes an instruction.

The proxy forwards all ten to the real winmm and side-loads `TombRaiderVR.dll`.

**The proxy exports all 180 winmm functions, not just those ten.** This is not
thoroughness for its own sake — a partial proxy is a real bug. The proxy takes
over the base name `winmm.dll` for the *whole process*, and the loader binds
every later module's winmm imports against it. SteamVR's `vrclient_x64.dll`
imports `timeSetEvent` and `timeKillEvent`; with those missing the import bind
failed, `LoadLibrary(vrclient_x64.dll)` was rejected, and OpenVR reported
`VRInitError_Init_VRClientDLLNotFound (102)` — which looks exactly like a broken
SteamVR install and sent the first round of debugging in the wrong direction.

The ten the game uses are real thunks (so the proxy's own init runs off them);
the other 170 are PE forwarders generated by `tools\gen_winmm_forwards.ps1`.
`tools\proxyvrtest.cpp` is the regression test: it loads the proxy first, exactly
as the game's import table does, then asks OpenVR to start.

**Why the thunks do not use PE export forwarders.** The tidy-looking version is
`#pragma comment(linker, "/export:timeGetTime=winmm.timeGetTime")`, but that is
circular: the loader resolves the `winmm` in the forwarder string by the normal
search order, finds *this* DLL first, and forwards to itself. The usual fix bakes
an absolute path into the forwarder string, which hardcodes `C:\Windows`.
Instead the proxy loads the real winmm by explicit path from
`GetSystemDirectory()` and thunks through function pointers — no hardcoded path,
no circularity, and it can be stepped through in a debugger.

The residual worry was that the loader also keys modules by base name, so a DLL
called `winmm.dll` asking for System32's `winmm.dll` might get handed back its
own handle and recurse to a stack overflow. `tests\proxytest.cpp` checks exactly
that: it confirms `timeGetTime` agrees with `GetTickCount` to within a few ms
(so the real winmm really was reached) and that **two** distinct `winmm.dll`
modules end up mapped. Loading by absolute path does not collide.

```
tests\build_proxytest.cmd
```

One ordering note: the proxy loads the mod lazily on the first forwarded call,
deliberately, because `LoadLibrary` under the loader lock is a documented
deadlock risk. In practice the game calls `timeBeginPeriod` during early startup,
long before the render loop, so hooks are installed well before anything draws.

---

## Building

Requires Visual Studio 2019 or newer with the C++ desktop workload. The project
uses `$(DefaultPlatformToolset)`, so it builds on whatever toolset is installed.

```
powershell -ExecutionPolicy Bypass -File tools\fetch_openvr.ps1
msbuild TombRaiderVR.sln /p:Configuration=Release /p:Platform=x64
```

Output: `build\x64\Release\TombRaiderVR.dll` and `build\x64\Release\winmm.dll`.

Only the OpenVR **headers** are needed to build. `openvr_api.dll` is resolved
with `LoadLibrary` at runtime, so the DLL has no static import on the runtime and
loads cleanly when no headset is attached. Verified imports: `OPENGL32.dll` and
`KERNEL32.dll`, nothing else — the CRT is linked statically so there is no
redistributable dependency inside a process we do not control.

### Self-test

```
tests\build_selftest.cmd
```

Checks the inline hook end to end (install, detour, trampoline, prologue-mismatch
rejection, clean removal) and the matrix maths. The load-bearing assertion is
that a **symmetric** OpenVR frustum reproduces `ogl_setPerspAngles` byte for
byte, so the only difference between our projection and the engine's own is the
asymmetry VR asks for.

### Diagnosing OpenVR: vrprobe

`tools\build_vrprobe.cmd` builds `vrprobe.exe`. Drop it in the game folder and
run it. It asks OpenVR exactly what the mod asks, in a plain x64 process, and
prints every answer — so it separates "OpenVR is unhealthy here" from "the mod
is broken" without launching the game.

```
cheap checks (these are advisory, not verdicts):
  VR_IsRuntimeInstalled              yes
  VR_IsHmdPresent                    yes

Background app (what mono mode asks for first):
  VR_InitInternal(Background)        OK
    IVRSystem                        OK (IVRSystem_026)

  sampling head pose for 3 seconds -- move the headset:
    valid  pos +0.512 -0.729 -0.361   fwd -0.80 +0.02 -0.60
```

**Move the headset while it samples.** A valid but frozen pose means the headset
is stationary or asleep, which looks exactly like "head-tracking is broken" from
inside the game.

The two cheap checks are labelled advisory on purpose. `VR_IsHmdPresent` is a
lightweight config probe that returns false in situations where init would in
fact succeed — a headset in standby being the usual one. An earlier version of
this mod treated it as a hard gate and refused to start on it, which threw away
the real `EVRInitError`. It no longer gates on either check; it attempts
initialisation and reports the runtime's own error symbol and description.

---

## Phase 2: native stereo with 6DOF

`Mode=stereo` turns on the rest of the code: per-eye matrices, real positional
tracking, and compositor submission. Both eyes are rendered natively by the
engine — no reprojection, no depth-buffer trickery, no shader edits.

Getting depth to actually appear took four attempts at one question — *where
does the per-eye transform go?* — and the three that failed are documented here
because each one fails in a way that looks like something else.

### Why the obvious answer is wrong

The natural place for a per-eye offset is the view matrix. It does not work,
and the reason is specific: of the engine's **125 vertex shaders, 38 transform
position as `dot(uViewMatrix[i].xyz, p.xyz)`** — rotation only. They never read
the translation column at all; for those passes the camera offset is baked into
`uModelMatrix` on the CPU beforehand.

So a per-eye translation written into `uViewMatrix` is simply invisible to 38
of the 125 shader paths. The symptoms are misleading:

- no parallax, so everything sits at infinity — which reads as **oversized**,
  not as flat
- `IpdScale` and `WorldUnitsPerMetre` appear inert
- a per-eye **rotation** was always visible, because rotation-only shaders do
  honour the rotation — which is why a 20° yaw test passed while every
  translation test failed

That last point is what kept the bug alive: the view matrix demonstrably drove
rendering, so the injection point looked correct.

All three earlier placements fail, each differently:

| Mode | Where the translation goes | Why it fails |
|---|---|---|
| `EyeOffsetMode=0` | `uViewMatrix` translation column | Invisible to the 38 rotation-only shaders (above) |
| `EyeOffsetMode=1` | `uModelMatrix` translation column | Skinned geometry is transformed by `uJoints[72*3]`, not `uModelMatrix`, so characters do not move with the world and float outside the map |
| `EyeOffsetMode=2` | `uProjMatrix`, post-multiplied by `translate(d)` | Correct depth and working scale keys, but the world **swims when you rotate your head** |

Mode 2's swim is worth understanding, because it is the trap one level below the
first. The view-space delta it applies is `d = (R_e − I)·t_g + t_e`. The game's
own view translation `t_g` is on the order of **82,000 world units**, so any head
rotation at all makes the first term thousands of units and the world slides
away underneath you. Chasing individual transform paths is a losing game.

### The fix: fold the whole eye transform into the projection

`EyeOffsetMode=3` leaves the view matrix **completely untouched** and composes
the per-eye transform into the projection instead:

```
clip = P · (R_e·v + t_e) = (P · E) · v        E = [ R_e | t_e ]
```

One multiply, rotation and translation together, applied to the one matrix
nothing in the engine can bypass — every one of the 125 shaders ends with
`uProjMatrix * vec4(viewpos, 1.0)`, skinned geometry included.

It is correct for **both** shader families by construction, and the proof is why
leaving the view matrix alone matters so much. With the game's own view matrix
in place:

- the full-dot shaders compute `R_g·w + t_g`
- the rotation-only shaders compute `R_g·p`, where the CPU already gave them
  `p = w + R_gᵀ·t_g`

which is the same `R_g·w + t_g`. **The two families agree exactly — and only
diverge once the view matrix is modified.** So don't modify it.

Mode 2's blow-up cannot happen here either: `E` is a metres-scale transform, so
there is no `t_g`-sized term to amplify.

In `validate_draw` this is a column-major `P · E` with `E`'s bottom row taken as
`(0,0,0,1)`, which is why column 3 picks up `P`'s own column 3. The `kProj`
dirty bit is forced by hand; `kView` deliberately is **not**, because in mode 3
nothing writes to the view matrix at all. `P` is restored after the original
uploads, so the engine never observes the substitution.

### 6DOF

Positional tracking is now honoured in stereo, not just mono. It previously
applied only on the mono path, which meant `PositionalTracking=0` silently did
nothing once stereo was on — the head's translation went through regardless.
Both modes now zero it in the same place.

The full per-eye chain, in order:

1. **Head pose** from `WaitGetPoses`, inverted to tracking→head.
2. **Translation dropped** if `PositionalTracking=0` — a pure pivot about the
   game camera, so a wrong world scale cannot push you through the floor.
3. **Eye offset** — `eyeFromHead` from `GetEyeToHeadTransform`, with `IpdScale`
   applied to its translation only.
4. `eye ← head ← tracking` composed in OpenVR's own convention.
5. **Into engine space** — OpenVR's Y-up metres to TR's Y-down world units:
   Y-flip if `FlipViewY`, then × `WorldUnitsPerMetre`.

`SeatedOrigin=1` remains the default, and matters as much in stereo as in mono:
standing space is absolute room coordinates, so a head 1.6 m off the floor is
~680 TR units of offset before you have moved.

The eye-to-head offset is the entire source of stereo separation, so it is
logged at init and a zero one is called out explicitly — otherwise a headset
reporting no IPD looks exactly like a world-scale problem:

```
vr: eye 0 eyeToHead offset = (-0.0320, +0.0000, +0.0000) m
vr: WARNING eye 1 has a ZERO eye-to-head offset -- there will be no stereo
    separation regardless of WorldUnitsPerMetre
```

### Live tuning: scale and IPD are separate controls

Scale is a perceptual judgement, so it is adjustable **in the headset** rather
than through an edit–rebuild–relaunch cycle. Every change is logged in a form
you can paste straight back into the ini.

| Key | Effect |
|---|---|
| numpad `+` | Raise `WorldUnitsPerMetre` — **smaller** world, **stronger** depth |
| numpad `−` | Lower it — bigger world, weaker depth |
| numpad `*` / `/` | `IpdScale` up / down |
| numpad `0` | Reset both to the ini values |

The two do different things, and confusing them wastes time:

- **`WorldUnitsPerMetre`** is the physically honest control. Apparent size and
  stereo depth are the same quantity — your IPD is fixed, so a world rendered
  too large leaves the eye separation proportionally too small, which reads as
  "enormous" *and* "no depth" simultaneously. One number fixes both.
- **`IpdScale`** is the cheat: it stretches eye separation without changing
  world size. Reach for it only when the scale feels right but depth still
  reads flat. `1.0` is physically honest.

Steps are multiplicative (`ScaleStep=1.25`, so three presses roughly double).
5% steps proved useless — scale differences only become obvious around 2×.
Values are clamped to 16–8192 units/m and 0.1–10× IPD. The log line reports the
resulting eye separation in world units for a 64 mm IPD, which is the number
worth sanity-checking:

```
tuning [world scale]: WorldUnitsPerMetre=528.8  IpdScale=1.000
                      (eye separation ~33.8 world units for a 64 mm IPD)
```

### Per-eye projection: separating shear from parallax

Two independent things make the eyes differ, and only one of them is depth:

- **parallax** — the ±13.4-unit view offset. This *is* depth.
- **shear** — `proj[8] = ∓0.2425`, the asymmetric frustum. A constant sideways
  shift carrying **no depth at all**.

Confirming that the two halves *differ* proves nothing, because the shear alone
is enough to cause that. `PerEyeProjection` isolates them:

| Value | Frustum | Use |
|---|---|---|
| `0` | The engine's own projection | **Not a clean control** — it also drops the HMD field of view and feeds a 1.78-aspect frustum into a 0.93-aspect viewport, changing two things at once |
| `1` | The HMD's true asymmetric frustum | Correct, and the default |
| `2` | HMD field of view, symmetrised — same total extent per axis, shear forced to zero | The clean single-variable test: FOV and aspect stay exactly as in mode 1 |

Mode 2 exists because of a specific failure. The shear contributes a **constant
disparity** — the pedestal — of `(r+l)/(r−l)`, which for this HMD is 0.485 NDC,
about **326 px**. The headset optics are meant to cancel it exactly. The actual
depth cue riding on top is only ~32 px at 2000 units. If that cancellation is
not happening, the eyes converge on the pedestal and the scene collapses to one
plane: flat, oversized, and immune to `IpdScale`.

Symmetrising averages the tangent magnitudes, which preserves `(r−l)` and
`(b−t)` exactly — so scale and aspect are untouched and the shear is the only
variable that moved.

`PerEyeView=0` is the mirror-image test: keep the shear, drop the parallax. If
the halves still differ with it off, the difference was never parallax.

### Live pass classification

The world/2D test now reads `vid_state.proj` **at draw time** rather than
trusting the flag cached by the `vid_setPass` hook. `vid_setPass` configures a
pass once and many draws follow, and anything that repoints `vid_state.proj` in
between — `vid_setOrtho3D` does exactly that — leaves the cached flag stale. The
pointer in `vid_state` at the moment of the draw is the actual truth about which
matrix is about to be uploaded.

Disagreements between the live and cached classification are counted and
reported. A histogram of every distinct `proj` pointer seen at draw time is
logged too, because the test assumes `proj` is only ever `&mProj[0]` or
`&mProj[1]` — if the engine points it somewhere else during gameplay, every
draw silently classifies as 2D and no per-eye matrices are applied anywhere:

```
proj pointers: mProj[0](ortho)=00000001..  mProj[1](persp)=00000001..
  proj=00000001..  draws=12043    perspective/world
  proj=00000001..  draws=1881     ortho/2D
```

An `UNRECOGNISED` row there is the explanation for a scene with no depth.

### Instrumentation: proving where stereo breaks

Most of Phase 2's new code is measurement, because "the headset shows mono" has
about six possible causes and they are indistinguishable from the outside. The
checks form a ladder from our own memory out to the pixels, and each one
isolates the stage below it.

**1. Did our write land?** The injection logs the packed view translation read
straight back out of engine memory (floats 3, 7, 11 of `mView_packed`), plus
the `uViewMatrix` uniform location from `shaders[].uid[1]`.

**2. Is the separation non-zero?** Each eye is logged exactly once, then the
distance between them is reported. Identical translations mean both eyes render
the same view and no scale setting can help:

```
inject: SEPARATION between eyes = 26.83 world units (dx=26.83 dy=0.00 dz=0.00)
```

**3. Did the GPU receive it?** An in-memory write is not an upload —
`validate_draw` only uploads when `consts` has the dirty bit set *and*
`shaders[].uid[1] >= 0`. `glGetUniformfv` asks the GPU what it actually holds,
per eye. This is the load-bearing check: if the two match, the matrices never
reached the shader; if they differ, the fault is downstream in viewport, target
or submit. It is re-armed every report window rather than latched once, so it
tracks live tuning changes instead of only ever sampling frame 1.
`glGetUniformfv` forces a pipeline sync, so it runs twice per 1800 frames and no
more. A driver that does not export it loses the diagnostic, not stereo.

**4. Are the two halves actually different pixels?** `CompareHalves` samples a
5×5 interior grid from each half of the eye target with `glReadPixels` and
counts how many differ by more than a small threshold. It runs before `Submit`
and the clear, on the frame just rendered. Correct stereo at any sane IPD moves
most of those samples; identical halves move none.

**5. Do the halves map to the right eyes?** `EyeMarkers=1` burns a red bar into
the left half and a blue bar into the right, in the same place in each:

| What you see | What it means |
|---|---|
| Red in left eye, blue in right | Halves map to eyes correctly |
| Both bars in both eyes | Submit bounds are being ignored |
| The same colour in both eyes | Both eyes are getting the same half |
| No bars at all | What you are looking at is not this texture |

**6. Does the view matrix drive rendering at all?** `DebugEyeYawDegrees=20`
swings the right eye's view by an angle that cannot be subtle or misjudged. If
the halves stay identical under that, the injection point itself is wrong
however correct it reads.

**The health report** ties these together every 1800 frames, in deltas:

```
stereo health @frame 5400: world draws=14203  duplicated=7104
  injected eye0=7101 eye1=7101  (99% of world draws got per-eye matrices)
  offscreen-skipped=0  classify-mismatch=0
verify: uViewMatrix ON GPU  eye0=(...) eye1=(...)  separation=26.83 world units
halves: 23 of 25 sampled pixels differ between the left and right halves
```

Two deliberate choices in that report, both of which were bugs first:

- **Deltas, not totals.** A one-shot report at frame 300 only ever sampled the
  menus, where almost everything legitimately is 2D. It read "2 injections out
  of 725 draws" and looked like a serious bug when it was a title screen.
- **The denominator is total world draws, not duplications.** Dividing
  injections by duplications reads 100% by construction — both are gated on the
  same condition, so it could never detect the failure it was warning about.

### How the frame is rendered

Each draw is issued twice into the two halves of one double-wide render target,
with different matrices and a different viewport. The scene is not re-run. That
matters because the engine creates a **GL 3.2 Core** context and ships **202
shader pairs**: `GL_OVR_multiview2` would mean editing 404 GLSL sources and
would still need driver support on a 3.2 core context. Duplicating draws needs
zero shader changes.

`validate_draw` cannot issue a draw — its two callers do, immediately after it
returns — so per-eye duplication happens one level up in `ogl_draw` /
`ogl_drawVB`. The engine's `FBO_default` global is overwritten with our eye FBO,
so every "return to the backbuffer" inside `ogl_setRenderTarget` lands in the
stereo target without hooking that function at all. Both halves go to the
compositor as one texture split by UV bounds.

### Where this came from: the Ghidra trace

`trace.txt` is the captured reverse-engineering session that produced
`src/Engine.h`, run against the shipped `tomb456.exe` with its PDB. Five
functions were traced end to end — `validate_draw`, `vid_setViewMatrix` +
`ogl_setPerspAngles`, `ogl_setRenderTarget`, `init_ogl`, and `vid_setPass` —
and nearly every design decision above traces back to something in it.

**The headline: there is no camera object.** The database holds 1611 structs and
not one is a `Camera` or `Frustum`. Camera state is flat globals plus a
dirty-flag bitmask, centred on `RenderState` @ `vid_state` (240 bytes, 29
members) with `shader` at +16, `consts` at +96, and the four matrix pointers
`proj`/`view`/`shadow`/`model` at +104/+112/+120/+144.

**The trap that shaped everything: the view matrix is not uploaded via
`glUniformMatrix4fv`.** That symbol is a function pointer with only three xrefs
in the whole binary, and `validate_draw` holds the only two call sites. Inside
it:

```c
if ((consts & 1)       && uid[0] >= 0) glUniformMatrix4fv(uid[0], 1, 0, proj);
if ((consts & 2)       && uid[1] >= 0) glUniform4fv      (uid[1], 4,    view);   // ←
if ((consts & 4)       && uid[2] >= 0) glUniformMatrix4fv(uid[2], 6, 0, shadow);
if ((consts & 0x10000) && uid[5] >= 0) glUniform4fv      (uid[5], 4,    model);  // ←
```

View and model go up as 4×`vec4` through `glUniform4fv`; only projection and
shadow use `glUniformMatrix4fv`. Hooking `glUniformMatrix4fv` — the obvious
move, and the literal thing that was asked for — would have caught projection
and missed the view matrix entirely. `init_ogl`'s extracted GLSL confirms it is
by design rather than a decompiler artifact: `uniform vec4 uViewMatrix[4]` and
`uniform vec4 uModelMatrix[4]`, against `uniform mat4 uProjMatrix`.

That single fact is why per-eye stereo needs no shader changes at all: a plain
`vec4[4]` uniform can simply be overwritten per eye. The trace stopped there,
and its conclusion was optimistic — it extracted the uniform *interface* rather
than reading the 202 shader bodies, so the 38 rotation-only consumers of that
uniform went unnoticed until the depth bug forced a look at the GLSL itself.

#### What each trace established, and what it decided

| Finding | Where | What it decided in Phase 2 |
|---|---|---|
| `uViewMatrix` / `uModelMatrix` are `vec4[4]`; `uProjMatrix` is the only `mat4` every shader ends with | `init_ogl` GLSL | Draw duplication over `GL_OVR_multiview2`; and ultimately `EyeOffsetMode=3`, which targets the one matrix nothing bypasses |
| `uniform vec4 uJoints[72*3]` — skinned geometry has its own transform path | `init_ogl` GLSL | Why `EyeOffsetMode=1` (model matrix) leaves characters floating outside the map |
| `consts` dirty bits: `1`=proj, `2`=view, `4`=shadow, `0x10000`=model, `0x1000000`=joints; a shader switch forces `0xff07bf` | `validate_draw` | Which bits to force by hand after substituting a matrix |
| The proj dirty bit is a **pointer** comparison (`vid_state_prev.proj != vid_state.proj`) | `vid_setPass` | Why new contents in the same `mProj[1]` never trip it, so `kProj` is set manually |
| `vid_setPass`'s 202-case switch points `proj` at `mProj[0]` (ortho/2D) or `mProj[1]` (perspective/world) per pass | `vid_setPass` | The world-vs-HUD signal the whole injection gate is built on |
| Nine cases (`0x3F`, `0x40`, `0x48`, `0xC0`, `0xC1`, `0xC4`–`0xC7`) never assign `proj` and inherit the previous pass's pointer | `vid_setPass` | Why classification reads the live pointer instead of a static shader-id table |
| Every render target is a layered 2D array texture — `glFramebufferTextureLayer` + `GL_TEXTURE_2D_ARRAY`, `color_index` is the layer | `ogl_setRenderTarget` | Per-eye targets need no new plumbing; the engine already renders to layered targets natively |
| Exactly one `FBO_custom` exists, and every target change re-attaches colour + depth and runs a full `glCheckFramebufferStatus` | `ogl_setRenderTarget` | Why Phase 2 allocates its own FBO rather than fighting for attachments |
| `FBO_default` is latched from `GL_FRAMEBUFFER_BINDING` at init | `init_ogl` | The redirection trick: overwrite that global and every "return to backbuffer" lands in the eye target, with no hook on `ogl_setRenderTarget` |
| **Scissor test is globally enabled at init** | `init_ogl` | Per-eye viewport must set scissor too, or geometry clips across the eye boundary |
| `glCheckFramebufferStatus`'s return value is discarded — no branch, no log | `ogl_setRenderTarget` | A bad layer index fails silently, so a black screen is a plausible symptom of an unrelated mistake |
| Depth is `GL_DEPTH_ATTACHMENT` only, never depth-stencil | `ogl_setRenderTarget` | The eye target matches — no stencil |
| GL 3.2 Core, 202 `shader_init` calls, 404 GLSL sources, all `#version 150` | `init_ogl` | The multiview cost estimate: 404 files to edit, plus driver support on a core context |
| A shader id above 201 skips all configuration, and `validate_draw` then indexes `shaders[]` out of bounds | `vid_setPass` | The out-of-range warning in the `vid_setPass` detour |

#### The view and projection setters

`vid_setViewMatrix` takes a 3×4 **integer** matrix — TR's classic 16384 = 1.0
fixed point — scales rotation by 1/16384, passes translation through raw as
world units, and negates Z for the handedness flip into GL convention. It writes
two separate globals: `mView` (rotation only, a different consumer) and
`mView_packed`, which carries rotation plus translation at float indices 3, 7
and 11. That transposed, row-vector layout is exactly what a `vec4[4]` uniform
uploaded without a transpose flag wants — and indices 3/7/11 are the ones the
Phase 2 in-memory readback prints.

`ogl_setPerspAngles` builds a **symmetric** frustum by construction: it writes
`e00 = 1/tanX`, `e11 = -1/tanY`, and explicitly zeroes the shear terms `e02` and
`e12`. The self-test's load-bearing assertion — that a symmetric OpenVR frustum
reproduces this function byte for byte — comes straight from this trace.
`vid_setPerspOffset` does nothing *but* set those two shear terms, so the engine
already exposes an asymmetric-frustum lever as an API entry point.

Neither has a single call xref. Both are address-taken into the `app` struct @
`0x1405833E0` by `vidInit` / `init_ogl`, and the real call path is
**game DLL → `app.<fn>` vtable → setter → `consts` bit → `validate_draw`
uploads**.

#### The road not taken

The trace's own suggestion was to inject at that vtable — swap `app.setViewMatrix`
and `app.setPerspOffset` for per-eye control without touching the GL layer. It
is a clean idea and Phase 2 does not use it, for reasons only the later shader
analysis exposed:

- swapping `app.setViewMatrix` still routes the eye offset through
  `mView_packed`, i.e. `uViewMatrix` — the exact column the 38 rotation-only
  shaders never read. It would have hit the same wall from a tidier place.
- `app.setPerspOffset` controls the frustum **shear**, which is a constant
  sideways shift carrying no depth. It is the right lever for the asymmetric
  frustum and the wrong one for parallax.

Hooking `validate_draw` instead — the single choke point where every uniform
reaches the GPU, which the very first trace identified — is what made it
possible to substitute a matrix the engine has no API for at all.

#### Caveats recorded in the trace

- **`gTargetWidth` and `gTargetHeight` sit ~44 MB apart** (`0x143298680` vs
  `0x140698678`) even though the code writes them as a pair. The trace flagged
  this as likely a bad PDB symbol or a Ghidra artifact and said to confirm the
  real store address before hooking either; `src/Engine.h` carries the same
  warning next to the constant.
- **`ogl_rt` is a single latch with no stack.** Nested target changes do not
  restore correctly — the inner restore clobbers the outer's saved state.
  Relevant if eye passes are ever wrapped around existing effect passes.
- **`init_ogl` always returns 1.** There is no failure path: if
  `wglCreateContext` fails, every later GL call silently no-ops.
- The analysis ran against a **copy** of the Ghidra project (staged under
  `C:\dev\Ghidra\`), so annotations made during the trace do not appear in the
  original `.rep`.

### Phase 2 settings

| Setting | Default | What it does |
|---|---|---|
| `EyeOffsetMode` | `2` | Where the per-eye transform is applied. **`3` is the working one** — see below |
| `PerEyeProjection` | `1` | `0` engine frustum, `1` HMD asymmetric, `2` HMD FOV symmetrised |
| `PerEyeView` | `1` | Per-eye view offset. `0` keeps the shear and drops the parallax |
| `PositionalTracking` | `1` in the ini | 6DOF. `0` is rotation-only |
| `IpdScale` | `1.0` | Eye separation only, world size unchanged |
| `ScaleStep` | `1.25` | Multiplicative step for the tuning hotkeys (clamped 1.01–4.0) |
| `EyeMarkers` | `0` | Red/blue eye-mapping bars |
| `DebugEyeYawDegrees` | `0` | Yaw the right eye by N degrees as a visibility test |

> **Note on `EyeOffsetMode`.** The `Config.h` fallback is `2` — the superseded
> mode that swims when you turn your head — but the generated `TombRaiderVR.ini`
> sets `EyeOffsetMode=3`, which is the working one. The fallback only applies if
> the ini cannot be written and none exists.

Sign-convention toggles, unchanged from Phase 1:

| Symptom | Setting |
|---|---|
| Upside down in the headset, fine on the monitor | `FlipSubmitV` |
| Upside down in **both** headset and monitor | `FlipProjectionY` |
| World tilts the wrong way when you lean | `FlipViewY` |
| Depth inverted / eyes crossed | `SwapEyes` |
| World feels giant or tiny; IPD wrong | `WorldUnitsPerMetre` |
| Suspect the duplication, not the matrices | `DuplicateDraws=0` |

---

## Phase 3: UI fixes for VR

Phase 2 makes the world stereoscopic. It leaves the flat layer behind, and that
layer is most of what you look at outside combat: the inventory ring, the map,
subtitles, load screens, and every pre-rendered cutscene.

The symptom is **double vision** — two copies of the HUD that refuse to fuse —
and it has a specific cause. The 2D layer is drawn with the engine's ortho
projection, which is *identical in both eyes*. The headset optics apply a fixed
per-eye correction that assumes an asymmetric render, roughly 15° each way here.
3D content carries that shear in its own projection and comes out aligned; the
ortho layer never had it, so the optics pull the two copies apart and your eyes
cannot converge on them.

Phase 3 addresses that in four parts: supply the missing shear, put the layer
somewhere sensible in space, get its Y convention right, and handle the passes
that no matrix can reach.

### 1. Give the 2D layer the shear it never had

`VRSystem::HudNdcShiftX` computes the horizontal NDC shift the flat layer is
missing:

```
shift = −P02  +  s · P00 · halfSeparation / depth
        ↑                  ↑
        align with         converge to a
        the optics         comfortable distance
```

The first term is the pedestal from Phase 2 — `(r+l)/(r−l)`, the constant
disparity the optics expect and cancel. Supplying it is what makes the layer
fusable at all. The second term is genuine convergence, placing the panel at
`HudDepthMetres` (4 m by default) rather than at infinity. Half the physical eye
separation is read from the HMD's own `eyeToHead` transforms and honours live
`WorldUnitsPerMetre` and `IpdScale`, so the panel keeps its place while you tune.

### 2. World-locked panel instead of a head-locked one

Shifting NDC per eye fuses the image but welds it to your face — the HUD swings
with every head rotation, which is uncomfortable to read and unpleasant to look
around. So by default (`HudLockToHead=0`) the 2D layer is turned into a **quad
fixed in the game camera's frame**: it stays where it is in the world while your
head turns, and you look around it the way you would a physical panel.

The pass's ortho matrix is replaced with

```
Q = P_persp · E · L · P_o
```

| Term | What it is |
|---|---|
| `P_o` | The engine's own ortho matrix for that pass — whatever it set, left intact |
| `L` | Lifts ortho NDC `(x, y, *, 1)` onto a plane in camera space, `(W·x, H·y, −Z, 1)`. **The z input is deliberately discarded**, so every 2D element lands on the one plane instead of scattering by draw order |
| `E` | The per-eye head transform. Head rotation lands *here*, which is exactly why the panel stays put |
| `P_persp` | The real per-eye projection — the same asymmetric frustum the world gets |

`Z` comes from `HudDepthMetres`; the panel's half-width `W` is `Z·tan(HudSizeDegrees/2)`
and its height follows the screen aspect. `HudSizeDegrees` defaults to **55°**
because the engine's 2D layer fills a flat screen edge to edge — mapped onto the
headset's ~94° field of view unchanged, it would be overwhelming and unreadable
at the edges. Near and far are inherited from the live perspective matrix and
widened if the panel would fall outside them.

`HudLockToHead=1` restores the flat per-eye NDC shift (valid because ortho output
has `w == 1`, so `m[12]` moves NDC x directly). It is kept deliberately: a few
passes — fades, full-screen colour effects — genuinely want to be screen-locked.

### 3. `HudFlipY`, or: why the HUD disappeared

Getting the Y convention wrong here does not produce an upside-down HUD. It
produces **no HUD at all**, which is a much harder symptom to read.

The engine's ortho matrix already flips Y, because TR's 2D space is Y-down — so
`P_o` emits ordinary GL NDC with Y **up**. The per-eye projection is built with
`FlipProjectionY` and expects Y-**down** input. Feed it Y-up and it flips a
second time, which inverts the image *and reverses triangle winding* — at which
point backface culling silently removes the entire layer.

The signature is unmistakable once you know it: **the HUD vanishes except for the
one element drawn with culling off, and that one is upside down.** `HudFlipY=1`
(the default) cancels the extra flip.

### 4. Pre-rendered video: the passes no matrix can reach

Some things stayed double-visioned after all of the above, and for a reason that
no amount of matrix work would have fixed: **five of the engine's shaders write
`gl_Position = vec4(aCoord, 1.0)`** — straight to clip space, no matrix
involved. Pre-rendered video cutscenes run through that path. They are identical
in both eyes, carry no shear, and there is no uniform to correct.

They are detected structurally rather than by shader id: `uid[0] < 0` means the
program has no `uProjMatrix` uniform at all.

Since no matrix can move them, **the viewport is shifted instead**. The same
`HudNdcShiftX` is evaluated at `VideoDepthMetres` (6 m) and converted to a pixel
offset, then `glViewport` is displaced for that one draw — `glDrawElements` runs
immediately after `validate_draw` returns, and the next draw's `SetEyeViewport`
puts it back. Scissor is explicitly re-enabled on the eye's own half so the
shifted quad cannot bleed into the other eye (`BeginFrame` disables scissor for
its full-target clear, so it cannot be assumed on).

The result is head-locked, and it fuses. Each bypass shader id is logged once,
so if something still refuses to fuse the log names the shader instead of
costing another play session:

```
bypass: shader 63 (0x3F) has no uProjMatrix -- writes clip space directly.
        Shifting its viewport per eye.
```

This was the first cut, and a shifted viewport is a fake transform — it cannot
represent size, keystone or roll. [Phase 4](#phase-4-fmv-fixes) replaces it with
real geometry and keeps this path only as a fallback.

### Diagnostics corrected along the way

Three instruments from Phase 2 were lying once `EyeOffsetMode=3` became the
working mode, and a fourth was missing:

- **The GPU verification read the wrong uniform.** Mode 3 leaves `uViewMatrix`
  identical in both eyes *on purpose*, so sampling it reported zero separation
  and looked exactly like the failure it was built to detect. It now reads the
  `uProjMatrix` translation column (`uid[0]`) when mode 3 is active, and the log
  line names which uniform it sampled.
- **The `halves:` counter is not a depth measure**, and its alarm has been
  removed. It compares the *same pixel coordinate* in both halves, so it reports
  image **shift**: a large constant offset like the frustum shear lights it up,
  while correct parallax of a few pixels across smooth texture falls under the
  threshold and reads as zero. A low number there means nothing is wrong — the
  old "THE TWO HALVES ARE THE SAME IMAGE" warning was firing on healthy frames.
- **Silently ignored options now say so.** `WarnIgnoredOptions()` runs at
  startup and logs any ini setting the active `EyeOffsetMode` cannot act on —
  `PerEyeView=0` and `DebugEyeYawDegrees` are both inert under mode 3, which
  never writes the view matrix at all.
- **`shaders[]` indexing is bounds-checked.** `vid_setPass` assigns
  `vid_state.shader` *before* its own range check, so any code reading
  `shaders[vid_state.shader]` has to clamp first — the array is only `0xCA`
  entries and the bypass detection reads it on every draw.

### Phase 3 settings

| Setting | Default | What it does |
|---|---|---|
| `HudDepthMetres` | `4.0` | Distance the 2D layer sits at. `0` disables the fix and it double-visions |
| `HudSizeDegrees` | `55` | Angular width of the panel, seen from the game camera. Height follows the screen aspect |
| `HudLockToHead` | `0` | `0` world-locked panel; `1` flat per-eye shift, welded to the head |
| `HudFlipY` | `1` | Cancels the engine ortho's own Y flip. Leave it on — see above |
| `VideoDepthMetres` | `6.0` | Distance for clip-space-direct passes, applied as a viewport shift. `0` disables |
| `FlatHud` | `1` | Keeps the 2D layer on the flat path rather than reprojecting it as world geometry |

The generated `TombRaiderVR.ini` sets all of these explicitly, with the
reasoning inline; the defaults above apply if a key is absent.

---

## Phase 4: FMV fixes

Phase 3 got the pre-rendered cutscenes to fuse. It did it by shifting the
viewport, which is a *fake* transform — and the ways it stays fake are visible
the moment you look around during a cutscene.

Phase 4 stops faking it. The video is captured once and replayed as real
geometry, so it behaves like an object in the room rather than a rectangle
being nudged around the screen.

### Why the video needs its own path at all

Five of the engine's shaders write `gl_Position = vec4(aCoord, 1.0)` — straight
to clip space, no matrix involved. FMV cutscenes run through that path. There is
no `uProjMatrix`, no `uViewMatrix`, nothing to substitute: **every technique
Phase 2 and Phase 3 rely on is unavailable here.**

They are detected structurally rather than by shader id — `uid[0] < 0` means the
program has no `uProjMatrix` uniform at all. That condition is never true during
gameplay, so the whole video path costs nothing outside cutscenes.

### Three attempts at moving something you cannot transform

Each of these fixed the previous one's most visible artefact and left a subtler
one behind.

| Attempt | What it does | What it gets wrong |
|---|---|---|
| **Constant NDC shift** | One per-eye offset from `HudNdcShiftX` | Fuses correctly, but the panel is welded to your face and swings with every head movement |
| **Project the centre** | Transform a point straight ahead at `VideoDepthMetres` by the eye transform, project it, put the viewport there | Translation only. The panel keeps a **constant angular size** wherever it sits — at the edge of a ~94° field of view it should shrink and foreshorten, and instead it stretches |
| **Project the four corners** | Fit the viewport to the corners' bounding box | Recovers the size term, so position *and* perspective size are right. What remains is **keystone**: an off-axis quad should go trapezoidal, and a bounding box is always a rectangle |

The corner fit is what `VideoLockToHead=0` does when the offscreen path is
unavailable, and it is genuinely close. Its residual error grows with panel
width, which is why a narrower panel also looks flatter — `VideoSizeDegrees`
of 50–70 is a good cinema size, and much above 90 the keystone starts to show at
the edges. Corners behind the eye are detected (`clip.w <= 0`) and fall back to
the constant shift rather than producing a garbage viewport.

### The fix: capture offscreen, replay as geometry

`VideoOffscreen=1` (the default) removes the keystone completely, by not
approximating a transform at all:

1. **Capture.** The pass is rendered **once** into an offscreen framebuffer at
   the size the engine expected, with no per-eye trickery — a flag makes the
   whole per-eye machinery stand aside for the duration.
2. **Replay.** A genuine quad is drawn per eye, scaled to the panel, placed at
   `VideoDepthMetres` in the game camera's frame, and put through the real
   per-eye transform and projection:

```
MVP = P_eye · E · L
```

with the same terms as the HUD panel in Phase 3 — `L` scaling the unit quad to
the panel and pushing it out to `Z`, `E` the per-eye head transform, `P_eye` the
real asymmetric projection. Y is negated for the same reason as the HUD: the
per-eye projection is built for Y-down input.

The quad is then ordinary geometry. It keystones, foreshortens and rolls exactly
like the world does, because it *is* world geometry — there is no residual to
argue about. It also rasterises the video **once instead of twice**, which very
nearly pays for the extra pass.

### What that required

**A minimal renderer of our own.** `VideoPanel` carries its own `#version 150`
vertex/fragment pair (unit quad, `uMVP`, `uTex`, `uFlip`), an RGBA8 colour
texture with a `DEPTH_COMPONENT24` renderbuffer, a VAO and a VBO. The
framebuffer's completeness *is* checked — unlike the engine's own, which
discards the status.

**27 more GL entry points**, for shaders, programs, VAOs, buffers, attributes
and `glActiveTexture`. They are deliberately kept out of the loader's success
flag, on the same principle as `glGetUniformfv` in Phase 2: a driver that will
not hand them over loses the offscreen panel, not stereo. `gl::LoadedShaderApi()`
reports the outcome and the loader line now says so:

```
gl: loader ready (shader api ready)
```

If the panel cannot be built, it says so and the corner-fit path takes over:

```
video: offscreen panel unavailable, falling back to the viewport fit
       (residual keystone off-axis)
```

**Not corrupting the engine's state cache.** This is the part that would break
everything if it were missed. `Replay` saves and restores the VAO, array buffer,
program, active texture unit, 2D texture binding, and the depth/blend/cull/
scissor enables. The panel is opaque and owns its pixels, so depth testing,
blending and culling are all switched off — which also means quad winding cannot
matter.

Restoring GL state is not enough on its own, though, because the engine keeps
its *own* shadow copy of what it last bound and skips redundant calls. We went
behind its back, so the parts we disturbed are explicitly invalidated in
`vid_state_prev`: `shader = 0xCA` — the same out-of-range sentinel `init_ogl`
itself uses — plus null `vb`/`ib` and `tex[0] = 0xFFFFFFFF`. The engine then
rebinds on its next draw instead of trusting a cache that is no longer true.

### Phase 4 settings

| Setting | Default | What it does |
|---|---|---|
| `VideoOffscreen` | `1` | Capture and replay as real geometry. `0` uses the viewport fit, with residual keystone |
| `VideoDepthMetres` | `6.0` | Distance the video panel sits at. `0` disables the video path entirely |
| `VideoSizeDegrees` | `60` | Angular width of the panel. Also caps the keystone error on the fallback path |
| `VideoLockToHead` | `0` | `0` anchors the panel to the game camera; `1` welds it to your head |
| `VideoFlipV` | `0` | Flip the captured video on replay. Only needed if a cutscene comes out upside down |

As with Phases 2 and 3, the generated `TombRaiderVR.ini` sets these explicitly.

---

## Phase 5: VR controller support

The Touch controllers are presented to the game as an ordinary Xbox pad. No
virtual-pad driver, no ViGEm, no code patching, and no inline hook — the whole
mechanism is one overwritten function pointer.

### Why there is nothing to hook, and why that is fine

The game resolves XInput at runtime rather than importing it:

```c
hMod = LoadLibraryA("xinput1_3.dll");
if (hMod || (hMod = LoadLibraryA("xinput9_1_0.dll"), hMod)) {
    _XInputGetState = GetProcAddress(hMod, "XInputGetState");
    _XInputSetState = GetProcAddress(hMod, "XInputSetState");
}
```

So there is no import table entry to patch — but there *is* a function-pointer
global, exactly the shape the Ghidra trace found `glUniformMatrix4fv` in.
Overwriting `_XInputGetState` (RVA `0x00692858`) makes the game call us
directly.

The payoff is larger than it looks, because of what the engine does with the
result. `inputUpdate()` polls pads 0–3 every frame and, the moment a poll
returns `ERROR_SUCCESS` with any button or stick active, sets
`app.input_type = INPUT_TYPE_XB`. **Simply answering the poll switches the game
to the Xbox control scheme and its on-screen prompts** — no separate work to
make the button hints say the right thing.

Three details keep it well-behaved:

- **Only pad 0 is claimed.** Indices 1–3 pass straight through to the real
  XInput, or report "not connected" if there is none.
- **A physical pad is merged, not replaced.** If one is plugged in, its buttons
  are OR'd with ours and the larger magnitude wins on each stick axis and
  trigger — so a real pad keeps working alongside the controllers.
- **The pointer is re-asserted every frame** from `ogl_present`. It is a couple
  of instructions, and it means the override survives the game re-resolving
  XInput and self-heals if the mod loaded before `WinMain` got there. On unload
  the original pointer is put back.

### Reading the controllers

`VRSystem::ReadControllers` uses OpenVR's **legacy** input API —
`GetTrackedDeviceIndexForControllerRole` to find each hand, then
`GetControllerState`. That is a deliberate choice: the mod ships no action
manifest, so SteamVR runs it in legacy mode and fills these structures in. The
modern Input API would require a manifest plus per-controller binding files,
which is a lot of machinery for "act like an Xbox pad".

Axes come from the legacy Touch layout — `rAxis[0]` the stick, `rAxis[1].x` the
trigger, `rAxis[2].x` the grip — and buttons through `ButtonMaskFromId`, with
`k_EButton_A` as the lower face button, `ApplicationMenu` as the upper, and
`SteamVR_Touchpad` as the stick click.

One runtime quirk is handled explicitly: some runtimes report the trigger and
grip **only as buttons**, with the axis flat. A pressed trigger or grip mask
therefore promotes its axis to 1.0 when the analogue value reads low.

### The mapping

| Action | Control | XInput |
|---|---|---|
| Move | Left stick | `LX` / `LY` |
| Look | Right stick | `RX` / `RY` — yaw only, see [Phase 10](#phase-10-decoupled-pitch) |
| Jump | Right lower face button | `A` |
| Roll | Right upper face button | `B` |
| Action | Left upper face button or both grips during gameplay | `Y` |
| Shoot | Right trigger | `RT` |
| Equip weapon | Left trigger | `LT` |
| Duck | Left grip | `LB` |
| Walk | **Right grip** | `X` |
| Sneak | Right grip + left upper face button | `RB` (the chord consumes `X` and `Y`) |
| Sprint | Left stick click | `L3` |
| System menu | Left lower face button | `BACK` (or `START`) |
| Photo mode | Both stick clicks | `L3` + `R3` |
| D-pad | Right stick click + left stick | `DPAD_*` — see [Phase 8](#phase-8-d-pad-input-support) |

Two of those deliberately differ from the flat-screen defaults, because they
suit hands better than thumbs:

- **Walk is the right grip**, not a face button, so it can be held while the
  left thumb keeps moving. The game binds Walk to XInput `X`, so the grip emits
  only `X`. TR6 binds Sneak to `RIGHT_SHOULDER`, so holding Y with the right
  grip consumes the ordinary Walk and Action signals and emits Sneak instead.
  Both grips take priority in gameplay: they hold Action without also sending
  Duck, Walk or Sneak. This works in TR4, TR5 and TR6, and after a physical pad
  is merged. In menus and FMVs the grips retain their individual bindings.
  Photo Mode keeps its [native L3+R3 chord](https://www.tombraider.com/news/video-games/photo-mode-returns-when-tomb-raider-iv-vi-remastered-launches-on-february).
- **System is the left hand's lower face button.** Touch has no Start or Back of
  its own, and putting either on a chord made it awkward to reach mid-play.
  `GamepadMenuUsesBack=0` sends `START` (pause/inventory) instead —
  `inputUpdate()` decodes `BACK` to internal key `0x62` and `START` to `0x63`,
  and which one a given screen treats as "System" lives in the game DLLs, so it
  stays switchable rather than hard-coded.

Dual-grip Action is added after the Y+LT first-person and Y+RT graphics chords,
so gripping while using a trigger cannot switch either mode. The mapping passes
mocked TR4/5/6 input tests but still needs an in-game block-push/pull check in
each game. The 2026-09-25 Release/x64 DLL containing it is installed; the prior
DLL is preserved as `TombRaiderVR.dll.pre-dual-grip-action-20260925-161204`.
The installed INI was not overwritten.

### Diagnostics

Touch's legacy button ids differ between runtimes, so a button landing in the
wrong place is a plausible failure. `GamepadLogButtons=1` dumps the raw mask
whenever it changes, which says what a button actually set rather than costing a
play session to find out:

```
pad: L raw=0x0000000200000002 stick=(+0.00,+0.00) trig=0.00 grip=0.00
```

The install itself logs once, along with the whole mapping, so the log records
what the controls were on that run:

```
pad: Touch controllers presented as an Xbox pad (_XInputGetState ...)
pad: move=Lstick look=Rstick jump=A(R lower) roll=B(R upper) action=Y(L upper)/LB+RB(both grips)
     system=BACK(L lower) walk=LS+RB(R grip) sneak=RB+Y duck=LB(L grip)
     equip=LT shoot=RT sprint=L3 photo=L3+R3
```

### Phase 5 settings

| Setting | Default | What it does |
|---|---|---|
| `GamepadEnabled` | `1` | Present the controllers as pad 0. `0` leaves XInput alone entirely |
| `GamepadMenuUsesBack` | `1` | Left lower face button sends `BACK` (System). `0` sends `START` (pause) |
| `GamepadLogButtons` | `0` | Log raw legacy button masks on change |

All three are set explicitly in the generated `TombRaiderVR.ini`.

---

## Phase 6: TR6 support

> **Superseded by [Phase 16](#phase-16-tr6-native-stereo-support) for normal
> play.** Phase 6 is retained as the history and implementation of the AER
> fallback used when native TR6 stereo is disabled or its build check fails.

Everything up to here was built against TR4 and TR5, which draw the world
straight to the backbuffer. **TR6 (Angel of Darkness) does not**, so it needs a
different stereo boundary. Which game is running is read from `gGame` (`0` =
TR4, `1` = TR5, `2` = TR6).

### Why per-draw duplication cannot work in TR6

TR6 renders the scene into the engine's own **2560×1440 offscreen textures** and
composites at the end. The measurement is stark:

| Game | World draws offscreen |
|---|---|
| TR4 / TR5 | ~9% |
| TR6 | **572,970 of 579,945** — 98.8% |

Phase 2 duplicates each draw into the two halves of one double-wide target. In
TR6 there is almost nothing on the backbuffer to duplicate, and the pieces that
*are* there cannot be split:

- the final composite samples those offscreen textures **whole, with 0–1 UVs**,
  so widening them breaks the sampling
- duplicating individual draws into halves of a target the composite later reads
  entire would corrupt it

### Alternate-eye rendering

Each frame is rendered entirely as one eye and blitted into that eye's half.
The other half keeps its preceding image. Both eyes are stereo-correct, but each
updates at half the rendered frame rate. `AlternateEyeGame6=0` disables AER.

#### The scratch target, and the flicker it prevents

The obvious implementation — render into one half of the double-wide target and
leave the other alone — fails immediately, and the reason is worth recording.
`FBO_default` *is* the double-wide target, so **any full-target `glClear` the
engine issues wipes both halves**. That reads as a hard flicker at half the frame
rate, with no head movement needed at all.

AER therefore points `FBO_default` at a **single-eye scratch framebuffer** sized
to the engine's own render resolution and copies the result into the selected
eye at present. The blit rescales, which is correct — the per-eye projection
already carries the HMD's aspect.

If the scratch cannot be created, that is logged plainly rather than silently
degrading:

```
stereo: mono scratch unavailable -- TR6 native stereo and
        alternate-eye fallback are disabled
```

#### AER only where it earns its cost

AER is worth half the frame rate only when there is an offscreen 3D scene to
reach. TR6's main menu and its FMVs are 2D — they draw straight to the
backbuffer, where ordinary per-draw duplication works at **full** rate and looks
better doing it.

The two are separated by counting offscreen world draws per frame, which needs
no new symbols: gameplay runs ~800, menus and video are near zero.
`AlternateEyeMinOffscreen` (default `50`) is the threshold.

The decision is **latched once per frame**, from the previous frame's count.
That matters: `AlternateEyeActive()` is consulted from the draw path, the
injection gate and present, so a mid-frame flip would leave one frame split
across two different targets. The cost of latching is one frame of lag at a
transition.

```
tr6: alternate-eye ON -- offscreen 3D scene (812 offscreen world draws last frame)
tr6: alternate-eye off -- 2D, full rate (0 offscreen world draws last frame)
```

### The FMV collision, and a sixth hook

Phase 4 detects video by a structural test: a pass with no `uProjMatrix`
(`uid[0] < 0`) writes clip space directly, so it must be an FMV quad.

**In TR6 that test is ambiguous.** TR6's scene composite — the draw that puts the
whole rendered world on screen — also goes through a clip-space-direct shader.
It has exactly the same signature as an FMV quad, so Phase 4's capture-and-replay
grabbed it and turned the entire game into a small floating panel.

The fix is a **sixth inline hook**, on `fmvShow` (RVA `0x00011350`), which the
engine installs as `app.fmvShow` and calls once per frame for as long as a
cutscene is on screen. That is an exact "a video is on screen right now" signal
where the shader test was only an approximation. A frame counter with a
two-frame tolerance turns it into `FmvActive()`.

TR6 therefore uses the offscreen panel **only while a video is genuinely
playing**; its gameplay and menus are left alone. TR4 and TR5 keep the original
behaviour, which was already working.

One subtlety that caused a real bug: the capture path and the viewport-shift
fallback must agree about whether the panel is handling a pass. The first cut
skipped the capture in TR6 but left the fallback suppressed by "the panel
exists", so TR6's bypass passes got **neither** — head-locked and unfused. Both
now consult the same `VideoOffscreenActive()`.

### Frame-rate reporting

Because AER halves the per-eye rate, the health report says what each eye is
actually getting:

```
perf: 89.7 fps rendered -- alternate-eye, so each EYE updates at half that
```

### Phase 6 settings

| Setting | Default | What it does |
|---|---|---|
| `AlternateEyeGame6` | `1` | Enable AER; Phase 16 uses it as the fallback when native stereo is disabled or unavailable |
| `AlternateEyeMinOffscreen` | `50` | Offscreen world draws per frame before AER engages. Raise it if a menu drops to half rate; lower it if gameplay does not engage; `0` forces AER on for every TR6 frame |
| `VideoSkipGame6` | `1` | In TR6, use the offscreen video panel only while `fmvShow` says a video is playing |

All three are TR6-only: `gGame != 2` takes the TR4/TR5 path regardless.

---

## Phase 7: culling fix

> **Superseded by Phase 7B.** Everything below is what was built without
> symbols and how it was reasoned about; the hop expansion, its exclude list
> and its head test have since been replaced by a real portal traversal. The
> measurements here still hold and are the reason several obvious approaches
> are not tried again — read it for those, not for the settings, which are
> gone.

Turn your head far enough away from where the game camera is pointing and there
is nothing drawn — no walls, no floor, just void behind Lara. Phase 7 fixes it.

The engine submits only the rooms in a list built by **portal traversal** from
the game camera. That set is correct for a flat screen and for the camera's
facing, and VR breaks both assumptions at once: the headset sees wider, and it
can look somewhere the camera is not.

### The culling is not in the exe

This was recorded as a dead end for good reason. `tomb456.exe` contains no
culling code at all, and the obvious lever does nothing:

> Hooking `ogl_setPersp` and scaling `tanY` by 2 widened the projection from
> **64.4° to 103.1° and produced zero extra geometry.** On TR6, 20× was the same.

The traversal lives in `tomb4.dll` and `tomb5.dll` — and those ship **without
PDBs**, 1791 unnamed functions in TR4 alone. Searching that statically for a
portal test is a long shot.

### Finding the code by asking the game

The way in is a nice trick, and it is what made the rest possible. The DLL has
to call across into the engine to draw anything — and the mod is already sitting
on those calls. **The return address inside the `vid_setPass` and `ogl_drawVB`
detours is a code address inside the DLL.**

`LogCallsites=1` records `_ReturnAddress()` at both hooks, deduplicated and
capped, and resolves each to `module+RVA` (via `EnumProcessModules`, since the
DLLs are ASLR'd and the raw pointer is useless on its own):

```
callsite: vid_setPass    <- tomb4.dll+0xC53A1
callsite: ogl_drawVB     <- tomb4.dll+0xC6018
```

One gameplay frame hands over the exact RVAs of the functions that submit
geometry, and the call graph leads back from there to whatever decided what to
submit. A blind search becomes a starting point.

What that turned up, for both DLLs: the traversal, the draw list (`int16` room
indices) and its count, the room array (stride `0x130`), and within a room its
portal list at `+0x08`, world position at `+0x28`, render clip rect at
`+0x4C`–`+0x52`, and `flipped_room` at `+0x68`.

### The mechanism: expand along portal connectivity

The engine already does the necessary thing for a special case — after traversal
it appends any room flagged `0x40000`, bypassing portals entirely. Phase 7 does
the same, for rooms reached by walking **portal connectivity** outward from the
visible set, `PortalHops` deep (default 3).

Connectivity rather than orientation is the whole point. The engine's traversal
runs in the **game camera's** space, because VR is injected at the shader
uniform and the engine's own view matrix never rotates with your head. A portal
beside or behind the camera fails the near-plane test *inside* the clipper,
before its rectangle is ever consulted. Connectivity has no orientation bias at
all: a room through the door behind you is one hop away whichever way the camera
happens to face.

Two hops is conservative; three covers deeper sightlines for about ten more
rooms and no measurable frame cost.

**The hook goes on the consumer, not the producer.** Appending at the traversal
function leaves a second traversal pass to run over the enlarged list, and that
pass overran it. At the room renderer the list is final.

**Full-screen clip rects are required, not lazy.** The engine's portal-clipped
rects are screen boxes computed for the game camera's view; we render from the
HMD's, so under head rotation they scissor the wrong region entirely. Appended
rooms also still carry the inverted "empty" bounds the previous frame reset them
to. `DrawAllRoomsClipRect=0` exists only as a diagnostic, to separate "the list
changed" from "the rects changed".

### Four things that were tried and measured wrong

Each of these looks reasonable and is not. They are recorded so nobody spends a
session re-deriving them.

| Approach | Verdict |
|---|---|
| **Append rooms by distance** from the camera | Works, but proximity is the wrong criterion — it happily adds a stacked room that shares world space with the one you are standing in and that no portal reaches, drawing foreign geometry over your own. Kept as `DrawAllRooms` for A/B only |
| **Widen the portal rectangle** | **Measured inert.** Swept to 200% of screen each way it changed neither geometry nor frame rate; 20× on TR6 was the same. The near-plane test rejects the portal before the rectangle is consulted, so no rectangle can rescue it. Removed |
| **Reject hop rooms whose world AABB overlaps a drawn room** | **Measured wrong.** A TR room box is a rectangle around an irregular interior, and neighbours share a border of wall sectors, so ordinary adjacency shows a 2048×2048 overlap. It rejected six legitimate pairs at once. Removed |
| **Match flip pairs by world position** | Replaced by the engine's own `flipped_room` field, which is unambiguous: the live room names its storage copy, and the copy holds `-1`. Skipping storage copies is now unconditional rather than a guess |

### The head test, and why it is off by default

Hop expansion adds any portal-connected room and draws it **without** a portal
clip, so a room that is connected but not actually visible through that doorway
still gets drawn — and where it shares world space with somewhere you can see,
you get foreign geometry laid over your own. That is the room-215 class of bug.

`PortalHeadTest=1` transforms the connecting portal's four corners
world → eye → clip, using the view matrix actually injected last frame and the
VR projection, and rejects the room if the quad lands wholly outside the
frustum. Going all the way to clip space means the projection supplies
handedness and field of view, so there is nothing to restate by hand. It is
deliberately conservative — a portal straddling the near plane counts as
visible, and `PortalHeadMargin` (0.35 NDC) expands the frustum — because losing
geometry is the worse failure.

The engine performs exactly this test already. It just performs it from the game
camera; doing it from the head is the piece that was missing.

It ships **off**, and the reason is instructive. The first version transformed
portals using the view matrix's translation column — which is not the `-R·p` a
textbook view matrix carries, because the engine packs something
camera-position-shaped there instead. The error scaled with world coordinates,
so the test behaved on a 116-room level and rejected essentially every portal on
a 242-room one. It now uses rotation only, with the camera position taken from
the engine's own globals, but it is unproven, so it is opt-in.

The measurement discipline changed with it: the pass/reject ratio is reported
**every 2000 decisions, not once**. A single early sample is exactly what hid
the bug — the one-shot fired in a small level where the test behaved, and never
re-measured on the large one where it did not.

```
rooms[TR4]: traversal found 18 of 116 rooms (hops 3, headtest off)
rooms[TR4]: 3 hop(s) added 27 -> drawing 45 of 116
rooms[TR4]: head test -- 1840 passed, 160 rejected (8% rejected) of the last 2000 portals
```

`DrawAllRoomsExclude` is the blunt companion: a per-level list of room indices
hop expansion must never add. It is honestly labelled — what makes room 215
special has never been identified, and three general mechanisms failed to
characterise it — but it is one line and it demonstrably works while the general
ones are still being proven.

### Two things that would have been silent corruption

- **The draw list holds 200 entries, not 206.** Deriving the bound as
  `(drawCount − drawList) / 2` gives 206 and is wrong: scanning both DLLs for
  RIP-relative references landing between the two addresses shows unrelated
  globals in the gap. TR5 levels have 242 rooms, so this is not academic — the
  wrong bound wrote over live engine state every frame.
- **Nothing writes `room+0x4B`.** That byte looks like a render flag and is not:
  it feeds a pass that relocates two-room objects with `ItemNewRoom`, moving
  items out of the room you are standing in so they stop blocking you. Writing
  it is what originally let Lara walk through a wall.

The hook itself is installed lazily — the game DLLs load after the mod does —
and verifies a per-game prologue (`48 8B C4 41 54` for TR4, `48 89 5C 24 10` for
TR5) before patching. A mismatch is logged once and leaves stock culling in
place rather than retrying every frame. TR4 and TR5 only; TR6 has no entry in
the table.

**The same per-build table carries more than culling.** Each row also holds
`laraWaterStatus`, the address of Lara's own water state in that DLL, which
[Phase 10](#phase-10-decoupled-pitch) uses to restore stick pitch while
swimming. Two consequences follow from it living here:

- A build whose timestamp is not in the table loses the swimming exception along
  with the culling fix, and says so in the log rather than reading a wrong
  address.
- Both `tomb4.dll` and `tomb5.dll` stay mapped for the whole session, so any
  lookup in this table must select on `CurrentGame()` rather than on whichever
  module happens to resolve first — see
  [Both game DLLs stay loaded](#both-game-dlls-stay-loaded).

The `ROOM_INFO` water bit at `0x6C` documented above was also found during that
work, not this one.

### Phase 7 settings

**All of these are retired.** See Phase 7B for what replaced them; an ini
that still sets one gets a log line on startup saying so.

| Retired setting | Replaced by |
|---|---|
| `PortalHops` | nothing — depth is decided by the geometry |
| `PortalHeadTest`, `PortalHeadMargin` | the traversal itself, always on; `CullFovMarginDegrees` for the margin |
| `DrawAllRoomsExclude` | nothing — a real traversal cannot reach the rooms it existed to exclude |
| `DrawAllRoomsClipRect` | `CullWidenBounds`, same job |
| `DrawAllRooms` | nothing |
| `RoomDumpKey` | `CullDumpKey` |

| Kept | Default | What it does |
|---|---|---|
| `LogCallsites` | `0` | Log each distinct DLL-side call into `vid_setPass` / `ogl_drawVB` as `module+RVA`. Verbose; a research tool, not a play setting |

---

## Phase 7B: the culling fix, done properly

Phase 7 stopped the void. It did not decide what to draw — it decided how far to
spread. This replaces it with the thing the engine itself does, run from the head
instead of from the game camera.

**What changed underneath: the game DLLs turned out to ship private PDBs.**
Everything Phase 7 recovered by tracing call sites and reading a decompiler is in
`tomb4.pdb` and `tomb5.pdb` by name. `FUN_18002ea30` is `GetRoomBounds`.
`DAT_18063dc60` is `draw_rooms`. The function hooked as "DrawRoomList" is
`PrintRoomsList`. Re-deriving the whole table from the symbols and diffing it
against the old one is now `tools\verify_addresses.py`, and the first run of it
matched on all 155 checks — including the two that were hardest to trust:

* `camX/camY/camZ` were three separate globals in the old table. They are
  `w2v_matrix[3]`, `[7]` and `[11]`: one matrix, and the same one the game's own
  culling uses.
* `laraWaterStatus`, found by snapshotting the writable data section on dry land
  and diffing it while swimming, is `lara` + 12 — exactly where TR1-3's PDBs put
  `lara_info::water_status`.

### The mechanism: narrow the frustum at every doorway

`src\PortalCull.cpp` hooks `PrintRoomsList` — the same consumer-side hook point
Phase 7 chose, and for the same reason — and walks portals out from the camera's
room a second time. The apex is the tracked head. The frustum is the headset's,
widened to a symmetric superset that contains both eyes.

At each doorway the portal quad is transformed into eye space, clipped against
the planes arriving from the previous room, and a new plane is built from the
head through each surviving edge. A room enters the list only if it can really be
seen through that chain of openings. The engine's own back-face test is kept
unchanged, with the head substituted for the camera — which is free, because the
world-to-eye transform is orthogonal, so `dot(A*n, A*(p-c) + t)` equals
`dot(n, p - headPos)` and the comparison survives being done in eye space.

| | Phase 7 | Phase 7B |
|---|---|---|
| criterion | hop count `N` | whether the head can see through the doorways |
| depth | `PortalHops`, tuned per level | decided by the geometry; the budgets only bound the worst case |
| rooms you cannot see | drawn, then excluded by index (`DrawAllRoomsExclude=215,12`) | not reached |
| flip storage rooms | reached, then filtered by `flipped_room` | unreachable by construction |
| addresses | `FUN_18002ea30`, `DAT_18063dc60` | `PrintRoomsList`, `draw_rooms`, by name |

The flip-room line deserves its own sentence, because it cost a debugging
session. A flip map's inactive half is a real entry in the room array at the same
world position as its live twin, so any mechanism that adds rooms the traversal
never reached will happily add both and draw one over the other. No live room's
portals name a storage room, so a real traversal cannot reach one — the same
reason the engine's own never draws one. There is no exclusion list here because
there is nothing to exclude.

### The lever Phase 7 missed: items

Fixing the room list is not enough, because the same camera-shaped assumption is
baked in one level further down. `S_GetObjectBounds` answers 1 / −1 / 0 for an
item's bounding box, and zero is reached two ways — every corner behind
`phd_znear`, or the projected rectangle missing the screen rect. Both are the
**game camera's** opinion. An enemy behind the camera fails the first; one beside
it fails the second.

So the rooms Phase 7 forced in were drawn with their furniture, enemies and
pickups missing. `CullObjects` hooks that test and second-guesses the zero answer
— only ever upward, to −1 ("visible, clip it"), never the other way.

No menu gate is needed, and that was checked rather than assumed: scanning both
DLLs for direct calls gives only world-drawing paths (`CalcItemMatrices`,
`CalculateLaraMatrices`, `CalcLaraMatricesHDAnim`, `DrawStaticObjects`,
`DrawRooms` and per-object helpers). The inventory ring draws through
`DrawAllInvItems` / `DrawThisInvItemSpecifically`, which do not call it.

### `bound_active`, and a rule that turned out to be unnecessary

Phase 7 deliberately never wrote `room+0x4B`, on the grounds that it fed an
item-relocation pass and that writing it was what once let Lara walk through a
wall. The PDBs settle it: that byte is `ROOM_INFO::bound_active`, and scanning
both DLLs for every one-byte access at `+0x4B` finds it touched by exactly four
functions — `GetVisibleRooms`, `GetRoomBounds`, `SetRoomBounds` and
`PrintRoomsList` — plus level load and savegame. `ItemNewRoom` is not among them.

What the old code was most likely hitting is that the engine keeps an **enqueue
count** in the upper bits, and only bit 0 means "already in `draw_rooms`".
Assigning the byte clobbers that count for a room still sitting in `bound_list`.
Phase 7B ORs bit 0 in and never assigns, which lets it use the engine's own dedup
instead of a linear scan of the draw list per portal.

One wrinkle needed handling: after the traversal, `GetVisibleRooms` appends every
room flagged `0x40000` straight into `draw_rooms` **without** touching
`bound_active`. Trusting the bit alone would append those twice, so the list is
marked once up front before the traversal starts.

### One space, one sign

The whole thing turns on a single relationship. The game's culling works in **phd
view space** — `v = R*(p − camPos)`, X right, Y down, **+Z forward**
(`SetRoomBounds` tests `z < 1` for "behind the camera"). The mod's matrices work
in the space `mView_packed` defines, which is the same transform with its **third
rotation row negated**, i.e. −Z forward.

That is not inferred. `vid_setViewMatrix` (`tomb456.exe` RVA `0x0000B960`) builds
it from the same `int[12]`: it scales the nine rotation terms by 1/16384, negates
exactly `m[8]`, `m[9]` and `m[10]`, and writes `m[3]`, `m[7]` and `m[11]` through
**raw**.

That last detail explains a Phase 7 bug. The translation column is not the `−R*p`
a textbook view matrix carries — when the input is `w2v_matrix` it is the
camera's world position, unscaled. Using it as though it were `−R*p` produces an
error that scales with world coordinates, which is exactly why the first head
test behaved on a 116-room level and rejected essentially every portal on a
242-room one. Nothing in Phase 7B uses the packed matrix at all; the camera
position comes from `w2v_matrix` directly.

### What to watch

The health report prints it every 1800 frames:

```
cull: 31.2 rooms/frame from the engine + 8.4 added by the head frustum, 12.1 items rescued/frame
```

`added = 0` forever means either the head never left the game camera's cone or
the traversal is not running; the `cull: head-frustum portal traversal live` line
tells the two apart. `CullDumpKey` (F8) prints the whole list with the added
rooms starred, which turns "that wall is missing" into a room number.

### Phase 7B settings

| Setting | Default | What it does |
|---|---|---|
| `PortalCulling` | `1` | The whole feature. `0` is stock engine behaviour |
| `CullFovMarginDegrees` | `8` | Angle added to each half of the culling frustum — covers canted displays and the few ms between the pose that culls and the pose that renders |
| `CullMaxDepth` | `16` | Doorways deep. A worst-case bound, not a visibility criterion |
| `CullMaxPortals` | `4096` | Portals per frame. Same |
| `CullFarUnits` | `0` | Optional distance limit in world units, off by default. A frame-rate lever, not a fix |
| `CullWidenBounds` | `1` | Widen every listed room's clip rect to the whole target (the old `DrawAllRoomsClipRect`) |
| `CullObjects` | `1` | Extend the fix to items, so the added rooms are not empty |
| `CullDumpKey` | `0x77` | F8. Dump the draw list, added rooms starred |

## Phase 8: D-pad input support

Touch controllers have no D-pad, and the game wants one — menus, the inventory
ring, and anything else that navigates by discrete directions rather than by
analogue movement. Phase 8 adds one as a **shift layer**: hold **R3** (click the
right stick) and the left stick emits D-pad directions instead of movement.

### Why R3 is the modifier

L3 is the obvious candidate and the wrong one. **L3 is Sprint, and sprint in this
game is held *while* running forward** — so "hold L3, push the left stick
forward" is already a gesture in live play. Putting D-pad up on the same
combination would mean choosing between them.

R3 was the only genuinely spare input on the pad: it emitted `RIGHT_THUMB` and
nothing else.

### Nothing is taken away

The shift is designed so that no existing behaviour is lost:

- **A plain R3 click still emits `RIGHT_THUMB`.** The D-pad only appears once the
  left stick is actually deflected past the deadzone, so R3 held with the stick
  centred behaves exactly as it did before, and whatever the game binds it to
  survives.
- **The left stick's movement axes are zeroed while shifted.** Otherwise you
  would walk and press a direction at the same time, which defeats the point of
  a shift layer.

### Only the dominant axis fires

The larger of `|x|` and `|y|` wins, so the stick cannot emit up and left at
once. A real D-pad allows diagonals; this deliberately does not, because the
feature is mostly for menus and inventory — where a diagonal reads as two
separate navigation events and moves the selection twice for one flick of the
thumb.

`DpadShiftDeadzone` (default `0.5`) is how far the stick must travel before a
shifted press registers. Raise it if directions trigger too easily, lower it if
they feel stiff.

The mapping line in the log records the shift when it is on:

```
pad: move=Lstick look=Rstick jump=A(R lower) ... sprint=L3 photo=L3+R3 dpad=R3+Lstick
```

### Phase 8 settings

| Setting | Default | What it does |
|---|---|---|
| `DpadShift` | `1` | Hold R3 to turn the left stick into a D-pad |
| `DpadShiftDeadzone` | `0.5` | Stick travel required before a shifted direction registers, 0–1 |

---

## Phase 9: inventory fix

Every inventory item drawn on top of every other one, in a single stack. The
cause is a good illustration of how Phase 2's central move — overwrite the
projection with the eye frustum — can throw away information that was not
obviously there.

### The engine lays the inventory out through the projection

`vid_setPerspOffset` (RVA `0x0000B8D0`) writes `e02` and `e12` — `m[8]` and
`m[9]`, the two shear terms — **and nothing else**. A shear of `s` offsets NDC by
`−s` at every depth, so it is how the engine places a draw on screen without
touching its geometry.

The inventory is laid out entirely with it. One captured frame, nine item draws:

| | Model matrix | View matrix | Joints | `e02` |
|---|---|---|---|---|
| item 1 | identity | identity | 1 | `2.0250` |
| item 2 | identity | identity | 1 | `1.3500` |
| item 3 | identity | identity | 1 | `0.6750` |
| item 4 | identity | identity | 1 | `0.0000` |
| item 5 | identity | identity | 1 | `−0.6750` |
| item 6 | identity | identity | 1 | `−1.3500` |

Identical in every respect except one number, evenly spaced `0.675` apart.
**That even spacing is the horizontal bar**, and the values past ±1 are the
items scrolled off the sides of the screen.

Overwriting `mProj[1]` with the per-eye frustum discarded all of it. Every item
then got the same shear, so every item landed in the same place — the stack.

### The fix: the shear is a skew of the engine's camera space

The insight that makes this clean is that the shear is not really a property of
the projection at all. A projection carrying `(sx, sy)` is *exactly* the same
projection without it, applied to space pre-skewed by

```
(x, y, z) → (x + (sx/m00)·z,  y + (sy/m11)·z,  z)
```

Multiply it out and the two agree term for term. So reproducing the engine's
placement means post-multiplying whatever projection we ended up with by that
skew — which touches column 2 alone, and is therefore four multiply-adds rather
than a matrix product.

Adding the engine's shear to the eye's own keeps both: the frustum asymmetry the
headset optics need **and** the engine's placement.

### Two things the first attempt got wrong

The obvious version — add the shear straight onto the eye frustum — fails twice
over, and both failures are worth knowing.

**Wrong side of the per-eye transform.** Every mode ends up evaluating `P · E · v`,
and the engine's shear must multiply the **engine's** `v.z`. Sitting in `P`, it
multiplied `(E·v).z` instead, so the offset swung with head orientation and the
row sheared as you looked around. `EyeOffsetMode=3` makes that worst, because
there the entire head transform lives in the projection. Applied after `E`, the
skew reaches `v` itself.

**Same NDC is not the same angle.** A shear of `s` offsets by `s·tan(fov)`, and
the engine's frustum is not the headset's. Measured here:

| | Engine | Eye |
|---|---|---|
| `tanX` | 1.119 | 1.108 |
| `tanY` | **0.629** | **1.197** |

So the row landed correctly — the two are 1% apart horizontally — while the
vertical placement was thrown nearly twice as far as intended, out into the lens
distortion. That was the fishbowl. Dividing by the engine's own `m00`/`m11`
converts angle to angle, and the ratio falls out on its own.

Because `ogl_setPersp` and `ogl_setPerspAngles` both explicitly zero `e02`/`e12`,
this is exactly zero during gameplay and costs nothing there. Only the engine's
screen-placed elements ever carry a shear.

`ProjOffsetScale` trims the whole offset. `1.0` reproduces the engine's layout
exactly, so the inventory row spans the same **angle** it does flat — which on a
96° game frustum is a wide row to sweep your eyes across. Lower it to pull the
elements toward the centre; `0` collapses them back into one stack, which is
what the bug looked like.

### The diagnostic that found it

The frame tracer from Phase 1 answers "which target is the scene drawn into".
A mislaid layer needs the other question: *of the three matrices that can place
an element — projection, view, model — which one carries the difference between
one element and the next?*

`DumpDraws` plus `DumpKey` (F9) answers it directly. Set a few hundred, get the
screen in question up in the headset, press the key, and each draw logs its
projection, view translation, model translation, joint count and render target —
**before** any substitution, so what appears is what the engine set:

```
dump 4   f=9312 sh=62  world-slot/persp  P[x=0.8938 y=1.5898 z=-1.0000 w=-1.0000
         shear=0.0000,0.0000 ofs=0.0000,0.0000]  V=(0.0,0.0,0.0)
         M=(0.0,0.0,0.0)  joints=1 rt=0
```

The code is blunt about why it exists: guessing is how the ortho-3D theory below
got written and shipped inert.

The health report gained two counters to match — `projoffset=` counts draws
carrying a shear, and `ortho3D=` counts the path below. Both are zero during
ordinary gameplay, so a non-zero value is itself the signal.

### The ortho-3D path, which was the wrong theory

`vid_setOrtho3D` (RVA `0x0000B880`) copies `mProj[0]` — the **ortho** matrix —
into `mProj[1]` and points `vid_state.proj` at it. Pointer identity, which is all
`IsWorldPass()` has, therefore reports "world space" for a pass that is
orthographic, and handing it a per-eye perspective frustum would divide an ortho
layout by a depth it was never built for.

`IsOrthoProjection` classifies by matrix **content** instead: `ogl_setPerspAngles`
writes `e32 = −1, e33 = 0` while `ogl_setOrtho` writes `e32 = 0, e33 = 1`, which
in the engine's column-major layout are `m[11]` and `m[15]`.

**This was written for the stacked inventory and was wrong about it.** The
inventory is a perspective pass placed by `vid_setPerspOffset`, not an ortho one.
Measured across a TR4 and a TR5 session, the ortho-3D path **never fired once**.

It is kept anyway, for a defensible reason: an ortho matrix must not be replaced
by a perspective frustum whatever draws it, and the `ortho3D=` counter reports
honestly if it ever does. If it fires, the layer keeps the engine's own
projection and gets stereo a different way — a flat per-eye NDC shift by default,
because an ortho projection has no perspective divide, so moving `m[12]` is pure
convergence and every relative x, y **and z** survives it. That last point is why
this defaults the opposite way to the HUD: these are 3D meshes with real depth,
and the HUD's panel discards the z input, which would leave every item
z-fighting itself. `Ortho3DLockToHead=0` builds the world-locked panel instead,
like the HUD's `Q = P_persp · E · L · P_o` but with `L`'s z column filled in so
ortho depth lands in a slab rather than collapsing onto one plane.

### Phase 9 settings

| Setting | Default | What it does |
|---|---|---|
| `PreserveProjOffset` | `1` | Re-apply the engine's `vid_setPerspOffset` shear as a camera-space skew. **This is the inventory fix** |
| `ProjOffsetScale` | `1.0` | Trim on the whole offset. Lower pulls elements toward the centre; `0` reproduces the stack |
| `Ortho3D` | `1` | Classify ortho-in-the-world-slot passes by content and keep the engine's projection. Never observed to fire |
| `Ortho3DDepthMetres` | `2.0` | Where that layer converges. `0` leaves it exactly as drawn |
| `Ortho3DLockToHead` | `1` | `1` flat per-eye convergence shift; `0` world-locked depth slab |
| `Ortho3DSizeDegrees` | `55` | Panel width when world-locked |
| `Ortho3DSlabMetres` | `0.5` | Depth-slab thickness when world-locked. Too thin and meshes z-fight |
| `DumpDraws` | `0` | Draws to capture on the dump hotkey. One line per draw, eye 0 only |
| `DumpKey` | `0x78` | Virtual-key code that arms the dump (F9) |

---

## Phase 10: decoupled pitch

The right stick turns only. Its vertical axis is dropped, so the game camera
never pitches from the stick.

**In VR the headset already owns pitch** — you look up by looking up. Stick
pitch is then a second source for the same axis, fighting the first, and it is
the uncomfortable one: a vertical rotation the inner ear did not ask for is the
strongest simulator-sickness trigger there is, worse than yaw. Yaw is left alone
deliberately, because turning on the spot with the stick is how you play seated.

### It does take something away

This is not free, and the setting says so. The engine **aims** and reads its look
camera from the pitch being suppressed, so a shot lined up by tilting the stick
has to be lined up by tilting your head instead.

It ships on anyway, for two reasons: comfort is the right default in a headset,
and the chord below hands the stick back on demand, so nothing is actually out of
reach. `DecoupledPitch=0` restores the stock two-axis stick.

### Applied after the merge, not at the source

The suppression happens in the XInput detour **after** the physical-pad merge,
and that placement is load-bearing. Zeroing the axis back where the Touch state
is built would only cover the controllers — the merge takes whichever source is
larger, so a physical pad plugged in alongside would put stick pitch straight
back and the setting would appear simply not to work.

### The chord hands pitch back, and only ever that

Hold **RT + RB** and stick pitch returns for as long as both are held. It is a
release, never a trigger: with `DecoupledPitch=0` the stick already pitches and
the chord does nothing at all.

That one-directionality was arrived at the hard way, and both wrong versions are
instructive:

- **OR** made the chord dead weight for exactly the people who want it — anyone
  running `DecoupledPitch=1`, which is the default.
- **XOR** made the chord take pitch *away* when the setting was off, so the same
  gesture meant opposite things depending on a value you cannot see while
  playing.

A momentary control has to do one thing. The trigger threshold is `30`, which is
XInput's own `XINPUT_GAMEPAD_TRIGGER_THRESHOLD` rather than a number invented
here, and Shoot and Walk both keep working while the chord is held — nothing is
taken away to pay for it.

### Know what RB is on Touch

Touch has no physical shoulder buttons. As [Phase 5](#phase-5-vr-controller-support)
describes, the **right grip** synthesises `XB_X` for Walk. A physical Xbox
`RB` also satisfies this pitch-release chord; Touch does not need to emit
`XB_RIGHT_SHOULDER` for ordinary Walk. Photo Mode uses L3+R3.

So this chord is, physically, **right grip + right trigger** — which in play
reads as *walk and shoot*. That is a combination people genuinely use, on a
ledge especially, and it **will** engage the chord. `DecoupledPitchChord=0` if
you would rather walk-and-shoot leave pitch alone.

The binding line in the log states which way the pad is configured:

```
pad: move=Lstick look=Rstick(yaw only; hold RT+RB for pitch) jump=A(R lower) ...
```

### Swimming is the exception, and it is not a comfort call

Underwater, TR steers the swim with the **look** axis. Suppressing pitch does not
merely make aiming awkward — it removes the ability to dive or surface at all,
and the head cannot substitute, because **the head turns the view while the stick
turns Lara**. So the suppression is lifted automatically whenever Lara is in
water (`DecoupledPitchWaterOff`, on by default). TR4/TR5 read Lara's classic
`water_status`; TR6 reaches the same input policy through its own player
position and water-volume query, described below.

Finding out *when she is in water* in the TR4/TR5 engine took four attempts, and
the three failures are each instructive.

#### Attempt 1: the camera room's water flag — structurally wrong

Every room carries an underwater bit, and the culling already walks the room
array every frame (see [Phase 7](#phase-7-culling-fix)), so this looked free. Two
things had to be established.

**The flag is not where the classic layout puts it.** `flipped_room` is confirmed
at `0x68`, and classic TR4/TR5 put `unsigned short flags` immediately after it at
`0x6A` with bit 0 = `ROOM_UNDERWATER`. Here `0x6A` reads `0x0000` in every room.
Dumping `0x60..0x6F` on every room change found the real one two bytes further
along:

```
rooms  2, 15, 30 -> 0x40      rooms 13, 7 -> 0x20
room   6         -> 0x28      room  0     -> 0x60
room  32         -> 0x41      <- the water room, the only one with bit 0 set
```

So it is a byte at **`0x6C`**, bit 0 — later confirmed independently in the
disassembly, where `TEST byte ptr [room + 0x6C], 0x1` appears throughout both
game DLLs.

**But the question was wrong.** It worked underwater and failed at the surface,
and the log said exactly why:

```
camera room 30  flags@0x6C=0x40  water=no     <- during a surface swim
     water room 32  flags@0x6C=0x41           <- Lara, directly below
```

The camera trails behind and **above** Lara. Submerged, it is inside the water
room and the flag answers correctly; at the surface it sits in the **air** room
while she swims, and reports dry. No offset fix helps — the camera is simply not
where Lara is.

#### Attempt 2: patching the surface case geometrically — still wrong

Asking "is the camera standing over a water room, close above it?" — XZ footprint
containment plus a height limit, to exclude poolside walks and bridges. It did
not work either, and it was chasing a symptom: the camera's position is not
Lara's state, however carefully it is interrogated.

#### Attempt 3: `water_status` by disassembly — confidently wrong

The right signal is Lara's own `water_status` (`ABOVE_WATER 0, UNDERWATER 1,
SURFACE 2, FLYCHEAT 3, WADE 4`). Hunting it statically produced a plausible
trail: from the rooms-array global, the functions testing `room+0x6C` bit 0, then
writes of small immediates to a global, landing on a struct at `0x1804EE81x`
whose readers appeared to compare against 0, 1, 2 and 4.

**It read `0` forever.** The comparison scan looked a few instructions ahead and
was matching unrelated compares, and the field is written from *registers*, not
immediates — so the search could not have found it. A byte-window dump of that
region showed pointers and zeros: not the Lara struct at all.

#### What worked: a memory diff

Stop reasoning about it and measure it. Snapshot the whole writable data section
on dry land, diff while swimming for bytes that went `0 -> 1..4`, then read the
survivors back **in three states**:

| address | surface | underwater | land | |
|---|---|---|---|---|
| **`0x4EE74C`** | **2** | **1** | **0** | matches the enum exactly |
| `0x4EE720` | 4 | 4 | 0 | cannot tell surface from submerged |
| `0x623AE4` | 2 | 1 | 2 | never returns to 0 |
| `0x4B79B0` | 2 | 2 | 2 | constant |
| `0x1B6CA7` | 2 | 0 | 0 | wrong underwater |

One address out of 96 followed `SURFACE 2 → UNDERWATER 1 → ABOVE_WATER 0`.

**The third state is what made it conclusive.** With only surface and underwater
readings, `0x623AE4` looks just as good; it is the return to `0` on dry land that
eliminates it. Two data points would have shipped the wrong address for the third
time.

It also sits at `0x4EE74C` — *below* the fixed window dumped in attempt 3, which
is why that pass found nothing.

#### Porting to TR4

TR4's was derived rather than re-diffed, using TR5 as a template. In TR5 the
field has a distinctive signature: written **ten times, as a word**, by the one
function that also tests `room+0x6C` bit 0. Exactly one TR4 function matches —
ten word writes to `0x1804F2E4C` — and that global independently checks out with
**97 xrefs** (TR5's has 106) whose readers compare against 0, 1 and 4.

This is the same porting method the
[per-build address table](#phase-7-culling-fix) already uses across DLL builds,
and unlike attempt 3 it had a confirmed counterpart to match against — the
`laraWaterStatus` column lives in that same table.

#### Both game DLLs stay loaded

A bug worth recording, because it only appears once a second game has an address.
`tomb4.dll` and `tomb5.dll` are **both** mapped for the whole session — the room
hook installs in both. Resolving "the first module that has an address" would
therefore read **TR4's** global while TR5 is being played, reporting a water
state belonging to nobody. The lookup selects on `CurrentGame()` and caches per
game.

#### What is verified, and what is not

| | Status |
|---|---|
| TR5, build `0x696B499C` | **Measured** by diff, confirmed in play |
| TR4, build `0x696B4999` | **Derived** by signature match from TR5 |
| Both 2025-09-10 (HD pack) builds | **Unverified** — the documented `−0xC0` / `+0xF40` shifts |

All of these are reads, so a wrong address misbehaves rather than corrupts, and
the pad log prints the value it finds:

```
lara: water_status at tomb5.dll+0x4EE74C (build 0x696B499C)
pad: water_status=2 (SURFACE) -- stick pitch RESTORED for swimming
pad: water_status=0 (above water) -- stick pitch decoupled
```

`WADE` counts as water too, since wading also steers with the look axis.

### TR6 uses its own player position and water volumes

Angel of Darkness does not share TR4/TR5's engine or
`lara_info::water_status`, so the original reader returned "unknown" in TR6 and
the swimming exception never activated. The input policy was already correct;
TR6 needed a trustworthy native answer to "is Lara in water?"

The shipped `tomb6.dll` has no matching private PDB, but its embedded AMX native
registration table preserves useful names. It maps `IsPointInWater` to the
wrapper at `+0x32350`; that wrapper calls the native predicate at
`+0x14E180`. Disassembly shows the predicate reading `gmapGMXCur`, walking the
level's water-volume lists, and testing the supplied XYZ point against each
volume's inclusive bounds. It returns one for a hit and zero for dry space.

The same table maps `mapGetPlayerPosition` to `+0x14E880`. That function loads
the live player pointer from `tomb6.dll+0xCB42D8` and copies the position at
player offset `+0x40`. The TR6 reader joins those two independently named paths:

```text
tomb6.dll+0xCB42D8  -> live player
player+0x40         -> Lara XYZ
IsPointInWater(XYZ) -> dry / in water
```

This follows Lara instead of the third-person camera. That distinction remains
load-bearing at the surface: the camera can sit in air above a pool while Lara
is swimming, which is exactly why the first TR4/TR5 camera-room attempt failed.

`LaraWaterStatus()` selects this path when `CurrentGame()==2`, copies Lara's XYZ
to a local vector, and normalises the predicate to `0` for dry or `1` for in
water. The existing Gamepad policy then suppresses stick pitch on land, restores
it for the complete time Lara occupies a water volume, and suppresses it again
after she exits. No hook or game memory is patched for this feature.

All RVAs are gated to `tomb6.dll` timestamp `0x696B49A4`. The reader checks both
`gmapGMXCur` and the live player pointer before calling game code, so title
screens and level transitions return "unknown" safely. An absent module or an
unknown build also stands down instead of guessing.

A supported run reports the binding and dynamic transitions:

```text
tr6 water: bound Lara position and IsPointInWater (build 0x696B49A4, player +0xCB42D8, query +0x14E180)
pad: water_status=1 (IN WATER) -- stick pitch RESTORED for swimming
pad: water_status=0 (above water) -- stick pitch decoupled
```

No new INI option was added; this follows the existing
`DecoupledPitchWaterOff=1` default. Release compilation completed with zero
warnings and errors, all 219 address/layout checks passed, and the native
self-test passed with zero failures. In-headset testing confirmed that TR6
swimming now restores vertical stick control.

### Zoom is the same exception, for the same reason

Binoculars, and any weapon combined with a laser sight, hand the camera to a
dedicated zoom camera (`BinocularCamera_TR4`/`BinocularCamera_TR5`) that reads
the right stick's Y axis directly for vertical aim. With `DecoupledPitch=1`
suppressing that axis, holding the zoom got a scope that panned sideways and
never up or down — mechanically identical to the swimming bug, and missed for
the same reason: the head is not a substitute for the stick where the *stick*
is what the game reads.

Confirmed by decompiling `ProcessLooking`, the function that decides — every
frame — whether the right stick means "look around" or "hand off to the zoom
camera". It makes that call from exactly two fields:

```c
if (BinocularRange != 0)  { /* mid-transition, either direction */ }
if (BinocularOn < 0)      { /* leaving */ }
```

`BinocularRange` is a ramp counter, nonzero for the whole entering/leaving
transition; `BinocularOn` goes negative on the way out and (per `CalculateNewCamera`,
which sets it) settles to a steady nonzero value once fully zoomed in. So
`BinocularOn != 0 || BinocularRange != 0` is not an approximation of "currently
zoomed" — it is the game's own boundary, read rather than guessed, exposed as
`IsOpticsZoomed()` next to `LaraWaterStatus()` in `GameDll.cpp`.

`DecoupledPitchZoomOff` (on by default) hands stick pitch back for exactly the
same lifetime this reports true:

```
pad: optics zoom ENTERED -- stick pitch RESTORED for the zoom camera
pad: optics zoom left -- stick pitch decoupled
```

Two more symptoms were reported alongside the frozen pitch — the wrong vignette
(binocular instead of sniper scope) when zoomed with a laser-sighted rifle, and
the laser dot sitting off from where the shot actually goes — and both are
**confirmed fixed by the same change**, not a separate one. Neither was
investigated on its own; both are exactly what a frozen vertical aim looks like
one layer further downstream, the zoom camera's own state no longer tracking
where the head is actually looking the moment its stick input got zeroed, with
the overlay and the laser dot both drawn from that same stuck aim vector. One
root cause, three symptoms, confirmed in play after `DecoupledPitchZoomOff`
landed.

### Phase 10 settings

| Setting | Default | What it does |
|---|---|---|
| `DecoupledPitch` | `1` | Drop the right stick's vertical axis so only the headset pitches the view. `0` restores the stock two-axis stick |
| `DecoupledPitchChord` | `1` | Hold RT + RB to get stick pitch back while held. Never takes pitch away; does nothing when `DecoupledPitch=0` |
| `DecoupledPitchWaterOff` | `1` | Restore stick pitch automatically while Lara is in water. TR4/TR5 read her own `water_status` (`WADE` counts); TR6 asks its native water-volume query about her position |
| `DecoupledPitchZoomOff` | `1` | Restore stick pitch automatically while zoomed through binoculars or a laser-sighted weapon, read from the same `BinocularOn`/`BinocularRange` fields the game itself checks |

---

## Phase 11: ceiling clamp

In a low tunnel or a crawlspace the game camera already sits close to the
ceiling. Add positional tracking on top and leaning or sitting up pushes the eye
straight through it — you end up looking at the back faces of the level from
inside the rock.

Phase 11 caps how high the tracked head may rise.

> **Superseded in part by Phase 15.** Everything below about *what* the clamp does
> and why it only ever subtracts still holds, but the quantity it clamps moved.
> Capping tracking-space **height** is only the same thing as capping the eye's
> world height while the game camera has no pitch; it now clamps the world offset
> that is actually used, on the way out. The settings are unchanged.

### It clamps; it does not move you

The obvious fix is to drop the camera toward torso height when headroom is
short. That is rejected deliberately.

A clamp is **subtractive**: it can only ever remove motion you would have had,
and it never introduces any. Lowering the camera is **additive** — the world
slides under you without your asking, which is unrequested vertical motion, the
same class of thing that makes stick pitch uncomfortable. In a headset that
matters more than the tidier-looking result.

So the head rises until it nearly touches, and then stops. Everything below the
cap behaves exactly as before: ducking, leaning and every rotation pass through
untouched, and the whole feature is a no-op until you actually run out of room.

### Where the clamp is applied, and why it must be there

In `VRSystem::BeginFrame`, on the **raw pose, before it is inverted**:

```c
vr::HmdMatrix34_t pose = hmd.mDeviceToAbsoluteTracking;
...                                     // cap pose.m[1][3] here
m_headFromTracking = InvertRigid(FromHmd(pose));
```

That is the only point at which the head is still a plain **position**. After
`InvertRigid` it is a view transform whose translation column is `−Rᵀp`, so
clamping the Y there would move the eye **sideways** as well as down — the
rotation is mixed into every component.

The arithmetic is then direct. With a seated origin, pose Y is height above the
seated zero, and the eye sits exactly at the game camera when that is `0`, so the
camera rises by `poseY × WorldUnitsPerMetre`. Cap that at the room's headroom
less `CeilingMarginUnits` and the eye cannot pass the ceiling:

```
maxRise = (headroom − CeilingMarginUnits) / unitsPerMetre
```

Clamped to zero when it goes negative — that is the case where the camera is
already at or above the ceiling, and the right answer is "do not rise at all"
rather than a negative cap that would push you down.

### Where headroom comes from

The culling already reads the camera position and every room's bounding box each
frame, so headroom costs nothing extra. `list[0]` is the room the game camera is
in, and **TR's Y is down-positive**, which makes a room's `YMin` its *ceiling*:

(Phase 7B moved this into `GameDll.cpp` and made it read the camera's own room
on demand, so it no longer goes stale when the culling is switched off.)

```
headroom = camY − room.YMin
```

`CameraHeadroom()` publishes it, returning false before the first rendered frame
and in menus. Callers must read that as **unknown**, never as "no headroom" —
otherwise the clamp would pin the head to the floor on every menu screen.

### The bounding-box limitation, and why it is acceptable

Headroom is the room's **bounding box**, so it reports the highest ceiling
anywhere in that room. Two cases:

- **Uniformly low rooms** — tunnels, crawlspaces, ducts. The box is exact, and
  this is precisely where a head clips through in the first place.
- **A low alcove off a tall hall** — the box reports the hall, so the clamp
  simply does not engage.

That is **wrong in the safe direction**: it can fail to clamp, but it can never
clamp somewhere roomy. A false clamp in an open room would feel like an
invisible ceiling and be far worse than the artifact it prevents.

Getting it exact would mean calling the engine's own `GetCeiling` for the sector
under the camera, which costs a new per-build global in the
[address table](#phase-7-culling-fix) for four builds — worth doing only if the
box version turns out to miss real cases.

### Shared plumbing

Headroom rides on the same `GameDll` per-frame room read that
[Phase 10](#phase-10-decoupled-pitch)'s water detection uses, and resolves
through the same per-build address table as
[Phase 7](#phase-7-culling-fix). One consequence worth stating: on a game build
whose timestamp is not in that table, the culling fix, the swimming exception
*and* the ceiling clamp all go quiet together. They fail safe and say so in the
log rather than reading a wrong address.

It logs once, the first time it bites:

```
vr: ceiling clamp active -- headroom 892 units, head capped at 0.24 m above the seated zero
```

One-shot rather than per-frame, so a long crawl does not fill the log — which
does mean it reports *that* it engaged, not how often.

### The comfort cost, stated honestly

Holding the view still while your body keeps rising is itself a mismatch: your
inner ear says you moved and the image says you did not, and that reads as **the
ceiling receding from you** rather than as your head stopping. It is a real
effect and it is the reason an alternative exists — fading to black on
penetration instead, which moves nothing and simply stops you seeing through the
geometry.

That alternative is not in this build. The clamp is what is here, and the
trade-off is: no clipping through ceilings, at the price of a mild sense of the
world backing away when you sit up in a vent.

### Phase 11 settings

| Setting | Default | What it does |
|---|---|---|
| `CeilingClearance` | `1` | Cap the tracked head's height so the eye stays below the room's ceiling. `0` restores the uncapped head |
| `CeilingMarginUnits` | `128` | How close the eye may get to the ceiling, in world units — about 0.30 m at the default scale. Raise if you still clip through; lower if the cap arrives too early |

---

### Builds, and what happens to an unknown one

The address table now carries **only builds with a PDB**. The old table had a
second pair of rows derived by applying a uniform per-DLL shift to the newer
build's addresses, marked UNVERIFIED — a reasonable thing to do when the
alternative was nothing, and not reasonable now that the alternative is running
`pdbdump` against that build's own PDB.

An unrecognised build is named in the log along with the ones that are known, and
the culling, the ceiling clamp, the swimming exception and the sky fix all stand
down for it. That is the honest failure: every address would otherwise be a
guess, and one of them is a hook target.

### TR6

Not addressed. It is a different engine with different room structures, and it
has no row in the address table, so the culling simply does not install for it.
The same is true of the sky fix — its scene is rendered offscreen and composited
whole (see [Phase 6](#phase-6-tr6-support)) rather than duplicated per draw, so
the per-draw mechanism Phase 12 depends on does not reach it either.

---

## Phase 12: sky fix

**Symptom.** Outdoors, the sky is a painted sphere a few metres away. Distant
cliffs sit *behind* it, and leaning or IPD makes the dome slide. On a monitor
the same mesh is fine.

**Cause.** `DrawSkyHD` in `tomb4.dll` / `tomb5.dll` already does the
classic monitor-correct thing: it pushes a copy of the current matrix onto the
matrix stack and **zeros its translation** before drawing `hd_sky` through it.
Confirmed in the disassembly — the pushed copy's three translation floats are
overwritten with `0` right after the copy is made. The dome is centred on the
game camera, so there is no parallax from walking. That is optical infinity for
one viewpoint.

The stereo path then composes the per-eye transform — IPD plus any 6DOF head
offset — on top (`EyeOffsetMode=3`: `P' = P * E`, or one of the other modes'
equivalent). The dome's vertices still sit at a finite mesh radius, so that
translation gives them stereo disparity equal to a few metres, and the engine's
own depth range is still in effect, so the dome's fragments can win a depth
test against farther world geometry.

`tools\xrefs.py` against both DLLs reports a single caller:

```
DrawSkyHD            <- PrintRoomsList (x1)
```

One hook covers every sky triangle the remaster issues.

**Fix.** `src\Sky.cpp` hooks `DrawSkyHD` and sets a flag for the life of that
call. Every `validate_draw` that runs underneath it computes the per-eye
transform once and, when the flag is set, zeros its translation before using it
— which reaches whichever `EyeOffsetMode` is active, since all four route that
same transform into the view matrix, the model matrix or the projection.
Looking around still turns the sky; leaning and IPD do not. Every duplicated
`ogl_draw` / `ogl_drawVB` underneath it sets `glDepthRange(1, 1)` for that draw
and restores the engine's range afterwards, so the fragments land on the far
plane and cannot win a depth test against the world. `SkyAtInfinity=0` is the
stock behaviour exactly: the hook is not installed.

The stolen window is the same 5-byte PIC `mov [rsp+8], rbx` as
`S_GetObjectBounds`, and it is the same bytes in both DLLs:

| DLL | `DrawSkyHD` RVA | size |
|---|---|---|
| `tomb4.dll` | `0x000C4CA0` | 1207 |
| `tomb5.dll` | `0x000B97F0` | 1195 |

#### Where the code is

| file | what it holds |
|---|---|
| `src\Sky.cpp` | the `DrawSkyHD` hook and the in-sky flag |
| `src\Hooks.cpp` | the rotation-only eye transform at inject time; far-plane depth around the duplicated draw |
| `src\GameDll.cpp` | the two-row address table those RVAs live in |
| `tools\verify_addresses.py` | re-derives both RVAs and the prologue from the PDBs |

#### What to watch

```
sky: hooked tomb4.dll (Tomb Raider IV) -- sky draws use rotation-only eye transform (optical infinity) and far-plane depth
sky: first DrawSkyHD draw (shader N) -- rotation-only eye transform, far-plane depth
```

The health report's `sky=` count is per injected eye. Outdoors it should be
non-zero; `sky=0` forever with the "hooked" line present means `DrawSkyHD` is
not running (indoors, or a title screen).

The GPU verify sample skips sky draws on purpose — both eyes share a
translation-free transform there, so sampling one would report zero separation
and look like an injection failure.

#### Phase 12 settings

All in `[VR]`, documented in `TombRaiderVR.ini`.

| Setting | Default | What it does |
|---|---|---|
| `SkyAtInfinity` | `1` | The whole feature. `0` is stock finite-dome stereo, for A/B |

---

## Phase 13: laser sight fix

**Symptom.** Hold R3 to raise a scope with the laser sight combined, put the red
dot on something and shoot: the bullet lands about a **foot above** the dot.
Horizontally it is dead on. On a monitor the two agree exactly, which is the
whole point of the laser sight.

### The dot is not in the world

That was the assumption worth killing first, because it makes the bug look
impossible: if the dot and the impact were both world geometry rendered through
one view, no rendering error could separate them.

`DrawBinoculars` (`tomb5.dll` RVA `0x000C5540`) takes a `LaserSight` branch at
`+0x66`, and what it does there is build a **screen-space quad**:

```
rbx = target_mesh_ptr
sx  = (phd_scr_right  - phd_scr_left + 1) / (mesh[0x30] - mesh[0x48])
sy  = (phd_scr_bottom - phd_scr_top  + 1) / (mesh[0x02] - mesh[0x32])
for each vertex:
    raw_vbuf[i].x = v.x * sx + 160.0      ; the old 320x240 half-extents
    raw_vbuf[i].y = v.y * sy + 120.0
    raw_vbuf[i].z = 0                     ; <- no depth at all
```

`160.0` and `120.0` are literal floats in `.rdata` (`0x0019C200`, `0x0019C1D8`),
and `z` is written as zero. `tools\xrefs.py` places the whole thing in the 2D
pass:

```
DrawBinoculars       <- S_OutputPolyList (x1)
DrawNormalLaserSight <- DrawBinoculars (x1)
```

So the dot is a **screen-centre crosshair drawn in the overlay pass**, scaled to
fill the screen rect. It has no world position and never did.

Both DLLs carry the same code at different addresses — the sizes are identical to
the byte, which is the cross-check that says they are one source:

| | `tomb4.dll` | `tomb5.dll` | size |
|---|---|---|---|
| `DrawBinoculars` | `0x000D2270` | `0x000C5540` | 1412 |
| `DrawNormalLaserSight` | `0x000D1E40` | `0x000C5110` | 869 |
| `GetTargetOnLOS` | `0x00015860` | `0x000132F0` | 2385 / 3792 |

Nothing here is hooked or patched. The addresses are the evidence trail for the
diagnosis, not an interface the fix depends on — which is why this phase needs no
row in the address table and works on both games without knowing which is live.

### The bullet is

`BinocularCamera_TR5` (`0x000D7C20`) raycasts twice, and both calls take the same
two arguments:

```
0x000D84E1  lea  rdx, [camera+0x10]      ; camera.target
0x000D84EE  lea  rcx, [camera]           ; camera.pos
0x000D84F5  call GetTargetOnLOS          ; r8d = 1  -> this one fires
...
0x000D8502  lea  rdx, [camera+0x10]
0x000D850C  lea  rcx, [camera]
0x000D8513  call GetTargetOnLOS          ; r8d = 0  -> this one only looks
```

`GetTargetOnLOS` (`0x000132F0`) copies the target into a **local** at `rbp-0x31`
before handing it to `LOS`, so `camera.target` itself is never clipped — the hit
point exists only on that stack frame, where `TriggerRicochetSpark` and
`DoBloodSplat` consume it. The impact is real world geometry at the end of a ray
out of `camera.pos`, and nothing global remembers where it landed.

### Why VR pulls them apart

Both things are on **one line through `camera.pos`** — that is why they agree on a
monitor, where your eye is on that line by construction. `phd_LookAt` builds the
view from `camera.pos` toward `camera.target`, so the crosshair's screen centre
*is* the ray.

The mod draws the 2D layer on the world-locked panel at `HudDepthMetres` (4 m),
so the crosshair ends up 4 m along that line while the impact is at the wall.
**Two points on a line project to the same pixel only from an eye that is on the
line**, and a tracked head is displaced from the game camera. The error at the
target is

```
error = h * (D / Z - 1)
```

for a head offset `h`, target distance `D` and panel depth `Z`. At `h = 0.3 m`,
`D = 10 m`, `Z = 4 m` that is **0.45 m** — the reported foot and a half. It reads
as purely vertical because seated your lateral offset is nearly zero while your
height offset is not; the same session's log had the head 1.21 m above the seated
zero (`vr: ceiling clamp active`).

The mechanism is falsifiable from the ini alone, without a rebuild: raising
`HudDepthMetres` shrinks the error and lowering it doubles it, because `Z` is
right there in the formula.

### Fix: put the head centre back on the line

Not the depth. Matching depths would mean knowing where the shot landed — a sixth
game-DLL hook on `LOS`, filtered to the call whose `start` is `&camera` — and it
would drag the **whole 2D layer's depth** along with the raycast, so sweeping the
scope from a crate at 2 m to a corridor at 50 m would haul the overlay through
the entire vergence range every frame. Worse artefact than the bug.

Instead, `OpticsHeadAtCamera` holds the head **centre** at the game camera for
the life of the optic. Every point on the aim line then projects to one pixel at
**every distance**, and the mod never has to learn the hit point at all.

**The IPD stays, and that is the part that matters.** The crosshair sits at
`(0, 0, -Z)` in the camera's frame — on the aim axis for *any* `Z` — so with the
head centre on `camera.pos` the **midpoint between the eyes is on the aim line
even though neither pupil is**. The two eyes' errors are equal and opposite:
they cancel in the fused direction, leaving the dot floating slightly in front of
the wall rather than resting on it. A vergence artefact, not an aiming error, and
the shot goes where the dot points. So the world keeps every bit of its stereo
depth — this is not a mono switch, and it does not need to be.

It reuses the drop that already existed. `VRSystem::HeadView` and
`VRSystem::EyeView` both zero the head pose's three translation floats when
`PositionalTracking=0`; the flag is OR-ed into that same condition, and
`m_eyeFromHead` is deliberately left alone:

```cpp
Affine head = m_headFromTracking;
if (!c.positionalTracking || m_headAtCamera) {
    head.r[0][3] = head.r[1][3] = head.r[2][3] = 0.0f;
}
```

Which is why the fix could be **tested before it was written**: run the whole
session at `PositionalTracking=0`, raise the scope, and the dot lines up. The
shipped version is that behaviour narrowed to the zoom.

**Latched once per frame**, in `Detour_ogl_present` beside the other game-DLL
reads, gated on the `IsOpticsZoomed()` Phase 10 already added. Not polled per
draw, and that is not an optimisation: the world draws, the 2D panel, the video
panel and the culling frustum all read the eye transform within one frame and all
have to agree about where the head is. A value read out of game memory mid-frame
could flip between them and leave the panel in a different space from the
geometry behind it. The cost of latching at the frame boundary is that it trails
the game's own control phase by one frame — 1/60 s, on the way into a zoom ramp
that is tens of frames long.

Culling follows it deliberately. With the head back at the camera the engine's
own visible set is the correct one again, so the Phase 7B head frustum simply
stops adding rooms for as long as the optic is up.

### What it costs

Raising an optic **moves your viewpoint** to the game camera. That is unrequested
motion, which this mod otherwise refuses to do — see Phase 11, where the ceiling
clamp only ever *subtracts* motion for exactly this reason. Three things make it
acceptable rather than hypocritical: it is bounded by how far your head is from
the camera, it happens on a deliberate button press, and it is **inherent** —
putting your eye on the aim line means moving it there. Any fix that does not
move your eye has to match the crosshair's depth to the target instead, with the
vergence problem above.

`OpticsHeadAtCamera=0` restores the stock behaviour, misalignment included.

#### Where the code is

| file | what it holds |
|---|---|
| `src\VRSystem.h` | `SetHeadAtCamera` / `headAtCamera`, and why the IPD survives it |
| `src\VRSystem.cpp` | the two places the head translation is dropped (`HeadView`, `EyeView`) |
| `src\Hooks.cpp` | the once-per-frame latch in `Detour_ogl_present` |
| `src\GameDll.cpp` | `BinocularOn` / `BinocularRange`, which `IsOpticsZoomed()` reads |

#### What to watch

```
optics: head centre HELD AT THE GAME CAMERA -- laser sight and bullet agree at every distance (eyes keep their offsets either way, so world depth is unchanged)
optics: head centre released to the tracked pose -- laser sight and bullet diverge by head offset
```

One line each way per zoom, never per frame. If neither line ever appears while
the scope is up, the latch is not firing and any alignment on screen is
`PositionalTracking=0` still being set globally.

#### Phase 13 settings

All in `[VR]`, documented in `TombRaiderVR.ini`.

| Setting | Default | What it does |
|---|---|---|
| `OpticsHeadAtCamera` | `1` | The whole feature. `0` is the stock displaced head, for A/B |
| `HudDepthMetres` | `4.0` | Phase 3's panel depth. Not part of the fix, but it is the `Z` in the error formula — the knob that proves the diagnosis |

---

## Phase 14: hide vignettes

**Symptom.** Raise the binoculars or a scope and the overlay reads as a picture of
an overlay: a rectangle hanging in space a few metres away, with your real
peripheral vision wide open all around it. The laser sight also has a transparent
red pane sitting behind its dot.

**Cause.** Every one of them is flat, full-screen artwork. `DrawBinoculars`
scales the mesh to `phd_scr_left..right` / `top..bottom` and writes it into
`raw_vbuf` with `z = 0`, so each is a 2D quad — which Phase 3 then lands on the
world-locked panel at `HudDepthMetres` along with the rest of the 2D layer.

That is not a placement bug with a better answer available. A vignette imitates
**the edge of your vision**, and the headset already has one of those: its own
field stop. Any rectangle drawn inside it is a second, smaller, wrong one. There
is nowhere to put it that helps, so the only useful fix is to not draw it.

### What is in there

`DrawBinoculars` emits six separate things, and `tools\xrefs.py` confirms the
five functions are called from nowhere else in either DLL:

| piece | what it is | stubbed |
|---|---|---|
| `DrawNormalBinocs` | the binocular vignette | `HideBinocularOverlay` |
| `DrawVCIHeadset` | TR5's VCI visor overlay | `HideBinocularOverlay` |
| `DrawLabyrinthFishEye` | the Labyrinth fisheye | `HideBinocularOverlay` |
| `DrawNormalLaserSight` | the scope frame, and its reticle lines | `HideScopeOverlay` |
| `DoInfraRedQuad` | one untextured quad over the screen rect, vertex colour `0xFF5050FF` | `HideOpticsTint` |
| **the aiming dot** | a `DefaultSprites` sprite at the centre of the screen rect | **kept** |

**The dot is not part of any of them**, and that is the whole reason this is five
targeted stubs rather than one suppression of `DrawBinoculars`. It is drawn inline
at `DrawBinoculars+0x3A5`, after `DrawGameInfo`, gated on `LaserSightActive` and
coloured through `LaserSightCol` — so it survives all five, still turns green on a
target, and still pulses. Phase 13 spent its whole length getting that dot to
agree with the bullet; throwing it away here would have been absurd.

`DoInfraRedQuad` deserves its own note, because its name is a trap. It was left
alone on the first pass on the assumption that it only fired in infra-red mode.
The call site says otherwise:

```
0x000C58BA  cmp  LaserSight, 0
0x000C58C1  jne  0x000C58CE       -> call DoInfraRedQuad
```

It fires **whenever the laser sight is up**, and it is drawn before the dot. That
is the red pane. Stubbing it takes the VCI headset's infra-red tint with it, since
the engine draws both through that one function — the setting says so, and the
quad is a colour wash rather than a brightness boost, so nothing in a dark level
becomes harder to see.

### Fix: one byte, not a hook

All five are `void`, all five are called only from `DrawBinoculars`, and no caller
reads a return value. So each is switched off by writing **`0xC3` — `ret` — at its
entry**. No trampoline, no stolen instruction window, no detour to get right, and
undoing it is one byte back. The rest of that first instruction is left as dead
bytes that nothing branches into.

It holds itself to `InlineHook`'s standard anyway: the prologue is compared before
the write, so a wrong address **declines to patch and logs it** rather than
corrupting an instruction stream. Five bytes for four of them, seven for
`DoInfraRedQuad` — `sub rsp,0x28` is only four bytes, and a five-byte window would
end mid-instruction, which would mean the bytes had been read from somewhere other
than the function the PDB names.

```
  DrawNormalBinocs        48 89 5C 24 08         mov [rsp+8],    rbx
  DrawVCIHeadset          48 89 5C 24 10         mov [rsp+0x10], rbx
  DrawLabyrinthFishEye    48 89 5C 24 10         mov [rsp+0x10], rbx
  DrawNormalLaserSight    48 89 5C 24 10         mov [rsp+0x10], rbx
  DoInfraRedQuad          48 83 EC 28 45 33 C0   sub rsp,0x28 / xor r8d,r8d
```

Byte-identical between the two DLLs, which is the cross-check that says they are
one source:

| | `tomb4.dll` | `tomb5.dll` | size |
|---|---|---|---|
| `DrawNormalBinocs` | `0x000D1B00` | `0x000C4DD0` | 831 |
| `DrawVCIHeadset` | `0x000D17B0` | `0x000C4A80` | 845 |
| `DrawLabyrinthFishEye` | `0x000D1440` | `0x000C4710` | 878 |
| `DrawNormalLaserSight` | `0x000D1E40` | `0x000C5110` | 869 |
| `DoInfraRedQuad` | `0x000D21B0` | `0x000C5480` | 189 |

Patching is **per stub**, so one moved address costs its own overlay and not the
other four, and the bytes come back out on rebind — the player can switch between
TR4 and TR5 from the title screen, and a byte written into the old module has to
be restored before anything is written into the new one. `tools\verify_addresses.py`
re-derives all ten RVAs and both prologue windows from the PDBs: 219 checks, up
from 209.

#### Where the code is

| file | what it holds |
|---|---|
| `src\Overlay.cpp` | the stub table, the verified one-byte patch, and its removal |
| `src\Overlay.h` | which draw is which, and why the dot is not among them |
| `src\GameDll.cpp` | the five RVAs per DLL |
| `src\Hooks.cpp` | `OverlayUpdate()` per frame, `OverlayShutdown()` on unload |
| `tools\verify_addresses.py` | re-derives all of it from the PDBs |

#### What to watch

```
overlay: 5 optic overlay draw(s) stubbed in tomb5.dll (Tomb Raider V) -- binocular=1 scope=1 tint=1. The laser dot is a separate sprite and is untouched.
```

Once, when a supported build is bound. A `prologue mismatch` line instead names
the one overlay that was left alone rather than patched blind, and the other four
still go.

#### Phase 14 settings

All in `[VR]`, documented in `TombRaiderVR.ini`.

| Setting | Default | What it does |
|---|---|---|
| `HideBinocularOverlay` | `1` | The binocular vignette, the VCI visor, the Labyrinth fisheye |
| `HideScopeOverlay` | `1` | The scope frame. `0` brings its crosshair lines back while the binocular circles stay gone |
| `HideOpticsTint` | `1` | The red pane behind the dot. `0` brings it back, and the VCI infra-red tint with it |

All three `0` is stock behaviour and patches nothing at all.

---

## Phase 15: the stick stops displacing you

**Symptom.** Sit perfectly still and swing the look stick: your viewpoint travels
sideways, into walls the game's own camera collision thinks are clear. Pitch the
camera and a forward lean turns into rise and fall, through ceilings. Turning
`PositionalTracking=0` fixes it and costs you all of 6DOF.

**What was wanted**, and it is worth quoting because three attempts missed it:
*when `PositionalTracking=1`, the camera should behave like `PositionalTracking=1`
for HMD movement and like `PositionalTracking=0` for analog stick movement.*

### Cause

Composing `finalView = EyeView * gameView` puts the eye at

```
p_eye = camPos − R_g^T · R_e^T · t_e
```

`R_g` is the **game camera's** rotation. The head's offset is therefore carried in
a frame the game rotates, so rotating the camera **sweeps the eye on an arc of
radius |offset|** with the player motionless. 0.4 m of lean is 338 world units of
sideways travel over a 180° turn. `PositionalTracking=0` only appears to fix it by
making `t_e` zero — there is no arc when there is no radius.

### Fix: integrate the offset in world space

Keep the offset in world units and move it only by what the headset did:

```
offsetWorld += R_camera^T · (thisFrame − lastFrame)
p_eye        = camPos + offsetWorld
```

Rotate the camera and `thisFrame == lastFrame`, so the offset does not change, so
the eye does not move — `PositionalTracking=0`'s behaviour for anything the camera
does. Move your head and the delta is applied in the frame you are facing **at
that moment**, so full 6DOF is intact and leaning forward still goes into the
screen — `PositionalTracking=1`'s behaviour for the headset. Camera *translation*
still carries you along, because the offset is measured from `camPos`.

**Integrating the delta is the whole trick**, and it is what separates this from
the two designs that came before it. Freezing a reference frame gives the same
no-sweep property, but it leaves "forward" pointing wherever it pointed when the
anchor was set — and TR's camera is not yours. It is a third-person orbit camera
the engine re-aims constantly: following Lara through a turn, swinging in a
corridor, cutscenes, flybys. A frozen frame decays into "leaning does something
arbitrary" within a minute of play. Re-deriving the frame on every delta has
nothing to decay.

### Where it is done, and why there

In `VRSystem::WorldLockOffset`, on the **pose**, before it is inverted. That is
the only place the head is still a plain POSITION; after `InvertRigid` it is a
view transform whose translation column is `-R^T·p`, and editing that moves the
eye along two axes at once.

The conversion is cheaper than it looks, because **the pose's own rotation cancels
out**. Following the real code path for a pose `(R_p, P)`:

```
headFromTracking = (R_p^T, −R_p^T P)
ToEngineSpace conjugates by F = diag(1,−1,1) and scales the translation,
  so H = (F R_p^T F,  −s F R_p^T P)
the head's position in H's own space is  −R_h^T t_h  =  s F P
```

The rotation drops out, as it must — where the head *is* does not depend on where
it is *looking*. So the head's offset in engine view space is just its tracked
position, scaled, with the Y flip applied, plus one sign on Z to go from the mod's
−Z-forward eye space to phd view's +Z-forward. That identity and the forward/back
round trip were checked numerically against a transcription of
`FromHmd → InvertRigid → ToEngineSpace` over 500 random poses: errors of 9e-13 and
5.8e-15, i.e. float noise. The second number is the one that matters, because it
means the conversion is a bit-exact no-op when nothing needs changing, and it runs
every frame.

### What it measures out at

| test | result |
|---|---|
| stick yaws 180°, head still | eye travels **0.0 units** (camera-framed: 338) |
| lean 0.4 m forward, camera fixed | eye moves **169.2 units** = 0.4 × 423, 100% along camera forward |
| camera walks (700, 0, −300), head still | eye moves by exactly (700, 0, −300) |

### The cost, which is real

The offset is accumulated state, so it can drift from your physical centre. Lean
0.5 m out, stick-turn 90°, lean back to centre and the eye sits **0.71 m** from
the camera: the two legs cancelled in different world directions. That is inherent
to delta integration, not a tuning problem.

Two things bound it. `RecentreKey` (Numpad 5) re-anchors on demand, starting from
what the camera-framed behaviour would have given right then, so it never jumps
the view by more than the drift it removes. And a **camera jump over two sectors
re-anchors automatically** — level loads, cutscene cuts, flybys and teleports,
where a carried-over offset would be meaningless. Two sectors is far enough that
no walk or camera swing reaches it in one frame.

If a long session drifts far enough to matter, the next step is a cap on the
offset's magnitude, clamped on output so walking back recovers it. Deliberately
not built yet.

### The ceiling clamp, brought along

Phase 11 capped `pose.m[1][3]` — tracking-space **height** — which is only the
same quantity as the eye's world height while `R_g` has no pitch. It now clamps
the offset that is actually used, and does it **on the way out**, leaving the
integrated state alone, so walking into a taller room gives your real height
straight back. `CeilingClearance` and `CeilingMarginUnits` keep their names and
their meaning.

### TR6 follow-up: restoring the camera below ceilings and inside walls

TR6 later exposed two gaps in that work. The visible symptom was intermittent
but severe: the headset camera could sit above a room's ceiling or behind its
walls, with Lara no longer visible, even though the third-person chase camera
itself was in a valid position.

The first cause was not collision at all. Stereo obtains poses through
`IVRCompositor::WaitGetPoses`, and that call uses the compositor's tracking
space. `SeatedOrigin=1` had only been passed explicitly to the mono
`GetDeviceToAbsoluteTrackingPose` path. In the failing TR6 session the
compositor returned standing-space height, and the log showed about `+500`
world units — roughly 1.18 m at the configured scale — being added above the
chase camera before any physical head movement. TR6 stereo now selects the
configured seated or standing compositor space before `WaitGetPoses`. This is
gated to TR6 so the established TR4/TR5 pose path is unchanged.

The second cause was a mixed reference frame in the first TR6 ceiling attempt.
The world-offset conversion used TR6's rendered camera matrix while the GMX
room lookup used the separate legacy `gcamCamera` position. During TR6's
offscreen scene chain those values need not describe the same point, so the
clamp could choose the wrong overlapping room box; the diagnostic symptom was
the impossible-looking `headroom 0 units` result.

The clean path uses one camera for the complete calculation:

- `CameraViewFrame` reads TR6's column-major, `-Z`-forward rendered camera
  matrix, recovers its world position with `-R^T * t`, and converts the rotation
  to Phase 15's `+Z`-forward contract.
- `CameraHeadroom` tests that exact recovered position against the active GMX
  room boxes in all three dimensions. If boxes overlap, it takes the largest
  valid headroom, avoiding a false ceiling at a shared boundary.
- `WorldLockOffset` then clamps the actual world-Y eye displacement against
  that room ceiling. Camera pitch can no longer rotate a forward or sideways
  tracking offset into unbounded vertical displacement.

This restores the stable TR6 camera placement while preserving full headset
6DOF and stick-independent world locking. The pickup/candy-bar culling hooks and
the swimming pitch exception were not changed. The release build completed
with zero warnings or errors, all 219 address checks agreed with the PDBs, and
the self-test completed with zero failures. The deployed DLL for this fix had
SHA-256
`D84D6BF9A9E7C535FB0151395D6165E4D533AD4F1FC637404C56B624B99B62D1`.

#### Where the code is

| file | what it holds |
|---|---|
| `src\VRSystem.cpp` | `WorldLockOffset` — the integration, the re-anchors, the ceiling clamp on output, and TR6's configured compositor tracking space |
| `src\VRSystem.h` | the offset state and `RecentreOffset()` |
| `src\GameDll.cpp` | `CameraViewFrame` — TR4/TR5's `w2v_matrix` or TR6's rendered matrix; TR6 GMX room headroom uses the same recovered position |
| `src\Hooks.cpp` | `RecentreKey` in the tuning-key poll |

#### What to watch

```
vr: head offset re-anchored to the game camera
vr: ceiling clamp active -- headroom 892 units, eye held 764 units above the camera
tr6 camera: stereo tracking space set to seated as configured
tr6 camera: world frame bound at tomb6.dll+0x29DCC0; world-space head offset and ceiling clamp active
```

These are not per-frame diagnostics: re-anchoring logs once per press or camera
jump, the clamp logs the first time it bites, and the two TR6 camera lines each
appear once when that game establishes its configured frame.

#### Phase 15 settings

All in `[VR]`, documented in `TombRaiderVR.ini`.

| Setting | Default | What it does |
|---|---|---|
| `HeadOffsetFrame` | `world` | Where the head's displacement lives. `camera` is the old behaviour, for A/B in the same session |
| `RecentreKey` | `0x65` | Numpad 5. Re-anchor the offset to the game camera. `0` disables |
| `PositionalTracking` | `1` | Unchanged, and no longer the lever for this: at `1` the stick already behaves as though it were `0` |

---

## Phase 16: TR6 Native Stereo Support

Phase 6 made Angel of Darkness stereo-correct with AER, but its two eyes came
from different game frames. Phase 16 renders **both eyes from the same game
frame** by replaying TR6's complete render-only scene boundary. This is the
native stereo path now used by default, and it has been confirmed working in
the headset on the supported `tomb6.dll` build.

### Why TR4/TR5's native path still cannot be reused

TR4 and TR5 render most world draws directly into the target represented by
`FBO_default`. Their native path can change the viewport and issue each draw
twice, once into each half of the double-wide VR texture.

TR6 renders 98.8% of its measured world draws into engine-owned offscreen
textures instead. Its final composite samples those textures over their full
0–1 UV range. Splitting those individual draws between two halves either leaves
the final target with almost nothing to duplicate or corrupts the intermediate
textures that the composite expects to be whole. Native TR6 stereo therefore
has to sit **above the complete offscreen pipeline**, not inside each final draw
call.

### The PDB supplied the missing boundary

The new `tomb6.pdb` exposes names and exact RVAs for the scene pipeline:

| Symbol | RVA | PDB size | Role |
|---|---:|---:|---|
| `App_Render_Scene_PlanarReflection` | `0x001B0AC0` | 2,208 bytes | Builds and renders the planar-reflection pass |
| `App_Render_Scene_PostProcess_SpecialCameras` | `0x001B1360` | 724 bytes | Processes special-camera targets |
| `App_Render_Scene_Main` | `0x001B1640` | 1,624 bytes | Renders the main scene from prepared draw data |
| `App_Render_Scene` | `0x001B1CA0` | 2,192 bytes | Owns the complete scene, reflection, postprocess and final-composite sequence |

The decorated name `?App_Render_Scene@@YAXXZ` proves the hook signature is
`void App_Render_Scene()`. Disassembly shows one game-loop call at RVA
`0x000BD59A`, immediately followed by `sysEndScene` at `0x000BD59F`.
`App_Render_Scene` itself constructs its local draw state, calculates visible
render data, updates room/character/effect render data, renders planar
reflections, calls `App_Render_Scene_Main`, runs the special-camera postprocess,
flushes commands and performs the final scene extras. It is therefore a
self-contained render boundary rather than a function that only consumes a
one-shot queue prepared by simulation.

That placement is the crucial fact: invoking this function twice repeats
rendering, while input, physics, animation control, audio, the outer game loop,
`sysEndScene` and `ogl_present` still execute once.

### One game frame, two complete scene passes

The detour performs this sequence:

```text
TR6 game loop advances once
  App_Render_Scene detour
    left eye
      select eye 0 matrices
      clear and bind the single-eye scratch FBO
      run the original App_Render_Scene in full
      blit the finished composite to the left half of the VR target
    right eye
      select eye 1 matrices
      clear and reuse the same scratch FBO
      run the original App_Render_Scene in full
      blit the finished composite to the right half of the VR target
    restore the double-wide stereo FBO
  sysEndScene runs once
  ogl_present submits both halves together
```

Both eye images consequently use the same simulation state and current tracked
pose. They differ only through the eye selected by `g_currentEye`, which drives
the existing asymmetric HMD projection, IPD offset and 6DOF head transform.

### Reusing TR6's intermediate targets safely

The implementation does not clone or widen TR6's private render targets. Each
eye runs sequentially through the engine's original target layout. When the
left pass finishes, its final composite is copied into the double-wide VR
texture before the right pass begins; the right pass can then overwrite every
intermediate without touching the saved left image.

The final composite for each pass is redirected through `FBO_default` into the
single-eye scratch framebuffer already developed for AER. The scratch uses the
engine's render dimensions, while `BlitMonoToHalf` scales the result to the
runtime-recommended per-eye dimensions. The per-eye projection already carries
the headset aspect, so that rescale is intentional.

After the second blit, the detour restores `FBO_default` and the bound framebuffer
to the double-wide stereo target. Anything TR6 draws between the scene return
and `ogl_present`—including ordinary UI—continues through the established
per-draw stereo/HUD path. The scissor-test enable state is also restored because
the blits temporarily disable it.

### How the existing draw hooks change inside the replay

Two small gates make the full-scene replay compose correctly with the original
TR4/TR5 stereo machinery:

- `g_inNativeTr6Scene` makes `DuplicatePerEye` call each low-level draw only
  once. The whole scene function is already running per eye; duplicating each
  draw again would render four scene passes and overwrite the results.
- `validate_draw` accepts offscreen world passes while that flag is set. TR6's
  world matrices therefore receive the selected eye transform throughout the
  intermediate pipeline instead of only when the engine targets its logical
  backbuffer.

Outside `App_Render_Scene`, the flag is false and the existing TR4/TR5, menu,
HUD and FMV behavior is unchanged.

### Exact-build hook and automatic AER fallback

The new hook is installed dynamically after `tomb6.dll` is resident and
`gGame == 2`. The supported DLL has PE timestamp `0x696B49A4`; the supplied DLL
and the installed Steam DLL were also confirmed byte-identical. At
`App_Render_Scene` the installer requires this position-independent seven-byte
prologue:

```text
48 8B C4             mov rax, rsp
48 89 58 08          mov [rax+8], rbx
```

Both the timestamp and bytes are checked before anything is patched. A future
game build with a different timestamp, a moved function or a changed prologue
is left untouched. If `AlternateEyeGame6=1`, the established AER path then
engages automatically rather than risking an unknown address.

Native mode does not use `AlternateEyeMinOffscreen`. The real
`App_Render_Scene` call is its scope, so menus and FMVs need no heuristic.
The offscreen-draw threshold remains only for AER, where the entire coming game
frame must be assigned to one eye before any of it renders.

### Configuration, logs and performance

| Setting | Default | What it does |
|---|---|---|
| `NativeStereoGame6` | `1` | Run the complete TR6 render scene once per eye |
| `AlternateEyeGame6` | `1` | Fall back to AER if native stereo is disabled or its hook cannot be installed safely |
| `AlternateEyeMinOffscreen` | `50` | AER-only gameplay/menu threshold; ignored by native stereo |

An older installed INI does not need editing: a missing `NativeStereoGame6`
key inherits the compiled default of `1`. Set it explicitly to `0` for an A/B
test or to force the AER fallback.

Successful activation produces these one-shot log lines:

```text
hook[TR6 App_Render_Scene]: ... (stole 7, tramp ...)
tr6: native scene hook installed (tomb6.dll+0x1B1CA0)
tr6: native stereo active -- App_Render_Scene and its complete offscreen/postprocess chain now render once per eye
```

The periodic performance report distinguishes the cost from AER:

```text
perf: 72.4 fps rendered -- TR6 native stereo, two scene passes per frame
```

Native stereo roughly doubles TR6's scene-rendering work, but unlike AER it
does not deliberately halve the update frequency of each eye. If the system
maintains 90 game frames per second, both eyes receive 90 newly rendered images
from the same 90 simulation frames.

### Validation and code locations

The release build completed with zero compiler warnings and errors. The
installed `tomb6.dll` matched the symbolized input, its timestamp and hook bytes
were read directly from the installed file, and the existing address verifier
still passed all 219 checks. Final in-headset testing confirmed that TR6 native
stereo works correctly.

| File | What changed |
|---|---|
| `src\Hooks.cpp` | TR6 build detection, dynamic scene hook, two-eye replay, offscreen injection gate, draw-duplication guard and AER fallback selection |
| `src\Config.h` / `src\Config.cpp` | `NativeStereoGame6`, enabled by default |
| `TombRaiderVR.ini` | User-facing native/AER controls and exact behavior |
| `src\DefaultIni.h` | Regenerated embedded configuration for fresh installs |

This phase changes TR6's stereo renderer only. Its culling work is documented
separately in Phase 17; its ceiling, sky, optics and world-locked camera-offset
gaps remain separate work because TR6 still has no corresponding row in
`GameDll.cpp`.

---

## Phase 17: TR6 Culling

> **Superseded in part by [Phase 20](#phase-20-tr6-pickup-and-scene-object-retention).**
> The all-room output descriptors, far-plane extension and exact render-only
> bounds policy remain. The private second map pass, replacement room seeds,
> expanded room AABBs and tracked camera on the main `Calculate` call were
> removed because they disturbed TR6's object preparation and hid small
> stock-visible pickups.

TR6 native stereo exposed a second camera problem. Turning the headset away
from Angel of Darkness's third-person camera revealed blue holes where walls,
floors and other room geometry had vanished. Lara facing the missing geometry
did not help. Rotating the **right stick until the game camera was directly
behind Lara** made geometry in front of her return.

That distinction identified the owner of the visibility decision: this was not
Lara-facing logic and not a stereo-eye error. TR6 was still culling against the
third-person camera while the headset was rendering a different direction.
TR4 and TR5 do not use this code; their culling fix remains Phase 7/7B.

### Why the TR4/TR5 fix cannot be reused

TR4 and TR5 expose their room traversal through `PrintRoomsList` and use the
layouts in `GameDll.cpp` and `PortalCull.cpp`. TR6 is a different engine. Its
room selection, portal rectangles, render-run flags and final room drawing all
live in `tomb6.dll`, with different structures and call boundaries.

The supplied TR6 symbols made the relevant path identifiable:

| Symbol | RVA | PDB size | Role |
|---|---:|---:|---|
| `ClippedOBB_CPP` | `0x001A4380` | 645 bytes | Shared oriented-bounds test used by room runs, meshes, shadows and gameplay |
| `SYS_DRAW_CRP::Calculate` | `0x001A6DB0` | 4,303 bytes | Builds portal rectangles and the render CRP |
| `mapCalcVisibleRooms` | `0x001A8290` | 1,246 bytes | Builds the map-side linked list of visible rooms |
| `mapDrawRoomList` | `0x0014E370` | 1,009 bytes | Prepares room data after simulation |
| `ClipRoom_SYS_D3D_ROOM` | `0x001AF7F0` | 362 bytes | Sets visibility bits for the 0x40-byte render runs in one room |
| `App_Render_Scene_Main` | `0x001B1640` | 1,624 bytes | Consumes the completed CRP and draws rooms |
| `SYS_DRAW_CRP::CalculateCharacters` | `0x001A5E50` | 843 bytes | Builds the character draw pool and performs its own OBB reject |
| `SYS_DRAW_CRP::CalculateObjectsAnimated_DynamicLight` | `0x001A61A0` | 903 bytes | Builds dynamic-lit animated-object entries |
| `SYS_DRAW_CRP::CalculateObjectsAnimated_StaticLight` | `0x001A6530` | 862 bytes | Builds static-lit animated-object entries, including furniture props |
| `SYS_DRAW_CRP::CalculateWater` | `0x001A6A60` | 840 bytes | Builds water entries through three additional OBB call sites |

The associated layouts matter as much as the function names:

- `SYS_DRAW_CAMERA_VIEW` is 400 bytes: projection at `+0`, camera at `+64`,
  combined camera-projection at `+128`, viewport at `+192`, and its embedded
  `SYS_DRAW_CRP` at `+224`.
- Every `SYS_DRAW_ITEM_POOL` has a 24-byte header containing capacity, item
  count, needed count and data pointer.
- A `SYS_DRAW_CRP::ROOM_PORTALS_DESC` is eight bytes: room index, first portal
  rectangle and portal count.
- Both the CRP room pool and level room table have a verified maximum of 192.
- The current GMX holds its room-pointer table at `+0x1A0` and count at
  `+0x7A0`. `ROOM_HEADER_TAG::bIsFlipRoom` at `+0x218` identifies inactive
  alternate room copies that must not be submitted.
- Each room header stores its world AABB minimum at `+0xA0` and maximum at
  `+0xB0`. `Calculate` accepts an input seed only if the camera lies inside
  that AABB, even when the room pointer is already present in the input pool.

### What was tried, and what each result proved

The first attempt changed only `SYS_DRAW_CRP::Calculate`. For the two verified
render call sites it copied the 400-byte camera view, widened its projection to
contain both headset eyes plus `CullFovMarginDegrees`, pre-multiplied the game
camera by the tracked head transform, rebuilt the combined matrix, and passed
the copy to the original function. The render camera itself stayed untouched,
avoiding a second application of the head transform in the shaders.

The hook activated and logged a roughly 124 by 126 degree culling frustum, but
the blue holes remained. This proved that adjusting the final CRP calculation
alone did not reach the visibility decision responsible for most missing room
geometry.

The second attempt moved upstream. `mapCalcVisibleRooms` first ran normally,
then ran privately with the head camera. Its linked room list and all 192
`MAP_ROOMCLIP` records were saved, while the stock list and records were
restored before simulation continued. `mapDrawRoomList` temporarily exposed the
union only during render preparation and restored the globals afterward. AI,
triggers and gameplay therefore never observed the experimental head-visible
set.

The diagnostic result was decisive:

```text
tr6 cull: render preparation received 1 stock rooms + 0 head-only rooms (1 in the head list); simulation globals restored
```

The missing geometry was still present even though both traversals selected
the same single room. Whole-room selection was not the only culling tier.

Disassembly of `App_Render_Scene_Main` then exposed the next tier. It calls
`ClipRoom_SYS_D3D_ROOM`, which loops over 0x40-byte room runs and calls
`ClippedOBB_CPP` before setting each run's pass bit. A separate direct OBB test
rejects static meshes. Both still used the third-person camera. Returning "not
clipped" at only the verified room-run return address `0x001AF8AD` and static-
mesh return address `0x001B1814` helped somewhat, confirming this layer was
real, but many blue holes remained because absent rooms never reached it.

### The final headset-validated correctness-first path

The working path removes the game-camera dependency from room submission,
room-associated object-list construction and the final per-object tests during
the main TR6 render calculation:

```text
map/simulation visibility remains stock
  main SYS_DRAW_CRP::Calculate call
    replace its input seeds with every active, non-flip GMX room
    temporarily expand each seed AABB just enough to contain the tracked camera
    run the original Calculate with the wide tracked-head camera
    restore every room AABB before returning to the engine
    replace the calculated room descriptors with one per active room
    assign every descriptor the full-screen portal rectangle [-1,-1,+1,+1]
  App_Render_Scene_Main
    submit every active room
    keep room runs and static meshes at the two render-only OBB sites
    keep characters, animated objects and water at their render-list OBB sites
    let normal GPU depth clipping finish visibility
```

Seeding alone was insufficient. Before traversing portals, `Calculate` tests
whether the camera point lies inside each seed room's AABB; in the observed
level this reduced 46 active seeds to five calculated rooms. The final room
descriptor replacement made all 46 room shells draw, but the character,
animated-object, static-object and water pools had already been built from
those five rooms. This is why walls improved while cabinets and drawers could
still lose geometry when the right-stick camera rotated.

During only the main render `Calculate` call, Phase 17 now saves the six AABB
floats for every active seed and extends each axis only far enough to contain
the tracked camera plus a 1024-unit guard band. The original function can then
populate every room-associated object pool through its normal code. All saved
bounds are restored immediately after that call, before the game resumes any
other work. Replacing the resulting descriptors afterward still prevents
portal traversal from removing a room merely because the right-stick camera
cannot see its portal. The full-screen rectangle is the identity clip region
expected by `mapLoadClipMatrix`; it is not an arbitrary large coordinate.

Inactive flip-room copies remain excluded, so the active and alternate forms
of a room are not drawn together. Capacity and pointers are checked before the
pools are touched. If the verified 192-room capacity is unavailable, the
override declines instead of writing past the engine allocation.

The OBB detour is deliberately not global. `ClippedOBB_CPP` is also used by
gameplay and shadows. The room-run and static-mesh return addresses always
return visible while TR6 VR culling is active. The verified character,
animated-dynamic, animated-static and three water return addresses do so only
while the render-only CRP calculation is on the stack. Every other caller,
including normal simulation work, executes the original function.

### Distant geometry and the far plane

The remaining distant blue edge was a separate clipping tier. TR6's stock
perspective ends at 65,536 world units. Native stereo exposes wider, longer
sightlines, so the physical GPU far plane could cut distant geometry even
after every CPU room and object reject had been neutralized.

When TR6 native stereo and `PortalCulling` are active, Phase 17 automatically
uses a 262,144-unit far plane for both the conservative CPU culling projection
and the per-eye GPU projection. The near plane is unchanged. A user-provided
positive `FarClip` value remains authoritative, so the automatic extension
does not override explicit tuning. This code is inside the TR6-only scene and
culling gates and cannot change TR4 or TR5 projections.

### Scope, safety and TR4/TR5 isolation

The original Phase 17 implementation was TR6-only. Installation required all
of the following:

- `CurrentGame() == 2`;
- `PortalCulling=1` and an active tracked VR view;
- `tomb6.dll` PE timestamp `0x696B49A4`;
- the exact verified prologue bytes at `SYS_DRAW_CRP::Calculate`,
  `mapCalcVisibleRooms`, `mapDrawRoomList` and `ClippedOBB_CPP`.

That revision installed four culling hooks atomically. Phase 20 replaces the
group with the three render-only hooks documented there and stops patching both
map functions. The TR6 scene-replay hook retains its own independent guard and
AER fallback.

No Phase 17 address exists in `tomb4.dll` or `tomb5.dll`, and none of these
detours can run while `CurrentGame()` is TR4 or TR5. Their established
`PortalCull.cpp` implementation is unchanged.

### Logs, validation and cost

The original Phase 17 run reported the layers independently:

```text
tr6 cull: upstream visibility hooks installed (Calculate +0x1A6DB0, mapCalc +0x1A8290, drawRooms +0x14E370, room OBB +0x1A4380)
tr6 cull: SYS_DRAW_CRP::Calculate now follows the tracked head for the private room pass and render passes (... degrees, including margin)
tr6 cull: render-only static-mesh and room-run OBB rejection disabled; accepted rooms now retain all geometry for head look
tr6 cull: render-list character, animated-object and water OBB rejection disabled; props no longer follow the right-stick camera
tr6 cull: correctness-first room submission active -- ... map seeds, ... portal-visible rooms expanded to ... active rooms
tr6 cull: native-stereo far plane extended to 262144 world units
```

This is intentionally correctness-first. Submitting every active room costs
more CPU and GPU time than portal culling, and Phase 16 already renders TR6's
complete scene twice per frame. Hardware that was close to its frame-time limit
may need lower game render settings. Depth testing, near/far clipping and all
non-targeted object/shadow tests remain enabled. The far plane is extended, not
disabled, and an explicit `FarClip` setting can still replace it.

The release build completed with zero warnings and errors, the existing symbol
and address suite passed all 219 checks, and the deployed DLL matched the build
artifact at SHA-256
`3584C1B9B4A6AD994354183B7C079B608562BC5B2F876E768DB81689AF88B89B`.
Final in-headset testing confirmed that the complete path works correctly. The
early seed-AABB expansion fixes cabinets, drawers and other room-associated
geometry that previously disappeared when the right-stick camera rotated. The
render-only object OBB bypass prevents those populated entries from being
rejected again, and the 262,144-unit CPU/GPU far plane removes the remaining
distant blue holes. Room walls retain the improvement from the all-active-room
submission path.

All Phase 17 implementation is in `src\Hooks.cpp`. `PortalCulling` and
`CullFovMarginDegrees` retain their existing configuration entries; no new INI
setting was required.

---

## Phase 18: TR6 Projected-Shadow Fix

After native stereo and the Phase 17 culling path were working, Lara's shadow
still appeared twice in the headset. Its proportions were wrong, and it moved
with the view camera instead of remaining fixed to Lara and the world. Final
in-headset testing confirms that Phase 18 fixes all three symptoms while
keeping Lara's shadow enabled.

### Cause

TR6 uses a dedicated projected-character shadow pipeline rather than drawing
Lara's shadow as ordinary scene geometry:

```text
App_DrawChar_CalcProjectedShadows
  calculate light cameras and projection records for visible characters

App_DrawChar_DrawProjectedShadows
  switch to each character's light camera
  render character depth into the projected-shadow atlas

App_Render_Scene_Main
  project the completed atlas shadows onto room geometry
```

Phase 16 intentionally allowed stereo matrix injection on every offscreen world
draw inside `App_Render_Scene`, because TR6 renders its scene through several
intermediate targets. That broad rule also reached
`App_DrawChar_DrawProjectedShadows`. Those draws are not player-camera scene
draws: they render Lara from a **light camera** to construct the shadow map.
Applying the left- and right-eye headset transforms there produced two
differently positioned and distorted depth maps. The later room projection
then exposed that error as double vision, incorrect proportions and a shadow
that appeared to move with the camera.

The supplied PDB identifies the relevant functions and data directly:

| Symbol | RVA | PDB size | Role |
|---|---:|---:|---|
| `App_DrawChar_CalcProjectedShadows` | `0x001BCE30` | 3,393 bytes | Builds up to 32 `DRAW_CHAR_SHADOW_DATA` records and their light matrices |
| `App_DrawChar_DrawProjectedShadows` | `0x001B8270` | 1,266 bytes | Renders character depth from those light cameras into the shadow atlas |
| `App_DrawRoom_DrawProjectedDepthShadow` | `0x001B06E0` | 989 bytes | Projects the completed character shadows onto rooms |
| `draw_char_shadow_data` | `0x003A3980` | 10,752 bytes | Array of 32 336-byte projected-shadow records |

`DRAW_CHAR_SHADOW_DATA` contains a validity flag, score, shadow camera,
projection, combined camera-projection, depth projection, position/falloff
vectors, near/far values, FOV and shadow-tile ID. This confirmed that the
character-depth renderer owns a separate camera and must not inherit a headset
eye transform.

### Fix

Phase 18 installs an exact hook at
`App_DrawChar_DrawProjectedShadows` (`tomb6.dll+0x001B8270`). During that
function only, a render-thread scope flag tells `validate_draw` not to replace
the engine's projection or view matrix:

```text
left-eye App_Render_Scene
  projected-shadow depth pass -> original light camera, no eye injection
  main scene/shadow projection -> left-eye headset camera

right-eye App_Render_Scene
  projected-shadow depth pass -> same original light camera, no eye injection
  main scene/shadow projection -> right-eye headset camera
```

The light-camera shadow atlas is therefore stable and geometrically identical
for both scene passes. The scope ends as soon as character-depth rendering
returns, so `App_DrawRoom_DrawProjectedDepthShadow` and the rest of the main
scene continue receiving the correct per-eye view. Lara's shadow remains
enabled; no character or environmental shadow is deliberately removed.

The hook uses a seven-byte, instruction-aligned, position-independent prologue:

```text
4C 8B DC             mov r11,rsp
49 89 53 10          mov [r11+0x10],rdx
```

It is still guarded by the supported `tomb6.dll` timestamp `0x696B49A4` and
the runtime byte comparison used by the other TR6 hooks.

### Safety, isolation and fallback

The projected-shadow hook and `App_Render_Scene` hook are members of the atomic
TR6 native-stereo hook set. Phase 19 expands that set to include the effects
builder and its three visibility safeguards; `NativeTr6HookReady()` returns
true only when all six are installed. If any entry point fails its build or
byte checks, the complete set is removed and TR6 uses the existing AER
fallback. This prevents native stereo from running with either the known
distorted-shadow path or a partially installed effects fix.

The suppression flag is active only while all of these conditions are true:

- the current game is TR6;
- native TR6 stereo is active;
- execution is inside the duplicated `App_Render_Scene` boundary; and
- execution is inside `App_DrawChar_DrawProjectedShadows`.

TR4/TR5, ordinary TR6 scene geometry, room shadows, shadow projection and all
gameplay state are unchanged.

A successful run includes:

```text
hook[TR6 App_DrawChar_DrawProjectedShadows]: ... (stole 7, tramp ...)
tr6: native scene/shadow/effects hooks installed (scene +0x1B1CA0, projected shadows +0x1B8270, ...)
tr6 shadow: projected-character light-camera depth pass isolated from headset view injection
```

The release build completed with zero warnings and errors, and all 219 existing
symbol/address/layout checks passed. The deployed DLL matched the build
artifact at SHA-256
`84C84BE5850F9F3FEDA951130E790291C8E5D96725BE975C8C63763CAF909987`.
Headset validation confirmed that Lara's shadow now has correct stereo fusion,
proportions and world-locked motion.

All Phase 18 implementation is in `src\Hooks.cpp`; no new configuration entry
was required.

---

## Phase 19: TR6 Effects Visibility

After room, prop and far-plane culling were fixed, some TR6 effects still
followed the third-person camera rather than the view rendered in the headset.
Rotating the camera with the right stick made dust particles and the pools or
glows of light beneath street lamps disappear and reappear, even though the
effect remained visible from the tracked VR view.

This was not another Phase 17 room-geometry failure. TR6 has a separate
visibility pipeline for room-attached FX nodes. Walls and props could remain
submitted while an effect in the same room was independently marked
off-camera before its particle or lighting code ran.

### Traced cause

The supplied `tomb6.pdb` identifies the relevant path:

```text
mapDrawRoomList
  mathIsBoundsClippedAlt(FX-node bounds)
  convert the result's sign into bit 0x100 on tagMAP_FXNODE
  fxProcessBox(node)
    most dust/particle/light cases return immediately if bit 0x100 is set
    type-specific emitter-distance and local-light tests run only afterward
```

The critical instructions in `mapDrawRoomList` are:

```text
tomb6.dll+0x0014E540  call mathIsBoundsClippedAlt
tomb6.dll+0x0014E545  mov  edx,[rsi+0x8c]
                      ...
                      cmovns edx,ecx     ; clear bit 0x100 when visible
                      ...
tomb6.dll+0x0014E56B  call fxProcessBox
```

`mathIsBoundsClippedAlt` returns `-1` for a rejected node and `0` otherwise.
`mapDrawRoomList` turns that into the `0x100` flag. The many effect cases in
the 55,064-byte `fxProcessBox` function test that flag before doing any useful
work. This primary gate explains why changing billboard orientation, emitter
distance or the later lamp-light check alone produced no visible improvement:
those paths were never reached for a node that already carried `0x100`.

Two secondary camera dependencies can still cause popping after the primary
flag is cleared:

- `fxCamDist` measures effect distance from `gcamCamera.Position`. The chase
  camera position orbits Lara when the right stick rotates, so a stationary
  emitter can cross a distance threshold without Lara or the emitter moving.
- The local-light case calls `mathIsBoundsClipped` immediately before
  `fxInsertFXLight`, adding another third-person-camera frustum reject to such
  effects as street-lamp illumination.

The final render-data builder also has two camera sources. Most light-beam and
particle builders receive `SYS_DRAW_CAMERA_VIEW`, while
`fxParticleAddSquareCamFacing` reads `gcamCamera.Position` and `LookAt`
directly. Those sources must agree with the tracked view so surviving
billboards do not become edge-on or camera-bound.

| Symbol | RVA | PDB size | Phase 19 role |
|---|---:|---:|---|
| `fxCamDist` | `0x00107890` | 104 bytes | Distance gate used nine times by `fxProcessBox` |
| `fxProcessBox` | `0x00109020` | 55,064 bytes | Dispatches room-attached particle and lighting effects |
| `fxParticleAddSquareCamFacing` | `0x001961A0` | 611 bytes | Builds camera-facing particle quads from `gcamCamera` |
| `fxFillGeoBufParticleSystem` | `0x00196600` | 583 bytes | Builds both above-water and underwater particle buffers |
| `mathIsBoundsClipped` | `0x0019F8E0` | 163 bytes | Secondary bounds gate before `fxInsertFXLight` |
| `mathIsBoundsClippedAlt` | `0x0019FA30` | 138 bytes | Primary FX-node reject used by `mapDrawRoomList` |
| `App_DrawEffects_UpdateRenderData` | `0x001A28D0` | 2,122 bytes | Builds debris, particles, snow, gas and light-beam render data |
| `gcamCamera` | `0x002FD540` | 160 bytes | Legacy camera position and look-at data used by FX code |

### Fix

Phase 19 applies four coordinated safeguards:

1. At only the `mapDrawRoomList` return address `0x0014E545`, the
   `mathIsBoundsClippedAlt` detour returns `0`. The primary `0x100` off-camera
   flag therefore stays clear for room-attached effects prepared for TR6
   native stereo.
2. Calls to `fxCamDist` originating inside the verified `fxProcessBox` address
   range measure from `gcamCamera.LookAt`, the stable gameplay focal point,
   instead of the orbiting chase-camera position. This keeps particle-emitter
   range stable while Lara and the effect remain stationary.
3. At only the `fxProcessBox` return address `0x00115FDB`, the secondary
   `mathIsBoundsClipped` result before `fxInsertFXLight` is forced visible.
   Other bounds tests, including actor and gameplay tests, remain original.
4. During `App_DrawEffects_UpdateRenderData`, a private copy of
   `SYS_DRAW_CAMERA_VIEW` receives the tracked head-centre view. The matching
   tracked position/look-at direction is temporarily published through
   `gcamCamera` for the legacy billboard builder, then all eight original
   position/look-at floats are restored immediately after the builder returns.

The order is important: the primary FX-node flag must be cleared before the
secondary distance, local-light and render-data corrections can have any
effect. Head centre rather than an individual eye is used to generate common
billboard geometry; the ordinary native scene replay supplies the distinct
left- and right-eye projections afterward.

### Hook safety and TR4/TR5 isolation

All Phase 19 behavior requires `NativeTr6Active()`. Installation additionally
requires the supported `tomb6.dll` timestamp `0x696B49A4` and exact prologue
bytes at every target. The three small visibility functions have RIP-relative
instructions in their stolen prologues, so their trampoline displacement
fields are explicitly relocated:

```text
fxCamDist
  48 83 EC 28                         sub rsp,0x28
  F3 0F 10 0D A4 5C 1F 00            movss xmm1,[rip+gcamCamera]
  relocated disp32 offset: +8

mathIsBoundsClipped
  48 83 EC 48                         sub rsp,0x48
  48 8B 05 55 E7 0E 00               mov rax,[rip+__security_cookie]
  relocated disp32 offset: +7

mathIsBoundsClippedAlt
  48 83 EC 48                         sub rsp,0x48
  48 8B 05 05 E6 0E 00               mov rax,[rip+__security_cookie]
  relocated disp32 offset: +7
```

`NativeTr6HookReady()` now requires six hooks as one atomic TR6 native-stereo
set: `App_Render_Scene`, projected-shadow drawing, effects render-data update,
`fxCamDist`, `mathIsBoundsClipped` and `mathIsBoundsClippedAlt`. If any hook
fails its timestamp, byte, allocation or relocation check, all six are removed
and TR6 uses the existing AER fallback. A partial effects fix cannot remain
active.

The primary and secondary bounds overrides are filtered by exact return
address, and the distance override is filtered to callers inside
`fxProcessBox`. No global clipping function is disabled. None of these hooks
can run for TR4 or TR5, and their rendering, particles and existing Phase 7
culling path are unchanged. No new configuration entry is required.

### Logs and validation

A successful Phase 19 run includes:

```text
hook[TR6 fxCamDist]: ...
hook[TR6 mathIsBoundsClipped (FX light)]: ...
hook[TR6 mathIsBoundsClippedAlt (FX nodes)]: ...
tr6: native scene/shadow/effects hooks installed (scene +0x1B1CA0, projected shadows +0x1B8270, effects +0x1A28D0, FX range +0x107890, FX bounds +0x19F8E0, FX-node bounds +0x19FA30)
tr6 effects: primary mapDrawRoomList FX-node off-camera flag disabled for native stereo
tr6 effects: fxProcessBox emitter range now uses the stable camera target instead of the orbiting chase-camera position
tr6 effects: fxProcessBox local-light frustum reject disabled for native stereo
tr6 effects: particle billboards and light-beam render data now face the tracked head camera; game camera restored
```

The final release build completed with zero warnings and errors. Deployment
was verified by hashing the build artifact and installed DLL; both matched at
SHA-256
`5037064EA06E5351D453A09F29B461287030863D652007DCD472671719935E0B`.
Final in-headset validation confirmed that dust particles and street-lamp
lighting no longer disappear and reappear when the right-stick camera rotates.

All Phase 19 implementation is in `src\Hooks.cpp`.

---

## Phase 20: TR6 Pickup and Scene-Object Retention

Phase 17 stopped the third-person camera from opening blue holes in TR6 rooms,
but small independently rendered objects could still disappear. The chocolate
bar at the beginning of Parisian Backstreets was the repeatable test case;
Croft Manor barrels and similar compact props used the same path. Their room and
its larger geometry could remain visible while the individual object vanished.

The stock-visibility A/B was decisive. The candy bar existed and rendered with
TR6's normal visibility path, then disappeared when the broad Phase 17 room
override ran. Native stereo replay, textures and gameplay pickup state were
therefore not the cause. The object was being lost during CPU render
preparation or one of the final renderer's bounds tests.

### What the original room override disturbed

TR6 makes several independent decisions before a small object reaches a draw:

```text
mapCalcVisibleRooms
  prepares map and per-room object state
SYS_DRAW_CRP::Calculate
  builds fixed object pools
App_Render_Scene_Main
  final-object AABB test
    final-object OBB test
      draw the object or submesh
```

The old private second `mapCalcVisibleRooms` pass restored `gmapRoomList` and
the 192 `gmapRoomClip` records, but that function is not only a list builder.
Its nested calculation also changes object state outside those restored
globals. The main override then replaced and reordered room seeds, enlarged
every seed AABB to contain the tracked camera, and calculated object pools from
that fabricated spatial state. That could change which overlapping room owned
a pickup and hide an object that stock preparation retained.

There was also an ownership error in the first camera substitution.
`SYS_DRAW_CAMERA_VIEW` is a 400-byte object containing its `SYS_DRAW_CRP` at
`+0xE0`. Passing the original CRP as argument one while passing a stack copy of
its owner as the camera-view argument split one logical object across two
addresses. Input matrices came from the copy while output pool headers belonged
to the original.

Phase 20 removes that entire map-side experiment. `mapCalcVisibleRooms` and
`mapDrawRoomList` are no longer hooked. The main render `Calculate` call keeps
the stock camera, stock room pointers, stock seed order and stock room bounds.
Only after the engine has built its object pools does the mod replace the final
room descriptors with full-screen descriptors for the active seed count. Room
shells remain available for head look without lying to the object builder about
where the camera is.

The reflection `Calculate` path still needs a tracked camera. It now patches
only the three matrices in the real `SYS_DRAW_CAMERA_VIEW` owner and restores
them immediately after the call, preserving the CRP/owner pointer identity.

### The paired final-render tests

An object surviving pool construction is clipped twice more. The final renderer
calls the cheaper AABB helper at `tomb6.dll+0x001A4610` first and reaches
`ClippedOBB_CPP` only if that succeeds. An OBB-only bypass cannot help a candy
bar rejected by the earlier AABB test.

Five final-render paths use this AABB-then-OBB sequence:

| Path | AABB return RVA | OBB return RVA |
|---:|---:|---:|
| 1 | `0x001B0066` | `0x001B0081` |
| 2 | `0x001B0155` | `0x001B016B` |
| 3 | `0x001B0858` | `0x001B0877` |
| 4 | `0x001B09B8` | `0x001B0A03` |
| 5 | `0x001B1BFC` | `0x001B1C1D` |

At only those ten return addresses, and only while TR6 VR culling is active,
the two detours return "not clipped." Nearby calls belonging to other scene
views, gameplay, effects and shadows continue through the original shared
functions. Compile-time assertions keep both allowlists exact.

This creates a clean responsibility boundary: **stock logic decides which
objects exist; the VR hooks decide which prepared objects may be drawn.** The
main CRP builders retain their existing exact render-only OBB exceptions for
characters, animated objects and water, and the final paired exceptions keep
small pickups and props from being rejected a second time.

### Hook safety and validation

The production culling group now contains only three hooks:

- `SYS_DRAW_CRP::Calculate` at `0x001A6DB0`;
- `ClippedOBB_CPP` at `0x001A4380`;
- the final-object AABB helper at `0x001A4610`.

All three require the supported `tomb6.dll` timestamp and exact prologue bytes.
They install atomically; if any target fails validation, all three are removed
and stock visibility remains active. The removed map functions are not patched
at all, and TR4/TR5 cannot enter the TR6 detours.

A successful run reports the narrowed policy independently:

```text
tr6 cull: render-only visibility hooks installed (Calculate +0x1A6DB0, OBB +0x1A4380, object AABB +0x1A4610)
tr6 cull: stock map camera, room seeds and object preparation preserved; final room visibility expands after Calculate
tr6 cull: final scene-object AABB rejection disabled; small ground pickups now reach the oriented-bounds and draw stages
tr6 cull: final scene-object OBB rejection disabled; ground pickups and props remain visible to the tracked view
```

The clean implementation builds with zero warnings and errors, all 219 PDB and
address checks pass, and the native self-test reports zero failures. It requires
an in-headset pass at the opening Parisian Backstreets candy bar before the
clean reimplementation is considered revalidated. No new INI setting is
required; Phase 20 follows the existing TR6 `PortalCulling` gate.

All Phase 20 implementation is in `src\Hooks.cpp`.

---

## Phase 21: TR4/5 Physics Update

Angel of Darkness gives Lara secondary motion on her chest. Tomb Raider IV and V
have nothing equivalent: their Lara is a rigid 15-joint skeleton, so her upper
body moves as one piece no matter how she jumps or lands. Phase 21 brings TR6's
behaviour to TR4 and TR5 on the HD models. The chest bounces when she leaves the
ground and when she lands, and it stays still while she walks and turns. Only
the front of the chest moves; the back, backpack, shoulders and arms stay where
the animation puts them.

It could not be ported as code. It was reimplemented, and most of this phase is
about finding out, from the binaries, what TR4/TR5 actually give a physics
system to work with.

### What TR6 has, and why it cannot be copied

`6\DATA\CHAR\LARA_HD.CHR` stores its bone names inline. Lara's skeleton ends:

```text
... SPINE_1 SPINE_2 THORAX NECK HEAD
PONY1_DYNAMIC ... PONY10_DYNAMIC
SHLDER_L BICEP_L ... LITTLE3_R
JUG_L_DYNAMIC  JUG_R_DYNAMIC
```

The `_DYNAMIC` suffix hands a bone to TR6's generic spring solver:
`CharSkeleton::setup_springs_system` at `tomb6.dll+0x15E430`, then
`SpringSystem::update` (`+0x16E7E0`), `::collide` (`+0x171C10`), `::deflect`,
`::interpolate` and `::teleport`. So the chest is not a bespoke system: it is
two extra skeleton bones run through the same solver as the ponytail.

That solver works on `CharSkeleton` bone arrays fed by AOD asset data (per-outfit
spring setups and deflector groups), and TR4/TR5 have none of it. Their Lara is
still the classic hierarchy, with no bone to drive:

```text
0 HIPS  1 THIGH_R  2 CALF_R  3 FOOT_R  4 THIGH_L  5 CALF_L  6 FOOT_L
7 TORSO  8 UARM_R  9 LARM_R  10 HAND_R  11 UARM_L  12 LARM_L  13 HAND_L  14 HEAD
```

That order is confirmed from the engine rather than taken from community
knowledge. `SkinUseMatrix` (`tomb5.dll+0x00127528`) is 14 byte-pairs, and the
only four populated are `(1,2)`, `(4,5)`, `(8,9)` and `(11,12)`: the two knees
and two elbows, the places the skin blends between rigid meshes. The HD outfit
meshes (`4\ITEM\OUTFIT_*.TRM`) carry no bone names. The braid is no
counterexample either: `HairControl`, `HairAdvance` and `CalcHairMatrices` are a
separate CPU spring chain that draws its own meshes.

### Measuring what TR4/TR5 give us

The work started as a measurement harness. It changed nothing on screen and
answered three questions before any rendering was touched.

**Which draws are Lara.** `DrawLaraHD` is hooked for its scope, not its argument
(`tomb4.dll+0x000C40F0`, `tomb5.dll+0x000B8C50` on the stock DLLs, with the
retail rows carrying their own; 8 position-independent prologue bytes each,
different in the two DLLs). Inside it the renderer skins through
`uJoints[72*3]` (`mBoneMats` is 3456 bytes, 72 x 4x3). One scope issues several
skinned draws: the body uploads 15 joints, and attachments upload 33-slot
palettes, 13 slots of which hold one repeated filler matrix. The body is picked
by structure, as the palette whose joint origins are genuinely separate (more
than 12 units apart), on a world pass. The latch then stays locked to that
draw's shader. Without the lock it flipped between shaders 12 and 20 from frame
to frame, and differencing across a flip fabricated accelerations of up to 1.9
million units/s² against a gravity of 5400.

**What the matrices are.** They are skinning matrices, bone x inverse-bind, not
bone-to-world transforms. In the body palette, forearm to hand measured 2.0 units
apart and hips to torso 0.5, which is impossible for bone origins and exactly what
skinning matrices give when a child shares its parent's orientation. The shader
later confirmed it directly: one `aCoord` is multiplied by all three of a
vertex's joints.

**Whether the drive signal is clean.** `dup=0%` in every session: not one
rendered frame repeated the previous torso matrix. The remaster interpolates poses
per render frame (`CalcLaraMatricesHDAnim`, `HDAnims`), so there is no 30 Hz
stair-step for a spring to buzz on.

### The solver, and how it got there

The final solver drives a damped spring from Lara's own engine state. Every
earlier version was measured and replaced for a specific reason:

| Version | What it did | What the log showed | Replaced by |
|---|---|---|---|
| World-space position spring | a point mass chasing an anchor carried by TORSO | `disp_peak` pinned at the 18-unit clamp in every report; it lags by c·V/k, 160 units at running speed | acceleration drive |
| Acceleration drive, parent frame | `x'' = -k·x - c·x' - a_parent + g` | settled at 2.82 units against a predicted gravity/stiffness of 2.84, but turning threw the torso sideways | vertical constraint |
| Constrained to world vertical | acceleration, velocity and position projected onto world Y each step | turning fixed; walking still bounced, and short landings were lost in filtering (below) | engine-state drive |
| **Engine-state drive** | Lara's `gravity_status` and `fallspeed` | every jump paired with a landing (`jumps=14 lands=14`) | final |

The acceleration drive differentiated the torso matrix twice, then smoothed,
capped and deadzoned the result. Those filters compound on short events, and a
landing is one. Simulated through the real pipeline, a 16000 units/s² landing
spike reached the spring at **0** if it fell within one frame and at 5250 if it
spread across three. So whether a jump registered depended on frame timing.

The engine already knows when Lara is airborne, so the final drive reads it from
her `ITEM_INFO` through `lara_item` (`tomb4.dll+0x004F3000`,
`tomb5.dll+0x004EE900`), identical in both DLLs:

| Field | Offset | Source |
|---|---:|---|
| `fallspeed` (int16, per game tick) | `+36` | `typedump.py` |
| `gravity_status` (bit 3 of a uint32 bitfield) | `+6176` | dbghelp `TI_GET_BITPOSITION`, since three flags share that word |
| `pos.y_rot` (int16) | `+110` | `PHD_3DPOS` layout |

In the torso's frame the chest feels gravity minus the torso's own acceleration.
Standing, that is full gravity, so it sags. In freefall the torso falls with the
chest, so the physical answer is weightless: the chest rises about 15 units on
the way up. That reads as the chest moving as she jumps, which is not wanted, so
`DynamicBonesAirGravity` defaults to `1` and keeps full gravity in the air. The
chest then holds still for the whole jump and answers the landing only; `0`
restores the physical behaviour. Landing adds a
kick of `fallspeed x 30 x DynamicBonesLandImpulse`. The kick uses the **deepest**
fallspeed of the jump, because the engine zeroes `fallspeed` on the landing tick,
and reading it there would give every landing a kick of nothing. Walking never
sets `gravity_status`, so it cannot bounce.

The spring was retuned for visibility after the logs showed every jump
registering but nothing visible. At stiffness 1900 and damping 22 it was a 6.9 Hz
flutter that died in 0.11 s, during Lara's own landing crouch. Dropping to 4 Hz
made it readable, and the clamp rose from 18 to 40 with it. Damping then set how
many bounces a landing gives: at 0.12 of critical each bounce kept 46% of the
one before, which is three or four visible swings. The default is 0.19, one
large bounce and one clearly smaller. Simulated landing peaks after a running
jump, relative to rest:

| Stiffness / damping | Bounces | Each of the last |
|---|---|---:|
| 1900 / 22 (6.9 Hz, gone in 0.11 s) | 6.9 | -- |
| 630 / 6 (4 Hz, zeta 0.12) | 19.1, 8.8, 4.1 | 46% |
| **630 / 9.5 (4 Hz, zeta 0.19)** | **16.1, 4.6, 1.3** | **28%** |

### Moving only the chest

Displacing joint 7 made the solver visible, and in-headset testing confirmed the
timing, gravity direction, lag direction and height. It also moved the back,
backpack and shoulder seams, and no setting could stop that: a joint transform
moves every vertex attached to it by the same amount and cannot tell front from
back. Separating them needs per-vertex weights, which live in the vertex shader.

**Getting into the shader.** The engine builds each of its 202 programs with one
call from `init_ogl` to:

```text
void shader_init(Shader* shader, int fvf, const char* vs, const char* fs)   // tomb456.exe+0x00011820
```

The hook substitutes the `vs` argument and passes everything else through, so no
engine state is swapped or has to be restored. Every patched source is first
test-compiled in a throwaway shader object; if it fails, the engine gets the
original and that pass simply does not deform.

**Which shader.** Decoding all 202 `shader_init` calls to their sources shows two
skinning families. Lara's HD body (shaders 12, 20 and 21) uses the three-joint
index+weight family, shaders 9 to 23, built from 5 sources:

```glsl
vec4 coord = vec4(aCoord, 1.0);   // one bind-pose position, model space
vec4 j = aLight;                  // j.xyz = joint indices
vec4 w = aColor;                  // w.xyz = joint weights
p = sum over k of uJoints[j[k]] * coord * w[k]
```

The patch is inserted after the last joint's contribution to `p`, before `vFog`,
`vPos`, `vWorldPos` and `gl_Position` read it. It moves the vertex by the solved
offset, multiplied by its total weight on TORSO and by a chest-region weight
computed from `coord`. A shoulder vertex shared with an upper arm therefore moves
only partway. `validate_draw` binds `shaders[vid_state.shader].id` at `+0x49`, and
`ogl_draw` binds the mesh's VAO before calling `validate_draw`. The uniforms are
uploaded after `validate_draw` returns, before `glDrawElements`, when both the
program and the vertex layout are live. NPCs share these programs, so the offset
is cleared on any draw that is not Lara's body.

**Where the chest is.** It is measured, not assumed. The first body draw reads
its own vertex buffer back through the bound VAO and collects every vertex that
mostly follows TORSO: 4185 of 27349 on the measured outfit. Front and back are
told apart by comparing the torso's local +Z with Lara's facing from `y_rot`.
The height band is fitted to the bust itself: the mesh is sliced by height, each
slice records how far forward it reaches, and the bust is the bump in that
profile. On the measured mesh the profile read, neck to waist:

```text
16  22  36  49 | 68  75  75  75  68 | 55  55  55
                  ------ bust ------   belly
```

The band keeps every slice within `DynamicBonesChestBand` of the peak, then
reaches a little past the bump at both ends. On the measured mesh that is 0.37
to 0.86 of torso height: the collarbone above is excluded outright, and the
belly below only catches the tail of the fade. A depth ramp excludes the back
and backpack -- the weight starts 51 units behind the front surface and is full
25 units further forward -- and a lateral fade stops short of the armpits. The
offset is measured from rest, so the chest keeps its authored shape while she
stands still.

### Final tuning

Three things were wrong once the effect was working in-headset, and each came
out of the solver rather than by turning knobs until it looked right.

**A landing rang too many times.** Damping sets how much of each bounce carries
into the next: at 0.12 of critical that ratio is 46%, which is three or four
visible swings. The default is now 0.19, where the second bounce is 28% of the
first and the third is 1.3 units -- one large bounce and one clearly smaller.

**The chest moved on the way up.** In freefall the torso falls with the chest,
so the physical answer is weightless and the chest rises about 15 units as she
jumps. `DynamicBonesAirGravity` now defaults to `1`, holding full gravity in the
air: simulated motion for the whole ascent is 0.0, and the chest answers the
landing only. `0` restores the physical behaviour.

**Too little of the breast moved.** The depth start went from 0.5 to 0.6 of
torso depth, so the sides and underside move rather than only the front surface;
the lateral limit from 0.28 to 0.34 of torso width; and the height band gained
`DynamicBonesChestBand` plus a small margin past the bump. Measured against a
mesh rebuilt from the logged torso profile, that is 1.37x the vertices at full
weight and 1.43x the total moving mass.

Amplitude is then one multiplier, `DynamicBonesChestStrength`, tuned by eye to
`2.5`. The solver's own numbers are unaffected by it -- these are its output,
before the multiplier -- so the first bounce reaches:

| Jump | Solver | On screen at 2.5x |
|---|---:|---:|
| Small hop | 6.5 | 16 |
| Standing jump | 11.3 | 28 |
| Running jump | 16.1 | 40 |
| Long fall | 22.6 | 57 |

`DynamicBonesMaxDisplace` clamps the solver **before** that multiplier, so
raising the multiplier never moves where the clamp bites; only falls past about
fallspeed 220 reach it.

### What was tried, and what each result proved

- **`shader_init` hooked as `void(void)`.** The detour's own calls clobbered the
  argument registers before it called the original, which wrote through a junk
  `Shader*` and crashed to desktop at startup. The signature is now read from
  the function body, and the hook no longer swaps the engine's `glShaderSource`
  pointer.
- **The two-matrix skinning family patched.** 63 programs were patched and none
  of them was Lara's body, so nothing moved. The old whole-torso view had also
  stood down, because "shader path active" only asked whether anything was
  patched. It now stands down only after a body draw has actually gone through a
  patched program with a fitted region. A body drawn with an unpatched program is
  logged.
- **A fixed height band, 0.18 to 0.52 of the torso.** It moved the right area,
  but weakly. The measured front profile showed the bust peak several rows below
  the band, on its fade-out edge. The band is now fitted to the profile, and
  those fractions are only the fallback for an outfit with no distinct bump.
- **A GPU compile test that reported success while testing nothing.** Its C
  string extractor ended each literal at the first `;`, which sits inside the
  patch text, so it compiled the unmodified shaders. It was fixed and extended
  into a transform-feedback test that checks where vertices actually end up.

### Configuration

The feature ships **on**. `DynamicBones=0` switches the whole thing off, and
`DynamicBonesApply=0` keeps the solver running and measuring while drawing
nothing.

| Setting | Template | Meaning |
|---|---:|---|
| `DynamicBones` | `1` | master switch for the solver and measurement |
| `DynamicBonesApply` | `1` | the only setting that changes what is drawn |
| `DynamicBonesShader` | `1` | `1` chest only, per vertex; `0` whole TORSO joint |
| `DynamicBonesChestStrength` | `2.5` | chest path amplitude multiplier |
| `DynamicBonesDebugScale` | `1` | overall multiplier on **both** paths; stacks with `ChestStrength` |
| `DynamicBonesDriveMode` | `1` | `1` engine state; `0` differentiated acceleration (kept for comparison) |
| `DynamicBonesStiffness` / `Damping` | `630` / `9.5` | 4 Hz, damping ratio 0.19: one large bounce, then a smaller one |
| `DynamicBonesAirGravity` | `1` | `1` still while airborne, landing only; `0` weightless, the physical answer |
| `DynamicBonesLandImpulse` | `0.2` | landing kick per unit of fallspeed |
| `DynamicBonesMaxDisplace` | `40` | clamp, world units |
| `DynamicBonesForwardSign` | `0` | `0` auto from facing; `1`/`-1` forces front |
| `DynamicBonesChestWidth` / `ChestDepth` | `0.34` / `0.6` | lateral limit and depth start, fractions of the measured torso |
| `DynamicBonesChestBand` | `0.65` | how much of the bust the fitted height band keeps |
| `DynamicBonesRegionDebug` | `0` | pushes the selected region out by this many units, to see it standing still |

The full set, with the reasoning behind each value, is documented in
`TombRaiderVR.ini`.

### Logs

A working run on the measured outfit:

```text
boneskin: 15 skin program(s) matched, 15 patched, 0 fell back to the original
boneskin: measured buffer 4 -- 27349 vertices, 4185 mainly on joint 7 (coord 0x1406 x3, joints 0x1401 x4 norm 0, weights 0x1401 x4 norm 1, stride 24)
boneskin: front of the torso is local +Z (30 samples, mean alignment 1.00 with Lara's facing)
boneskin: live -- Lara's body (shader 21) now deforms per vertex, and the whole-torso view has stood down
dynbones: locked to shader 20 (15 joints) -- the latch stays here so the drive stays continuous
dynbones: engine drive -- jumps=7 lands=7  last landing fallspeed=100 (grounded now)
```

The fit also logs a side-view shape map of the torso, the front profile the band
came from, and how many vertices the region selects at full weight, so a wrong
fit shows in the log.

### Safety, scope and validation

- **TR4/TR5 only.** TR6 already has the real system and none of these hooks can
  run for it.
- **A build whose `shader_init` address is known** for the chest path. That was
  the stock build only, because the address came from its PDB;
  [Phase 22](#phase-22-retail-and-definitive-edition-builds) adds it to the
  retail and HD Definitive Patch rows. A row that lacks it leaves it at 0 and
  logs that the path is unavailable.
- **Degrades rather than breaks.** A patch that fails to compile ships the
  original shader. A shader path that never engages leaves the whole-torso view
  running. `DynamicBonesShader=0` forces that view.
- **Verified statically:** `tools\verify_addresses.py` checks every address and
  prologue this phase adds -- `DrawLaraHD` and `lara_item` in each DLL row,
  `shader_init` in the exe -- along with the rest of the table, 486 checks in
  all. The patched shaders compile and link on an
  RTX 3070 (NVIDIA 560.94) with both new uniforms active. In a transform-feedback
  test, chest vertices move by exactly the offset, seam vertices by their TORSO
  share, and back, arm and out-of-region vertices not at all.
- **Validated in-headset:** jump detection and landing pairing, gravity and lag
  direction, the per-vertex path bouncing the right area, the profile-fitted
  band, and the final tuning below -- reported as "pretty much perfect" in play.
  The back, backpack and shoulders staying still is established by the
  transform-feedback test above rather than by an explicit in-headset check.
  **Not yet validated in-headset:** the last amplitude step,
  `DynamicBonesChestStrength` 2.0 to 2.5, which is a pure scalar on motion that
  was already correct.

Implementation is in `src\DynamicBones.*` (solver, `DrawLaraHD` hook, body-draw
selection, engine-state drive) and `src\BoneSkin.*` (`shader_init` hook, shader
patch, vertex readback, region fit, per-draw uniforms). Supporting changes are in
`src\GameDll.*` (`lara_item`, `DrawLaraHD`), `src\Engine.h` (`shader_init`),
`src\GL.*` (uniform and readback entry points), `src\Hooks.cpp` (wiring, plus the
scope guard that restores the joint palette and uploads the uniforms on every
return path out of `validate_draw`), `src\Config.*`, `TombRaiderVR.ini` and
`tools\verify_addresses.py`.

---

## Phase 22: Retail and Definitive Edition Builds

Everything from Phase 1 to Phase 21 was reverse engineered against one set of
binaries: the v1.0.2a build dated 2026-01-17, which ships private PDBs. That is
what `tools\pdbdump.py` reads, what `tools\verify_addresses.py` checks against,
and what every address in `src\Engine.h` and `src\GameDll.cpp` came out of.

It is not what most people have installed. The retail Steam release is the
2025-09-10 build, and the HD Definitive Patch ships its own `tomb456.exe` dated
2025-07-01 on top of the retail DLLs. On either of those, the mod used to start,
hook the exe, and then stand down almost everywhere else:

```text
gamedll: tomb4.dll build 0x68C12FDA is unsupported; the culling fix is inactive for it.
tr6: build-specific scene/culling hooks unavailable -- tomb6.dll timestamp 0x68C12FE6 is not supported
```

The exe-side hooks survived because `Engine.h` already carried extra rows for
the community HD pack, and one of those rows turns out to *be* the retail exe.
Everything that lives in a game DLL — the culling fix, the sky fix, the
binocular and laser-sight stubs, the water query, and all of TR6 — was gated on
a single `tomb4.dll` / `tomb5.dll` / `tomb6.dll` timestamp and silently switched
itself off. Phase 22 makes every one of those address sets per build.

### The four binaries, and which build is which

| File | 2026-01-17 (PDBs) | 2025-09-10 (retail) | HD Definitive Patch |
|---|---|---|---|
| `tomb456.exe` | `0x696B49A7` | `0x68C12FEB` | `0x68639C21` (2025-07-01) |
| `tomb4.dll` | `0x696B4999` | `0x68C12FDA` | same as retail |
| `tomb5.dll` | `0x696B499C` | `0x68C12FE9` | same as retail |
| `tomb6.dll` | `0x696B49A4` | `0x68C12FE6` | same as retail |

The HD Definitive Patch replaces only the executable: its three DLLs are
byte-identical to the retail ones, so they need no rows of their own. Its exe
and the retail exe are the same code built twice — `.text`, `.data`, `.pdata`
and `.reloc` are byte-identical, and the differences are the PE debug
timestamps, the PDB age, and four bytes in an `.rdata` table the renderer's
texture init reads. That is why the two exe rows share one address list
(`TR_HD_ADDRS`) and differ only in the timestamp that selects them.

### Porting addresses without a PDB

The retail binaries have no symbols and no PDB, and their addresses do not
differ from the 2026-01-17 build by a constant — the two are the same source
compiled at different times, so functions moved by varying amounts and a few
changed shape. `tools\port_addresses.py` recovers the map structurally instead
of by hand:

1. Every function is taken from `.pdata` — the x64 unwind table, so the bounds
   are exact rather than guessed — and disassembled.
2. Each instruction is normalised with RIP-relative displacements and
   branch/call targets wildcarded, because those are the only bytes that move
   when surrounding code moves. Struct offsets, immediates and stack layout are
   kept, so a match also asserts that **the field offsets agree**.
3. Functions whose normalised stream is unique in both builds are paired
   outright. Pairs then propagate: the Nth call target of a paired function
   pairs with the Nth of its partner, and every RIP-relative operand casts a
   vote for a data-address pairing. Repeat until nothing new is learnt.
4. What is left is paired by nearest neighbour between two already-paired
   anchors, accepted only above a similarity threshold.

A code address maps by instruction index when its function pair is exact and by
aligned instruction (`difflib`) otherwise. A data address maps by vote, and the
tool prints the vote count and any dissent, so a thin result is visible rather
than merely plausible:

```text
python tools\port_addresses.py PDB\tomb4.dll retail\tomb4.dll 0x004F3000 0x000C40F0
# ref PDB\tomb4.dll stamp 0x696B4999  funcs 3081
# tgt retail\tomb4.dll stamp 0x68C12FDA  funcs 3087
# paired 2644 (1964 exact)
```

| Pairing | Functions, reference / target | Paired | Exact |
|---|---|---:|---:|
| `tomb4.dll` | 3081 / 3087 | 2644 | 1964 |
| `tomb5.dll` | 3148 / 3171 | 2712 | 1987 |
| `tomb6.dll` | 5399 / 5449 | 4377 | 2885 |
| `tomb456.exe` | 1359 / 1408 | 1154 | 887 |

93 addresses were carried across this way — 27 for `tomb4.dll`, 27 for
`tomb5.dll` and 39 for `tomb6.dll` — plus `shader_init` on the exe rows. Every
one was then re-checked individually; the tool locates a candidate, it does not
get the last word.

### What actually differs, DLL by DLL

**`tomb4.dll` and `tomb5.dll` are easy.** Every data address was agreed
unanimously by all of its references in matched code — 4 to 600 votes each, no
dissent — and the neighbour relationships survive intact: `phd_winxmax` and
`phd_winymax` still 4 apart, `lara_item` still `0x1C0` above `lara`,
`BinocularRange` still 8 above `BinocularOn`, the `outside_*` rect in the same
order. The struct layouts the hooks depend on — the 304-byte `ROOM_INFO`,
`lara+12`, `camera+4` and `+12`, `ITEM_INFO+6176` — appear unchanged in hundreds
of matched instructions and were never once remapped.

Three prologues did move, and all three for the same trivial reason: the retail
compiler chose a different home slot for `rbx`.

| Function | 2026-01-17 | Retail |
|---|---|---|
| `DrawSkyHD` | `48 89 5C 24 08` | `48 89 5C 24 18` |
| `DrawVCIHeadset` | `48 89 5C 24 10` | `48 89 5C 24 08` |
| `DrawLabyrinthFishEye` | `48 89 5C 24 10` | `48 89 5C 24 08` |

All are still five complete, position-independent bytes, so the expected bytes
simply travel with the row instead of being a shared constant: `GameDllLayout`
grew `drawSkyHDPrologue`, `drawVCIHeadsetPrologue` and
`drawLabyrinthFishEyePrologue`, exactly as `PrintRoomsList`'s already did. Those
same three functions had also changed shape too much to pair by body, so each is
placed by its unique call site instead — `PrintRoomsList`'s first call, and
`DrawBinoculars`' two calls that bracket `DrawNormalBinocs`, whose surrounding
call sequences agree one-for-one between the builds.

**`tomb6.dll` moved the most,** which is also where the mod has the most to
lose. Its row is a new `Tr6Layout` struct holding all 39 addresses, the
call-site allowlists, and the three prologues that embed a RIP-relative
displacement and so cannot be shared. Four findings are worth recording:

- **The globals are unanimous.** `gcamCamera` (41 references), the player
  pointer (281), `gmapGMXCur` (364) and the camera matrix (28) each agreed with
  no dissent. `fxCamDist`'s RIP-relative read resolves to the new `gcamCamera`,
  and both bounds helpers' cookie reads to the new `__security_cookie`.
- **`IsPointInWater` has no unwind entry,** so it is not a `.pdata` function and
  cannot be paired by body. It is identified the way it was originally: it is
  the sole callee of the AMX wrapper registered under the name
  `IsPointInWater`, and its body reads the new `gmapGMXCur` with the same
  `+0x7A0` / `+0x1A0` room offsets.
- **Return addresses were paired call site by call site** on the same callee, by
  address order and surrounding-code similarity. The two animated-object sites
  were then confirmed in the decompiler by their distinctive field offsets —
  `+0x3C4` / `+0x3D0` dynamic, `+0x3DC` static.
- **The final scene-object renderer was restructured.** Retail inlines the
  mesh-part loop into the character path (a new function at `+0x1B06A0`), so it
  has **six** paired AABB/OBB sites where the 2026-01-17 build has five:

  | Path | 2026-01-17 AABB / OBB | Retail AABB / OBB |
  |---|---|---|
  | mesh parts, joints < 4 | `1B0066` / `1B0081` | `1B0595` / `1B05AB` |
  | mesh parts, joints >= 4 | `1B0155` / `1B016B` | `1B0605` / `1B061B` |
  | characters | `1B0858` / `1B0877` | `1B0808` / `1B082D` |
  | mesh parts, inlined | *(reached by call)* | `1B08BD` / `1B08D9` |
  | static objects | `1B09B8` / `1B0A03` | `1B1062` / `1B10B1` |
  | main scene | `1B1BFC` / `1B1C1D` | `1B2289` / `1B22AA` |

  The inlined copy is the same test the other build reaches through its call, so
  it is allowlisted rather than treated as a new site.
  [Phase 20](#phase-20-tr6-pickup-and-scene-object-retention)'s rule is
  unchanged: the allowlists stay exact — `Tr6Layout::kMaxSceneObjectSites` is 8
  and unused slots are 0 — rather than disabling either shared bounds helper
  globally, and `static_assert`s still check that the nearby non-render call
  sites (`1B2732` and `13F5A6` on one build, `1B2DBD` and `13FAA7` on the other)
  stay out of both lists. Every TR6 struct offset the hooks use — `+0x40`,
  `+0xA0`, `+0xB0`, `+0xE0`, `+0x1A0`, `+0x218`, `+0x7A0`, the 400-byte camera
  view — is unchanged in every matched instruction that uses it.

### One address the exe was missing

`shader_init` (`tomb456.exe+0x00011B50` on both 2025 builds) is what
[Phase 21](#phase-21-tr45-physics-update)'s per-vertex chest path patches. It was
deliberately placed last in `Layout` so that a row initialising positionally and
stopping short of it came out `0`, which `BoneSkin` reads as "not available on
this build" — the mechanism that let the HD rows exist before the address was
known for them. It is now known, the rows carry it, and both 2025 exes embed the
same 315 GLSL sources as the 2026-01-17 build, so the shader patch applies to
all three unchanged.

With the chest path available everywhere, it is on by default: the template now
ships `DynamicBones=1` and `DynamicBonesApply=1`, with `DynamicBonesShader=1`
choosing the per-vertex path and the whole-torso view kept as the automatic
fallback. `DynamicBonesApply` is ignored while the shader path is live, because
doing both would move the chest twice.

### Selecting a build at runtime

Nothing is probed or pattern-matched at load. Each family looks up its row by PE
timestamp and either binds it or stands down, exactly as before — there are
simply more rows:

- `Engine.cpp` picks a `Layout` for `tomb456.exe` from `kBuildStock`,
  `kBuildHD1` and `kBuildHD2`.
- `GameDll.cpp` picks a `GameDllLayout` for the current game from four rows, two
  per DLL.
- `Tr6LayoutFor()` picks a `Tr6Layout` for `tomb6.dll` from two.

Every hook still verifies its exact prologue bytes before patching anything, so
a row that is somehow wrong fails as a logged `prologue mismatch` and a rollback
rather than a corrupted instruction stream. An unrecognised build now logs what
it does know, instead of one expected timestamp:

```text
tr6: build-specific scene/culling hooks unavailable -- tomb6.dll timestamp 0x12345678 is not
     supported; native stereo uses AER and culling stays stock. Known builds:
tr6:   0x696B49A4  tomb6.dll v1.0.2a debug (2026-01-17)
tr6:   0x68C12FE6  tomb6.dll retail (2025-09-10)
```

and a supported one names itself once, so a log says which binaries were bound:

```text
tr6: bound to tomb6.dll retail (2025-09-10) (build 0x68C12FE6)
```

The TR6 detours read their row through one `g_tr6` pointer, set together with
`g_tr6ModuleBase` before any TR6 hook is installed, so a detour that can rely on
the base can rely on the row. Both are cleared in `RemoveHooks`.

### Verifying a build that has no PDB

`tools\verify_addresses.py` could previously do one thing: re-derive the whole
address map from the PDBs and diff it. For the ported rows there is no PDB and
no name to compare against, so it checks the property that actually matters for
safety — that **each address carries the bytes the source says it does**, and
that the code around it has the shape the hook assumes:

- Every hook and stub target on a PDB-less build is checked against the exact
  prologue array the source will `memcmp` at install time, and the window is
  required to end on an instruction boundary.
- Every TR6 return address is checked to be preceded by a direct `E8` call **to
  the specific function its detour hooks** — which is what makes a
  return-address allowlist meaningful at all — and the AABB and OBB site counts
  are checked to match.
- `fxCamDist`'s RIP-relative read is resolved and checked to land on that row's
  `gcamCamera`; `IsPointInWater`'s first instruction is checked to read that
  row's `gmapGMXCur`; the FX light return is checked to fall inside
  `fxProcessBox`'s range.

The binaries are found by PE timestamp in `retail\` and `HD_Definitive_Patch\`.
Neither directory is part of the repository, so a missing one is reported and
skipped rather than failed:

```text
=== builds without a PDB (retail, HD_Definitive_Patch) ===
  skipped tomb6.dll 0x696B49A4 -- no binary of that build found
```

With every binary in place there is nothing to skip, and the run ends:

```text
OK -- all 486 checks agree with the PDBs / binaries.
```

### Scope and validation

- **Verified statically:** with the four retail binaries and the HD Definitive
  Patch exe present, `tools\verify_addresses.py` passes all 486 checks, every
  ported address and prologue included.
- **Three exe builds, two builds of each DLL.** A build outside those tables is
  not patched; it logs the builds it knows and falls back exactly as an
  unsupported build always has — AER for TR6 native stereo, stock culling, stock
  sky, no overlay stubs.
- **The HD Definitive Patch needs no DLL rows,** because its DLLs are the retail
  ones. If a future release of it changes them, they need their own rows, which
  is what `port_addresses.py` is for.
- **Validated in-headset:** both ported builds — the retail Steam release and
  the HD Definitive Patch — passed testing, so the ported rows are confirmed in
  play and not only against the binaries.

Implementation is in `src\GameDll.h` and `src\GameDll.cpp` (the two retail
`GameDllLayout` rows, the `Tr6Layout` table, `Tr6LayoutFor`,
`Tr6LogKnownBuilds`), `src\Hooks.cpp` (every TR6 constant replaced by a row
lookup through `g_tr6`), `src\Engine.h` (`shader_init` in the HD/retail rows),
`src\Sky.cpp` and `src\Overlay.cpp` (per-row prologues), `src\Config.h`,
`TombRaiderVR.ini` and `src\DefaultIni.h` (chest physics on by default), and
`tools\port_addresses.py` with the PDB-less section of
`tools\verify_addresses.py`.

---

## Phase 23: TR4/5 First Person

Status as of 2026-09-25: implemented for supported TR4/5 builds, with subsequent
regression fixes and diagnostic logging. The reported first-person
disappearing-room culling case is now confirmed fixed in headset. The reference
is the first-person implementation in `TombRaider123VR`; this is not a claim of
identical in-headset smoothness. Sideways wobble while moving forward, subtle
stick-turn judder and visible vertical gun aiming are still open. TR6 uses a
different engine and has **no first-person port**;
its existing stereo and third-person paths are unchanged by this feature.

### Enabling and using it

The shipped template has `FirstPerson=1` under `[VR]`. Each launch starts in
third person; press **Y + LT** to select first person, or press it again to
return to third person. **Y + RT** switches classic/remastered graphics while
the feature is enabled in TR4/5. The first-person chord is edge-triggered and
has the same ownership in gameplay and menus, so it does not toggle graphics
in a menu. Its action/trigger inputs are consumed to avoid leaking into gameplay.

The camera is anchored only during eligible gameplay. Inventory, title screens,
FMVs, cutsequences and fixed/cinematic cameras retain their own presentation.
Returning from a menu preserves the previous viewing heading when Lara is the
same item and has not been relocated. A new item or large relocation establishes
a fresh anchor instead of reusing stale positional state.

For an older installed INI, back it up and merge the first-person settings from
the repository template into its existing `[VR]` section. Do not replace unrelated
tuning just to obtain the new keys. If the INI was deleted, the updated DLL
generates a fresh one at launch. `FirstPerson=0` disables the port and restores
the legacy controller-chord behavior.

### Camera, movement and aiming

- **Animated eye anchor:** the camera uses the game's interpolated head joint
  (`GetJointAbsPositionLerp`, joint 14), with local eye offset `(0, -32, 144)`
  in game units. It does not reuse the third-person camera's height. Headset
  translation is relative to a captured neutral, not absolute standing height.
- **Directional ground movement:** forward, sidestep and continuous backward
  walking use native animation/action states. Backward uses `Back | Walk`, not
  the native back-hop or modern-controls run toward the camera. Native input
  heading, Lara's facing and movement angle are aligned before animation;
  residual native turn rate is cleared. Side/back horizontal animation movement
  uses the TR1–3 three-times scale once per animation tick; forward movement and
  jump motion are not multiplied.
- **Head/body alignment:** ordinary ground movement follows physical headset
  yaw plus artificial stick yaw. Body following is time-scaled using a 60 Hz
  reference. Native interactions, climbing, swimming, death and TR4 vehicles
  retain their own movement rules. Jump preparation and forward-jump steering
  have explicit handling rather than treating airborne movement as walking.
- **Physical-turn centering:** the neck-to-head compensation removes the
  duplicate horizontal eye arc that made physical rotation move the view off
  Lara's center and return after 360 degrees. Artificial turns also pivot the
  accumulated tracking offset. Actual leaning and ducking remain tracked.
- **Room-scale movement:** physical horizontal steps can move Lara through
  native wall, floor/ledge and room-transition checks. This moves her body
  directly rather than synthesizing stick input. Only accepted body motion
  actually visible at the current interpolation fraction is subtracted from
  pending headset displacement, avoiding double-counting. D-pad shifting and
  unsupported movement states do not perform this body drag; interaction
  transitions clear stale horizontal displacement.
- **Head aiming:** the native `AimWeapon` hook writes headset yaw/pitch into
  both arm controls before animation/firing, including the revolver's differing
  aim/fire arm paths. The arm lock prevents long guns from adding torso rotation
  twice. Horizontal head aiming works in headset, but the visible guns remain
  level when looking up or down: vertical visual aiming is a known defect. The
  mocked test proves the arm fields are written, not that the rendered gun pose
  or projectile path follows vertical gaze.
- **Visibility:** head, face attachments and braid are hidden while anchored
  when enabled. Rolls temporarily hide the whole body even if head hiding is
  disabled. Original mesh visibility is restored when leaving the mode.
- **Recenter:** End captures a new first-person positional neutral and clears
  room-scale counters without changing world viewing heading. The existing
  `RecentreKey` (Numpad 5 by default) uses the same first-person reset.

### Regression fixes after the initial port

The TR5 movement regression came from treating its `lara.Vehicle` struct slot
like TR4's. TR4 uses `-1` for no vehicle; TR5 leaves the unused slot zero and
its native above-water routine does not read it. The shared guard consequently
blocked TR5 ground locomotion, body turning and room-scale movement. The vehicle
check is now TR4-only, and the tests use each game's actual initialization value.

The first-to-third-person off-center camera came from carrying the absolute
headset position and stale third-person translation into the chase camera.
Third-person displacement no longer accumulates in the background while first
person is active. On exit, a fresh third-person positional neutral is captured
and translation is cleared for that same frame; subsequent physical leaning is
relative to the new neutral. Viewing rotation and stereo eye separation are
preserved. Tests cover explicit toggles, automatic camera/menu exits and tracking
reacquisition. This camera-handoff fix is deployed; final comfort validation
across both games remains an in-headset task.

TR4/5 room culling previously began its headset portal traversal in the room
seeded by the game's third-person camera. First person can put the eye across a
doorway or in a vertically stacked room while that seed stays behind. The
first-person traversal now asks the native floor/room lookup which room contains
the effective eye, including headset translation, and starts there. If that
lookup fails, it falls back to the engine's camera room. Third person still uses
the engine's seed; object culling and TR6 are unchanged. The fix builds and
passes the portal, first-person and address checks. The previously reported
whole-wall/room disappearance is confirmed fixed in headset; other room layouts
and both games still need broader live checks. A
`cull: first-person eye in room ...` log line records both room numbers when
they change.

The separate TR1–3 **wall-clipping fix** now protects the camera itself. While
Lara walks, jumps or falls, it sweeps from her interpolated collision origin
toward the final rendered eye, including horizontal physical head translation.
Native `GetCollisionInfo` checks at most 16 game units per step with a 64-unit
radius; a blocked step leaves the eye at the last clear point. Airborne checks
allow the floor to drop away without mistaking it for a wall. Hanging, ledge
pull-up, climbing and push/pull instead retract the forward eye offset to 16
units by default, without extending a custom anchor farther into an obstacle.
These changes affect only the active first-person scene camera, not Lara's
movement or the third-person chase camera. The new clearance passes mocked
TR4/5 tests and builds, but **has not yet been verified in a headset**. This is
distinct from the room-culling fix above: that fix keeps rooms drawn; this one
keeps the viewpoint out of nearby walls.

The 2026-09-25 Release/x64 DLL was installed for headset testing. The previous
installed DLL is preserved beside it as
`TombRaiderVR.dll.pre-fp-wall-clearance-20260925-160125`. The installed INI was
not overwritten; older INIs can omit `FirstPersonInteractionAnchorZ` and use
its built-in default of 16.

### First-person settings

All settings below belong in `[VR]`. These are repository/generated-template
defaults, not a promise that an older installed INI has been updated.

| Setting | Default | Purpose |
|---|---|---|
| `FirstPerson` | `1` | Enable the TR4/5 feature and new chords; startup remains third person |
| `FirstPersonJoint` | `14` | Animated head joint used for the eye anchor |
| `FirstPersonAnchorX` | `0` | Eye offset in joint-local game units |
| `FirstPersonAnchorY` | `-32` | Local vertical offset; negative is up |
| `FirstPersonAnchorZ` | `144` | Local forward eye offset |
| `FirstPersonInteractionAnchorZ` | `16` | Retract forward eye offset during constrained interactions; cannot extend a custom normal anchor |
| `FirstPersonHeadTranslation` | `1` | Physical leaning/ducking relative to neutral; tracked rotation remains active |
| `FirstPersonRoomscaleNeckMetres` | `0.15` | Neck-to-head compensation distance, clamped to 0–0.4 m; `0` disables it |
| `FirstPersonRoomscaleMove` | `1` | Let collision-checked physical steps move Lara |
| `FirstPersonRoomscaleDeadzoneMetres` | `0.02` | Lean allowance before the body follows |
| `FirstPersonRecenterKey` | `0x23` | End; recapture positional neutral while preserving viewing heading |
| `FirstPersonDriftLog` | `0` | Enable room-scale/heading and forward-motion diagnostics in `TombRaiderVR.log` |
| `FirstPersonHideHead` | `1` | Hide head/face/braid; roll body hiding applies independently |
| `FirstPersonMoveWithHead` | `1` | Convert modern-controls movement using the HMD viewing direction |
| `FirstPersonBodyFollowsHead` | `1` | Enable body following during supported ground states |
| `FirstPersonBodyDeadzoneDegrees` | `0` | Body-follow angular deadzone |
| `FirstPersonBodyTurnDegreesPerFrame` | `4` | Body-follow turn limit at a 60 Hz reference, scaled by elapsed time |
| `FirstPersonHeadAim` | `1` | Write headset yaw/pitch into gun-arm controls; visible vertical gun tilt is currently defective |
| `FirstPersonMotionGuns` | `0` | Experimental Quest Touch dual-pistol/Uzi tracking in remastered/HD graphics; off by default |
| `FirstPersonMotionGunGripForwardMetres` | `0.1778` | Mesh grip calibration; positive moves the gun back along its barrel axis, with the calibrated grip anchored to the controller |
| `FirstPersonMotionGunRaiseMetres` | `0.0254` | Raise the mesh in controller-local up while preserving the calibrated grip anchor; metres |
| `FirstPersonMotionGunRightMetres` | `0` | Controller-local lateral mesh offset; positive right |
| `FirstPersonMotionGunPitchDegrees` | `0` | Barrel angle correction; positive up |
| `FirstPersonMotionGunYawDegrees` | `0` | Barrel angle correction; positive right |
| `FirstPersonMotionGunRollDegrees` | `0` | Grip roll correction; positive clockwise |
| `FirstPersonMotionGunHotkeys` | `1` | Enable focused-game Ctrl+function-key live calibration |
| `FirstPersonTurnDegreesPerSecond` | `120` | Maximum continuous right-stick yaw rate |
| `FirstPersonTurnDeadzone` | `0.25` | Right-stick turning deadzone |

### Experimental Touch motion guns (TR4/TR5)

`FirstPersonMotionGuns=1` is an opt-in first-person test build for dual
pistols and Uzis with both Touch controllers tracked and Lara's guns ready.
It moves each arm/gun draw with its own controller, suppresses the rest of
Lara's body and hair, and replaces each native hitscan shot's origin and
direction at the firing-view call so the ray begins at that hand's muzzle.
The wrist is recovered from the renderer's skinning palette and inverse bind
pose before placing each arm. The palette translation alone is not the wrist:
using it as a pivot caused the hand and attached gun to orbit with wrist
rotation. Both native and HD mapped-bone paths now apply the correction to
the entire masked palette, including helper bones and blended vertices.
The barrel's local +Y axis follows controller forward; the separate native HD
muzzle offset supplies shot position without tilting the barrel toward the
wrist-to-tip vector. Lara's native muzzle-flash generation uses the same
tracked wrist frame. The pistol wall-impact LOS is redirected from the
tracked muzzle along the spread-adjusted controller ray.
Pistol arm aim is prevented from rotating the camera's head/torso anchor.
Native spread, ammunition and hit processing remain in place. The other guns,
classic graphics, third person, missing controller poses and TR6 retain the
existing behavior; `FirstPersonHeadAim` remains the fallback.

The earlier 0.30 m/0.173 m backward and 0.127 m upward compensations have been
removed. `FirstPersonMotionGunGripBackMetres` and
`FirstPersonMotionGunGripUpMetres` in existing INIs are ignored. No controller
direction is latched when drawing the guns; those compensations could drift
during physical turns and did not correct the underlying pivot.
After Quest testing confirmed the corrected pivot and aiming were almost
perfect but the hands remained about a foot forward, a separate local grip
calibration was added: `FirstPersonMotionGunGripForwardMetres=0.3048`.
This is an empirical 12-inch fit adjustment, not a measured mesh landmark.
The recovered wrist frame is translated so this local grip point maps onto
the tracked controller, and rotation takes place about that calibrated grip.
There is no headset-yaw offset, draw-time latch, or change to the bind-pose
recovery. The same adjusted frame drives the mesh, muzzle flash and shot
origin; barrel direction is unchanged. Actual hand fit still needs headset
confirmation. Adjust the value and restart; 0 restores the preceding build's
placement without reverting its pivot or aiming fixes (with raise also zero).
The latest requested fit is **1 inch up and 5 inches forward** relative to
that 12-inch-back test: grip-forward is now `0.1778` (7 inches back) and
raise is `0.0254` (1 inch up). These are controller-local mesh calibration
values, not a headset-facing offset. For manual tuning, decrease grip-forward
to move the guns forward and increase raise to move them up; one inch is
`0.0254` metres. INI edits take effect on restart, or use the live keys below.
Angle calibration rotates around the adjusted grip anchor and updates both
the rendered barrel and shot direction; muzzle flash and shot origin remain
on the same calibrated frame. With angles zero the previous fit is preserved.

#### Live gun calibration keys

Focus the game window, enter first person and draw pistols/Uzis in remastered
graphics with both controllers tracked. These keys adjust **both hands**:

| Keys | Position | With Shift also held |
| --- | --- | --- |
| Ctrl+F1 / Ctrl+F2 | Left / right | Yaw left / right |
| Ctrl+F3 / Ctrl+F4 | Down / up | Pitch down / up |
| Ctrl+F5 / Ctrl+F6 | Backward / forward | Roll left / right |
| Ctrl+F7 | Save current fit to INI | Restore last loaded/saved fit |

Tap once per step: **1/4 inch** for position, **1 degree** for angle. Key repeat
is suppressed. Changes are live but temporary until saved; restoring does not
overwrite the INI. Save preserves unrelated settings and makes
`TombRaiderVR.ini.motion-gun-calibration.bak` (previous backup is replaced).
Save/restore requests a system sound; errors and current values are written to
`TombRaiderVR.log`. Position is limited to +/-0.5 m, pitch/yaw to +/-90 degrees,
and roll to +/-180 degrees.

The mod consumes calibration function-key messages before native game actions,
and reserves Ctrl while calibration is available so Ctrl cannot also fire the
guns. Set `FirstPersonMotionGunHotkeys=0` and restart to restore native Ctrl
while using motion guns. Plain function keys, Alt chords, menus, third person
and background applications are not calibration targets. Key dispatch,
combined angle/position pivots, muzzle matching and INI save/reload/backup are
covered by automated tests; headset operation still requires in-game checking.
Regression tests now include nonzero bind offsets, both palette layouts,
scaled/non-orthogonal animation frames, left/right hands, full wrist rotations,
heading rotations, helper-bone blends, and matching rendered muzzle/shot positions.
The test fixture explicitly detects the previous pivot formula's failure.
It also checks the calibrated grip through full rotations, 1:1 controller
translation, zero-calibration compatibility, and shared muzzle placement.
Binary checks cover shot and flash hooks/call sites for all four supported DLLs.
These are synthetic and static checks, not an in-headset verification: grip
fit, physical 360-degree turns, flash placement and actual wall impacts still
need Quest testing. Smoke and shell effects are not controller-rebased.
If the mode is unusable, set `FirstPersonMotionGuns=0` and restart.
`TombRaiderVR.log` reports tracked arm draws and left/right shots in
five-second windows for diagnosis. LOS logs show the requested ray and return
value, not a measured wall impact (native collision uses a local endpoint).
There is no VRIK: the arms move as rigid
pieces and need not connect anatomically to Lara's hidden torso.

### Remaining motion issues and diagnostics

**Sideways wobble while moving forward is not yet resolved.** Enabling the
forward-motion diagnostics does not change controls or camera behavior. With
`FirstPersonDriftLog=1`, `fp-forward:` lines summarize roughly one-second windows
of eligible forward movement, measuring animated head offset (`animSide`),
per-sample lateral body/eye movement (`rootSideStep`, `eyeSideStep`), pending
tracked displacement (`trackedSide`), heading error (`bodyYawError`) and render
interpolation fraction (`frac`). Existing `locomotion:` lines report body-drag
and heading data. These separate candidate sources; animated head sway is a
hypothesis, not a confirmed diagnosis or a deployed stabilization fix.

For a capture, restart with logging enabled, enter first person, hold forward
in an open area for 10–15 seconds with the headset reasonably still and no
right-stick input, then quit. Preserve `TombRaiderVR.log` before another launch
overwrites it. Return `FirstPersonDriftLog` to `0` after collecting diagnostics;
the shipped template leaves it off.

**Subtle stick-turn judder is also unresolved.** Artificial yaw currently
advances at controller polls using elapsed time; it has not been moved to an
independent render-frame integrator or given an added smoothing filter. Recent
local logs reported approximately 59–60 rendered FPS. Poll/render cadence and
headset refresh/reprojection behavior are candidate explanations for the
difference from physical turning, not a measured root cause. No stick-judder
fix has been deployed. Input/render timing must be measured before changing
this path; a change also needs body-alignment, aiming, room-scale and camera
handoff regression coverage.

### Implementation and validation

The port lives in `src/FirstPerson.cpp`, `src/FirstPerson.h`,
`src/FirstPersonClearance.h` and `src/LocomotionMath.h`, with integration in `Gamepad`, `VRSystem`, `Hooks`,
`Config` and the per-build `GameDll` address tables. Camera replacement is
limited to the scene-camera call site. Required camera/aim/locomotion hooks
verify their prologues; installation failure rolls them back and leaves the
mode third person. Unsupported game builds are not guessed at.

`tools/first_person_tests.cpp` exercises the actual first-person, VR tracking
and controller implementations with mocked engine memory/native calls. Coverage
includes chords, directional gaits and jump handling, head aiming, physical
rotation and neck compensation, room-scale collision/interpolation accounting,
per-game movement guards, recentering, visibility restoration, camera handoff
and diagnostic sampling. Wall-clearance coverage includes walking, jump/fall
floor drops, blocked airborne eyes, interaction retraction and the third-person
guard. From an **x64 Visual Studio developer command prompt**
at the repository root (with the `build` directory present):

```bat
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\openvr\headers /Isrc tools\first_person_tests.cpp /Febuild\first_person_tests.exe /Fobuild\first_person_tests.obj /link /OPT:REF user32.lib
build\first_person_tests.exe
```

The latest run passed **14,736 checks**. These are synthetic checks, including
parameter sweeps, not 14,736 in-game scenarios or proof of headset smoothness.
The Release x64 build also succeeded. Native address/prologue/layout checks
were extended in `tools/verify_addresses.py`; the latest recorded run against
the available PDB/retail binaries passed **791 checks**. To include an installed
game directory in binary discovery:

```bat
python tools\verify_addresses.py "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
```

Live validation still needs both TR4 and TR5: forward/side/back movement,
physical and stick turns, leaning/room-scale collision, guns, jumps/rolls,
interactions, menus, graphics switching and first/third-person transitions.
The reported whole-room culling failure is fixed in headset, but other doorway
and stacked-room layouts need checks in both games. Vertical gun pose and shot
trajectory also need direct headset/gameplay validation. The new wall-clearance
path still needs in-headset walking, jump, fall, ledge and lean checks in both
games before calling it fixed in live play.
Do not label the port fully equivalent to TR1–3 based on compilation or these
tests alone.

---

## Known limits

These are honest gaps, not oversights.

- **TR4/5 first-person motion is still being refined.** Sideways forward-motion
  wobble and subtle stick-turn judder remain under investigation. Horizontal
  head aiming works, but visible guns do not tilt vertically with gaze. The
  reported disappearing-room culling case is fixed, though broader headset
  validation remains. TR6 first person is not implemented. See
  [Phase 23](#phase-23-tr45-first-person).
- **`Config.h`'s fallbacks are not the generated ini's values.** `EyeOffsetMode`
  falls back to the superseded `2` and `Mode` to `mono`; the generated
  `TombRaiderVR.ini` overrides both. Running with no ini and no way to write one
  gets the bring-up behaviour, not the playable one.
- **The frame graph is not fully traced.** The engine renders a post-processing
  chain into its own layered array textures before compositing. The stereo hooks
  only split work that targets the backbuffer, detected by reading the `ogl_rt`
  latch (`color_id == 0 && depth_id == 0`). Post passes that assume a
  full-target viewport may need per-pass handling. Phase 1 sidesteps this
  entirely by injecting on every world-space pass.
- **The 2D layer is one flat panel, not true world geometry.** Phase 3 places it
  on a quad fixed in the game camera's frame, which is stable and readable, but
  every 2D element lands on the same plane — the z ordering within the layer is
  discarded rather than depth-sorted.
- **Only one custom FBO exists in the engine** (`FBO_custom`), and
  `ogl_setRenderTarget` re-attaches colour and depth plus a full
  `glCheckFramebufferStatus` on every target change. We allocate our own FBO
  rather than fight it for attachments.
- **`glCheckFramebufferStatus`'s return value is discarded by the engine.** An
  incomplete FBO from a bad layer index fails silently, so a black screen is a
  plausible symptom of an unrelated mistake.
- **Depth has no stencil anywhere** (`GL_DEPTH_ATTACHMENT` only), so the eye
  target matches that.
- **Room 215 is expected to be fixed, and has not been confirmed.**
  [Phase 7B](#phase-7b-the-culling-fix-done-properly) narrows the frustum at
  every doorway, so a room that is connected but not visible through that
  doorway is no longer reached — which is precisely the mechanism
  [Phase 7](#phase-7-culling-fix) diagnosed behind the room-215 bug, and the
  reason `DrawAllRoomsExclude` is gone rather than carried forward. It is a
  prediction from the diagnosis, not a measurement. If 215 comes back, press
  `CullDumpKey` while it is on screen: the dump names the doorway it was reached
  through, and that would mean the room really is visible along some sightline
  and the cause lies elsewhere.
- **TR6 culling is correctness-first.** Phase 17 submits every active room to
  prevent the third-person camera from opening blue holes in the headset view.
  This costs more than portal culling, and current in-headset validation says it
  works substantially better rather than claiming every scene is perfect.
- **The controllers are a gamepad, not hands.** Phase 5 maps them to XInput;
  there is no motion aiming, no hand presence in-world, and no haptics.
- **TR6 native stereo roughly doubles the scene work.** It replays the whole
  render/postprocess chain for both eyes each game frame. This is confirmed
  working in-headset; the remaining limitation is performance on hardware that
  cannot sustain the requested VR frame rate. AER remains available as a
  half-rate fallback with `NativeStereoGame6=0`.

## Safety

`Bind()` sanity-checks the host module before anything is written — PE64, and a
`SizeOfImage` large enough to contain the 232 MB `.data` section our globals live
in — and the hooks verify the exact prologue bytes at every target before
patching. On a game update the bytes stop matching, every hook is rolled back,
and the DLL logs `prologue mismatch` instead of corrupting an instruction
stream. The shared engine hooks are installed all-or-nothing.

The TR6 scene and culling hooks are installed later, after `tomb6.dll` is
resident. The six-hook native scene/shadow/effects set has its own
build/prologue guard and selects AER if any member cannot be installed. Phase
20's three render-only culling hooks install atomically as a separate group: a
failure removes that complete group and leaves stock TR6 visibility active.

Stolen prologue bytes must be position-independent once copied to the
trampoline, and where they are not, the displacement is relocated rather than
assumed away. `InlineHook::Install` takes the byte offsets of any RIP-relative
`disp32` fields in the stolen bytes and shifts each by `target - trampoline`,
refusing the hook if a fixup would fall outside the stolen range or overflow
`int32`. This is not optional where it applies: a raw copy leaves the
displacement relative to the trampoline, which can sit up to 2 GB away, so an
instruction like `83 0D <disp32> 01` (`OR dword ptr [rip+disp32], 1`) would
corrupt an arbitrary address rather than merely misbehave. Phase 19 uses this
for `fxCamDist`, `mathIsBoundsClipped` and `mathIsBoundsClippedAlt`; their
verified displacement fields are relocated at offsets `+8`, `+7` and `+7`
respectively.

## Layout

```
src/              the mod: hooks, engine map, OpenVR glue, matrix maths
src/GameDll.*     binding and address table for tomb4.dll / tomb5.dll
src/FirstPerson.* TR4/5 first-person camera, locomotion, aiming and visibility
src/LocomotionMath.h first-person directional movement and neck-pivot maths
src/PortalCull.*  head-driven room culling, hooked into the game DLL
src/PortalGeom.h  the frustum maths behind it, tested by tests/
src/Sky.*         DrawSkyHD hook: sky draws at optical infinity
src/DynamicBones.* TR4/5 chest physics: DrawLaraHD hook, spring solver
src/BoneSkin.*   shader_init hook: per-vertex chest deformation
src/Callsite.*    return-address census, from before the DLLs had symbols
src/DefaultIni.h  the compiled-in ini template, written out when none exists
src/proxy/        the winmm shim that gets us loaded
PDB/              tomb456.exe + tomb4/tomb5.dll and their PDBs
tools/            PDB extraction, disassembly, address verification, vrprobe
tools/first_person_tests.cpp mocked-engine first-person regression harness
tests/            selftest (hooks + maths + frustum), proxytest (loader)
docs/             engine-map.html — the full renderer map
TombRaiderVR.ini  the ini template; src/DefaultIni.h is generated from it
trace.txt         the Ghidra session that produced src/Engine.h
```
