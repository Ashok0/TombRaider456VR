# TombRaiderVR

**Phase 1: a head-tracking test** for **Tomb Raider IV–VI Remastered**
(`tomb456.exe`, v1.0.2a, 2026-01-17 build).

The mod loads into the game, reads the head pose from an OpenVR runtime, and
composes it onto the game camera so that **looking around with the headset moves
the in-game view**. In its shipped configuration the picture still goes to the
monitor: one image, the engine's own field of view, nothing submitted to the
compositor. That is the point — it isolates the view maths (tracking,
handedness, scale) from everything else, which are the things hardest to get
right and easiest to misdiagnose once stereo rendering is in the way.

That is **Phase 1**, and it is where this project currently is. `Mode=mono` is
the default in the shipped ini, it is the mode that is meant to be run today,
and it is what the rest of this README is mostly about.

| | | State |
|---|---|---|
| **Phase 1** | Mono head tracking — one image, engine projection, no compositor | Shipped default. Run this. |
| **Phase 2** | Stereo — per-eye matrices, double-wide target, compositor submit | Implemented in the same binary, off by default, not yet validated on hardware |

Phase 2 is `Mode=stereo` and is described near the bottom. The two share one
binary and one set of hooks — the phase boundary is a config switch, not a
branch.

The renderer was reverse-engineered from the shipped binary and its PDB. Every
address in `src/Engine.h` was read out of Ghidra and re-verified against the
decompiler before being written down; `docs/engine-map.html` is the full map of
how the renderer works and why the hooks sit where they do.

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

Copy four files into the game folder, next to `tomb456.exe`:

| File | From |
|---|---|
| `winmm.dll` | `build\x64\Release\winmm.dll` (the proxy) |
| `TombRaiderVR.dll` | `build\x64\Release\` |
| `TombRaiderVR.ini` | repo root |
| `openvr_api.dll` | `SteamVR\bin\win64\openvr_api.dll` |

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

Five inline hooks. All five are installed either way; in Phase 1 three are
active and two sit inert.

| Function | RVA | Role in Phase 1 |
|---|---|---|
| `vid_setPass` | `0x0000C4C0` | **Active.** Classifies each pass as world-space or 2D |
| `validate_draw` | `0x00011CC0` | **Active.** Composes the head pose into the view matrix |
| `ogl_present` | `0x00012600` | **Active.** Frame boundary; samples the next head pose |
| `ogl_draw` | `0x00012BD0` | Phase 2 only (per-eye duplication) |
| `ogl_drawVB` | `0x00012CF0` | Phase 2 only |

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

## Phase 2: stereo

`Mode=stereo` switches on the rest of the code. It is implemented but has not
been run against a headset, so treat this section as a description of what is
written rather than of what is known to work.

Move to it only once Phase 1 looks right — if the picture moves with your head
and the world feels the right size, the two hardest things (handedness and
scale) are correct, and stereo is then mostly plumbing.

Each draw is issued twice into the two halves of one double-wide render target,
with different matrices and a different viewport. The scene is not re-run. That
matters because the engine creates a **GL 3.2 Core** context and ships **202
shader pairs**: `GL_OVR_multiview2` would mean editing 404 GLSL sources and
would still need driver support on a 3.2 core context. Duplicating draws needs
zero shader changes, because `uViewMatrix` is already a plain `vec4[4]` uniform.

`validate_draw` cannot issue a draw — its two callers do, immediately after it
returns — so per-eye duplication happens one level up in `ogl_draw` /
`ogl_drawVB`. The engine's `FBO_default` global is overwritten with our eye FBO,
so every "return to the backbuffer" inside `ogl_setRenderTarget` lands in the
stereo target without hooking that function at all. Both halves go to the
compositor as one texture split by UV bounds.

First-run toggles:

| Symptom | Setting |
|---|---|
| Upside down in the headset, fine on the monitor | `FlipSubmitV` |
| Upside down in **both** headset and monitor | `FlipProjectionY` |
| World tilts the wrong way when you lean | `FlipViewY` |
| Depth inverted / eyes crossed | `SwapEyes` |
| World feels giant or tiny; IPD wrong | `WorldUnitsPerMetre` |
| Suspect the duplication, not the matrices | `DuplicateDraws=0` |

---

## Known limits

These are honest gaps, not oversights.

- **Phase 2 has never been run against a headset.** Phase 1 is the mode this
  project is meant to be run in today; every limit below is a Phase 2 limit.
- **The frame graph is not fully traced.** The engine renders a post-processing
  chain into its own layered array textures before compositing. The stereo hooks
  only split work that targets the backbuffer, detected by reading the `ogl_rt`
  latch (`color_id == 0 && depth_id == 0`). Post passes that assume a
  full-target viewport may need per-pass handling. Phase 1 sidesteps this
  entirely by injecting on every world-space pass.
- **The HUD is drawn into both eyes flat**, at the same screen position, without
  per-eye matrices. That is a comfortable default but it is not a world-space
  HUD.
- **Only one custom FBO exists in the engine** (`FBO_custom`), and
  `ogl_setRenderTarget` re-attaches colour and depth plus a full
  `glCheckFramebufferStatus` on every target change. We allocate our own FBO
  rather than fight it for attachments.
- **`glCheckFramebufferStatus`'s return value is discarded by the engine.** An
  incomplete FBO from a bad layer index fails silently, so a black screen is a
  plausible symptom of an unrelated mistake.
- **Depth has no stencil anywhere** (`GL_DEPTH_ATTACHMENT` only), so the eye
  target matches that.

## Safety

`Bind()` sanity-checks the host module before anything is written — PE64, and a
`SizeOfImage` large enough to contain the 232 MB `.data` section our globals live
in — and the hooks verify the exact prologue bytes at every target before
patching. On a game update the bytes stop matching, every hook is rolled back,
and the DLL logs `prologue mismatch` instead of corrupting an instruction
stream. Hooks are installed all-or-nothing.

## Layout

```
src/              the mod: hooks, engine map, OpenVR glue, matrix maths
src/proxy/        the winmm shim that gets us loaded
tools/            fetch_openvr.ps1, gen_winmm_forwards.ps1, vrprobe
tests/            selftest (hooks + maths), proxytest (loader behaviour)
docs/             engine-map.html -- the full renderer map
TombRaiderVR.ini  shipped config; Mode=mono, i.e. Phase 1
```
