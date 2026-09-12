## Tomb Raider IV-VI Remastered VR Mod
VR mod for Tomb Raider IV-VI Remastered.  Tomb Raider IV: The Last Revelation and Tomb Raider V: Chronicles work in native stereo with 6DOF.  Tomb Raider VI: Angel of Darkness is not officially supported as it runs off an updated version of Core Engine and is considerably harder to mod.  AOD does work in VR but it only works with AER.  Performance is poor and there are lots of visual glitches.

## AI Usage
Claude Code was used heavily in the development of this mod.  AI was used to reverse engineer the game with Ghidra, explore strategies for porting the game to VR, and write code, and iterate on failures.  I used the AI to probe the game logic so I could debug the game in real-time and make architectural decisions when Claude was otherwise determined to make incorrect decisions.   

## VR Mod Features
* Native stereo (TR4/5 only)
* Culling fixes for VR
* Camera fixes for tight collision areas
* UI fixes
* FMV fixes
* Gamepad and VR controller support
* Dpad input support
* Decoupled pitch
* Sky fix — the HD sky dome sits at optical infinity instead of a few metres away

## Installation
[WIP]

## Controls
[WIP]

## Development Notes

**A VR mod for Tomb Raider IV–VI Remastered** (`tomb456.exe`, v1.0.2a,
2026-01-17 build), driving an OpenVR runtime.

The mod loads into the game, reads the head pose from an OpenVR runtime, and
composes it onto the game camera. It is built in two phases that share one
binary and one set of hooks — the phase boundary is a config switch, not a
branch.

| | | State |
|---|---|---|
| **Phase 1** | **Mono head tracking** — one image to the monitor, engine projection, nothing submitted to the compositor | The bring-up test. `Mode=mono` |
| **Phase 2** | **Native stereo with 6DOF** — per-eye matrices, double-wide target, positional tracking, compositor submit | Working. `Mode=stereo` |
| **Phase 3** | **UI fixes** — the flat 2D layer placed on a world-locked panel, and video cutscenes made fusable | Working. On by default in stereo |
| **Phase 4** | **FMV fixes** — cutscenes captured offscreen and replayed as real world geometry | Working. On by default in stereo |
| **Phase 5** | **VR controller support** — Touch controllers presented to the game as an Xbox pad | Working. On by default |
| **Phase 6** | **TR6 support** — alternate-eye rendering for Angel of Darkness, which renders its scene offscreen | Working. Auto-detected per game |
| **Phase 7** | **Culling fix** — the missing geometry behind Lara, fixed inside the game DLLs | Working. TR4 / TR5 |
| **Phase 8** | **D-pad input** — hold R3 and the left stick becomes a D-pad | Working. On by default |
| **Phase 9** | **Inventory fix** — items no longer stack, by preserving the engine's own projection shear | Working. On by default |
| **Phase 10** | **Decoupled pitch** — the headset owns pitch; the right stick turns only | Working. On by default |
| **Phase 11** | **Ceiling clamp** — caps the tracked head so it cannot rise through low ceilings | Working. On by default |
| **Phase 12** | **Sky fix** — the HD sky dome drawn at optical infinity instead of its mesh radius | Working. TR4 / TR5 |

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

**Phase 6** adds TR6. Angel of Darkness renders its scene into offscreen
textures and composites at the end, so per-draw duplication cannot reach it at
all; it gets alternate-eye rendering instead, selected automatically from which
game is running. See [Phase 6: TR6 support](#phase-6-tr6-support).

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

**There is no ini file in the repo to copy.** The configuration lives in the
DLL as a compiled-in template (`src/DefaultIni.h`), and the mod writes
`TombRaiderVR.ini` beside itself on first launch — ready to play, with
`Mode=stereo`, `EyeOffsetMode=3`, and every option from every phase documented
in place. The write is `CREATE_NEW`, so an existing file is never overwritten
and tuned settings are safe. To reset to defaults, delete the ini and relaunch.

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
| Action | Left upper face button | `Y` |
| Shoot | Right trigger | `RT` |
| Equip weapon | Left trigger | `LT` |
| Duck | Left grip | `LB` |
| Walk | **Right grip** | `X` + `RB` |
| Sprint | Left stick click | `L3` |
| System menu | Left lower face button | `BACK` (or `START`) |
| Photo mode | Left grip + right grip | `LB` + `RB` |
| D-pad | Right stick click + left stick | `DPAD_*` — see [Phase 8](#phase-8-d-pad-input-support) |

Two of those deliberately differ from the flat-screen defaults, because they
suit hands better than thumbs:

- **Walk is the right grip**, not a face button, so it can be held while the
  left thumb keeps moving. The game binds Walk to XInput `X`, so the grip emits
  `X` — and `RIGHT_SHOULDER` as well, so that the Photo Mode chord `LB` + `RB`
  still works. `X` and `RB` never collide in practice.
- **System is the left hand's lower face button.** Touch has no Start or Back of
  its own, and putting either on a chord made it awkward to reach mid-play.
  `GamepadMenuUsesBack=0` sends `START` (pause/inventory) instead —
  `inputUpdate()` decodes `BACK` to internal key `0x62` and `START` to `0x63`,
  and which one a given screen treats as "System" lives in the game DLLs, so it
  stays switchable rather than hard-coded.

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
pad: move=Lstick look=Rstick jump=A(R lower) roll=B(R upper) action=Y(L upper)
     system=BACK(L lower) walk=LS+RB(R grip) duck=LB(L grip) equip=LT shoot=RT
     sprint=L3 photo=LB+RB
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

Everything up to here was built against TR4 and TR5, which draw the world
straight to the backbuffer. **TR6 (Angel of Darkness) does not**, and that single
difference invalidates the technique the whole stereo path is built on.

Phase 6 adds a second rendering strategy for TR6 — **alternate-eye rendering**,
or AER — and the per-game detection to switch between them. Which game is
running is read from a global (`gGame`: `0` = TR4, `1` = TR5, `2` = TR6).

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

So instead of splitting each draw, **each frame is rendered entirely as one
eye** — mono, through the engine's normal path, at its own size — and blitted
into that eye's half of the stereo target. The other half keeps the frame it was
given. Nothing is resized and nothing is duplicated.

Both eyes are stereo-correct, because each frame carries its own eye's matrices.

**The cost is honest and unavoidable: each eye updates at half the frame rate** —
45 Hz out of 90. Full-rate stereo would mean running the entire offscreen chain
twice into parallel target sets, which is a substantially larger piece of work.
`AlternateEyeGame6=0` turns it off, and TR6 then renders flat at full rate.

#### The scratch target, and the flicker it prevents

The obvious implementation — render into one half of the double-wide target and
leave the other alone — fails immediately, and the reason is worth recording.
`FBO_default` *is* the double-wide target, so **any full-target `glClear` the
engine issues wipes both halves**. That reads as a hard flicker at half the frame
rate, with no head movement needed at all.

AER therefore points `FBO_default` at a **single-eye scratch framebuffer** sized
to the engine's own render resolution. The engine clears and renders into it
exactly as it would a normal backbuffer, and `ogl_present` blits the result into
one half of the eye target. The other half is never bound, so nothing can
disturb it. The blit rescales, which is correct — the per-eye projection already
carries the HMD's aspect.

If the scratch cannot be created, that is logged plainly rather than silently
degrading:

```
stereo: mono scratch unavailable -- TR6 alternate-eye will flicker,
        because the engine's clears reach both halves
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

Because AER halves the per-eye rate, the health report now measures wall-clock
frame rate over its 1800-frame window and says what each eye is actually
getting:

```
perf: 89.7 fps rendered -- alternate-eye, so each EYE updates at half that
```

That is the number that decides whether the judder is worth chasing, and it was
guesswork before.

### Phase 6 settings

| Setting | Default | What it does |
|---|---|---|
| `AlternateEyeGame6` | `1` | Alternate-eye rendering in TR6. `0` renders TR6 flat at full rate |
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
pad: move=Lstick look=Rstick jump=A(R lower) ... sprint=L3 photo=LB+RB dpad=R3+Lstick
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
describes, the **right grip** synthesises `XB_X` and `XB_RIGHT_SHOULDER`
together — `X` because that is what the game binds Walk to, `RIGHT_SHOULDER` so
the `LB`+`RB` Photo Mode chord stays reachable.

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
water (`DecoupledPitchWaterOff`, on by default).

Finding out *when she is in water* took four attempts, and the three failures are
each instructive.

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
| `DecoupledPitchWaterOff` | `1` | Restore stick pitch automatically while Lara is in water, read from her own `water_status`. `WADE` counts |
| `DecoupledPitchZoomOff` | `1` | Restore stick pitch automatically while zoomed through binoculars or a laser-sighted weapon, read from the same `BinocularOn`/`BinocularRange` fields the game itself checks |

---

## Phase 11: ceiling clamp

In a low tunnel or a crawlspace the game camera already sits close to the
ceiling. Add positional tracking on top and leaning or sitting up pushes the eye
straight through it — you end up looking at the back faces of the level from
inside the rock.

Phase 11 caps how high the tracked head may rise.

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

## Known limits

These are honest gaps, not oversights.

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
- **The culling fix is TR4 and TR5 only.** TR6 is a different engine and has no
  row in the address table, so Angel of Darkness still culls to the game camera.
- **The controllers are a gamepad, not hands.** Phase 5 maps them to XInput;
  there is no motion aiming, no hand presence in-world, and no haptics.
- **TR6 updates each eye at half the frame rate.** Alternate-eye rendering is a
  consequence of the engine compositing from offscreen targets, not a shortcut.
  Full-rate stereo would mean running the whole offscreen chain twice into
  parallel target sets — a substantially larger piece of work than anything in
  Phases 1–6.

## Safety

`Bind()` sanity-checks the host module before anything is written — PE64, and a
`SizeOfImage` large enough to contain the 232 MB `.data` section our globals live
in — and the hooks verify the exact prologue bytes at every target before
patching. On a game update the bytes stop matching, every hook is rolled back,
and the DLL logs `prologue mismatch` instead of corrupting an instruction
stream. Hooks are installed all-or-nothing.

Stolen prologue bytes must be position-independent once copied to the
trampoline, and where they are not, the displacement is relocated rather than
assumed away. `InlineHook::Install` takes the byte offsets of any RIP-relative
`disp32` fields in the stolen bytes and shifts each by `target - trampoline`,
refusing the hook if a fixup would fall outside the stolen range or overflow
`int32`. This is not optional where it applies: a raw copy leaves the
displacement relative to the trampoline, which can sit up to 2 GB away, so an
instruction like `83 0D <disp32> 01` (`OR dword ptr [rip+disp32], 1`) would
corrupt an arbitrary address rather than merely misbehave. None of the five
permanent hooks currently need it — it was added for temporary hooks used during
the culling investigation, and it is there for the next target that does.

## Layout

```
src/              the mod: hooks, engine map, OpenVR glue, matrix maths
src/GameDll.*     binding and address table for tomb4.dll / tomb5.dll
src/PortalCull.*  head-driven room culling, hooked into the game DLL
src/PortalGeom.h  the frustum maths behind it, tested by tests/
src/Sky.*         DrawSkyHD hook: sky draws at optical infinity
src/Callsite.*    return-address census, from before the DLLs had symbols
src/DefaultIni.h  the compiled-in ini template, written out when none exists
src/proxy/        the winmm shim that gets us loaded
PDB/              tomb456.exe + tomb4/tomb5.dll and their PDBs
tools/            PDB extraction, disassembly, address verification, vrprobe
tests/            selftest (hooks + maths + frustum), proxytest (loader)
docs/             engine-map.html — the full renderer map
TombRaiderVR.ini  the ini template; src/DefaultIni.h is generated from it
trace.txt         the Ghidra session that produced src/Engine.h
```
