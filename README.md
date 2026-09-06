# TombRaiderVR

Stereo VR injector for **Tomb Raider IV–VI Remastered** (`tomb456.exe`, v1.0.2a,
2026-01-17 build), driving an OpenVR runtime.

The renderer was reverse-engineered from the shipped binary and its PDB. Every
address in `src/Engine.h` was read out of Ghidra and re-verified against the
decompiler before being written down; `docs/engine-map.html` is the full map of
how the renderer works and why the hooks sit where they do.

---

## What it hooks

Five inline hooks. Two do the VR work, three are the plumbing stereo cannot
function without.

| Function | RVA | Role |
|---|---|---|
| `vid_setPass` | `0x0000C4C0` | **VR work.** Classifies each pass as world-space or 2D |
| `validate_draw` | `0x00011CC0` | **VR work.** Per-eye projection + view injection |
| `ogl_draw` | `0x00012BD0` | Plumbing. Duplicates the draw per eye |
| `ogl_drawVB` | `0x00012CF0` | Plumbing. Same, for the other draw path |
| `ogl_present` | `0x00012600` | Plumbing. Submit, mirror, `WaitGetPoses` |

`validate_draw` is the single choke point where every uniform reaches the GPU,
which makes it the right place to substitute matrices — but it *cannot* issue a
draw. Its two callers do that immediately after it returns, so per-eye
duplication has to happen one level up in `ogl_draw` / `ogl_drawVB`.

`vid_setPass` is the only thing in the engine that distinguishes "3D scene" from
"HUD". It decides per pass whether `vid_state.proj` points at `mProj[0]` (the 2D
ortho layer: logos, menus, subtitles, fades) or `mProj[1]` (world space). Reading
that pointer *after* the original runs is the only correct way to classify a
pass — several cases never assign `proj` at all and inherit the previous pass's
value, so a static table of shader ids would get the sticky ones wrong.

### How stereo is produced

Each draw is issued twice into the two halves of one double-wide render target,
with different matrices and a different viewport. The scene is not re-run.

This matters because the engine creates a **GL 3.2 Core** context and ships
**202 shader pairs**. `GL_OVR_multiview2` would mean editing 404 GLSL sources and
would still need driver support on a 3.2 core context. Duplicating draws needs
zero shader changes, because `uViewMatrix` is already a plain `vec4[4]` uniform.

The engine's `FBO_default` global is overwritten with our eye FBO, so every
"return to the backbuffer" inside `ogl_setRenderTarget` lands in the stereo
target without hooking that function at all.

---

## Building

Requires Visual Studio 2019 or newer with the C++ desktop workload. The project
uses `$(DefaultPlatformToolset)`, so it builds on whatever toolset is installed.

```
powershell -ExecutionPolicy Bypass -File tools\fetch_openvr.ps1
msbuild TombRaiderVR.sln /p:Configuration=Release /p:Platform=x64
```

Output: `build\x64\Release\TombRaiderVR.dll`.

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
No injector.

If OpenVR is unavailable or no headset is present, the DLL logs and returns
without patching anything. Two logs are written beside the DLL:
`TombRaiderVR-proxy.log` (did the shim load?) and `TombRaiderVR.log` (everything
else).

To uninstall, delete `winmm.dll`.

### How the loader hook works

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
Installing hooks after rendering has already begun would be a (small) race
against the render thread; if you ever see that, the fix is to suspend threads
around the patch.

---

## Diagnosing OpenVR: vrprobe

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

Note the two cheap checks are labelled advisory on purpose. `VR_IsHmdPresent` is
a lightweight config probe that returns false in situations where init would in
fact succeed — a headset in standby being the usual one. An earlier version of
this mod treated it as a hard gate and refused to start on it, which threw away
the real `EVRInitError`. It no longer gates on either check; it attempts
initialisation and reports the runtime's own error symbol and description.

If a **Background** init fails with `VRInitError_Init_NoServerForBackgroundApp`,
the mod retries as an **Overlay** app. Background apps deliberately will not
start SteamVR — they attach to a server that is already running — whereas an
Overlay app will bring the runtime up and still does not own the scene.

## Bring-up: mono head-tracking first

`Mode=mono` in the ini is the mode to start in, and it is the default in the
shipped ini. It composes the head pose onto the game camera and renders normally
to the monitor:

- no stereo target, no `FBO_default` redirection, no GL objects created at all
- no draw duplication
- no compositor submission — OpenVR initialises as a **Background** app, so
  SteamVR only has to be running and tracking; the game never becomes the VR
  scene application
- the engine keeps **its own projection**, so field of view is unchanged

That isolates the view maths from everything else. If the picture moves with your
head and the world feels the right size, the two hardest things (handedness and
scale) are correct, and stereo is then mostly plumbing.

Mono also injects on **every** world-space pass rather than only backbuffer-
targeted ones. That is deliberate: the engine's frame graph is not fully traced,
and if the scene renders offscreen before compositing, a backbuffer-only gate
would produce no head-tracking at all and tell you nothing about why.

What to look for in `TombRaiderVR.log`:

```
config: mode=MONO head-tracking ...
vr: head pose ACQUIRED
present: MONO head-tracking active (no stereo target, no submit)
inject: MONO -- view only, engine projection untouched
vr: head @ x=+0.031 y=+1.612 z=-0.204 m  (fwd -0.02 -0.11 -0.99)
```

The `head @` line prints every 120 frames. **If those numbers do not change when
you move, the problem is tracking, not the injection** — which is the one
distinction that is otherwise painful to make.

| Symptom in mono | Try |
|---|---|
| View does not move at all | Check for `head pose ACQUIRED`; check `head @` numbers change |
| Looking up looks down | `FlipViewY` |
| World feels giant / tiny | `WorldUnitsPerMetre` |
| Turning head turns the wrong way | `FlipViewY` (it conjugates rotation, not just translation) |

Switch to `Mode=stereo` only once mono looks right.

## First-run tuning

Almost every "it looks wrong" on first run is one of two sign conventions, so
they are ini toggles rather than a recompile. The engine renders **Y-flipped**
(`ogl_setPerspAngles` writes `e11 = -1/tanY`, and `ogl_setScissor` flips Y
against `gTargetHeight`) and TR world space is **Y-down**, while OpenVR is Y-up
in metres. That is two independent flips.

| Symptom | Setting |
|---|---|
| Upside down in the headset, fine on the monitor | `FlipSubmitV` |
| Upside down in **both** headset and monitor | `FlipProjectionY` |
| World tilts the wrong way when you lean | `FlipViewY` |
| Depth inverted / eyes crossed | `SwapEyes` |
| World feels giant or tiny; IPD wrong | `WorldUnitsPerMetre` |
| Suspect the duplication, not the matrices | `DuplicateDraws=0` |

`WorldUnitsPerMetre` defaults to **423** — Lara is ~762 TR units tall and reads
as roughly 1.8 m. Raise it to shrink the world and the apparent IPD.

---

## Known limits

These are honest gaps, not oversights.

- **The frame graph is not fully traced.** The engine renders a post-processing
  chain into its own layered array textures before compositing. The hooks only
  split work that targets the backbuffer, detected by reading the `ogl_rt` latch
  (`color_id == 0 && depth_id == 0`). Post passes that assume a full-target
  viewport may need per-pass handling. This is the part most likely to need live
  iteration against a headset, and it is the one thing here that has not been
  validated on hardware.
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

The hooks verify the exact prologue bytes at every target before patching. On a
game update the bytes stop matching, every hook is rolled back, and the DLL logs
`prologue mismatch` instead of corrupting an instruction stream. Hooks are
installed all-or-nothing.
