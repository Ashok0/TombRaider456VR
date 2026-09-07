# Tomb Raider IV-VI Remastered VR Mod

**A VR mod for Tomb Raider IV–VI Remastered** (`tomb456.exe`, v1.0.2a,
2026-01-17 build), driving an OpenVR runtime.

The mod loads into the game, reads the head pose from an OpenVR runtime, and
composes it onto the game camera. It is built in two phases that share one
binary and one set of hooks — the phase boundary is a config switch, not a
branch.

| | | State |
|---|---|---|
| **Phase 1** | Mono head tracking — one image to the monitor, engine projection, nothing submitted to the compositor | The bring-up test. `Mode=mono` |
| **Phase 2** | **Native stereo with 6DOF** — per-eye matrices, double-wide target, positional tracking, compositor submit | Working. `Mode=stereo` |
| **Phase 3** | **UI fixes** — the flat 2D layer placed on a world-locked panel, and video cutscenes made fusable | Working. On by default in stereo |

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

Note that the shipped `TombRaiderVR.ini` still selects `Mode=mono`. Set
`Mode=stereo` for Phase 2.

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

> **Note on `EyeOffsetMode`.** The built-in default is `2`, and the shipped
> `TombRaiderVR.ini` does not set the key — so unless you add `EyeOffsetMode=3`
> explicitly, stereo runs the superseded mode that swims when you turn your
> head. Mode 3 is the one the code documents as the fix.

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

The result is head-locked, which is the right behaviour for a full-screen video
anyway. Each bypass shader id is logged once, so if something still refuses to
fuse the log names the shader instead of costing another play session:

```
bypass: shader 63 (0x3F) has no uProjMatrix -- writes clip space directly.
        Shifting its viewport per eye.
```

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

None of these keys are present in the shipped `TombRaiderVR.ini`, so the
built-in defaults above are what runs unless you add them.

---

## Known limits

These are honest gaps, not oversights.

- **The shipped ini still selects Phase 1**, and does not set `EyeOffsetMode`,
  so a bare `Mode=stereo` runs the superseded mode 2 rather than the working
  mode 3. See the note under [Phase 2 settings](#phase-2-settings).
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
trace.txt         the Ghidra session that produced src/Engine.h
TombRaiderVR.ini  shipped config; Mode=mono, i.e. Phase 1
```
