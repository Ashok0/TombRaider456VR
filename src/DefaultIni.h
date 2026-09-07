// DefaultIni.h -- the stock TombRaiderVR.ini, embedded.
//
// GENERATED FILE. Do not edit by hand: change TombRaiderVR.ini in the repo root
// and run  python tools\\gen_default_ini.py
//
// The DLL writes this out when no ini exists beside it, so a fresh install gets
// the documented, working configuration rather than bare struct defaults -- the
// comments in the template carry most of what was learned tuning this thing,
// and a generated key=value dump would throw all of it away.
//
// Source: TombRaiderVR.ini, 29337 bytes, 587 lines.
#pragma once

namespace tr {

// Newlines are LF here; the writer expands them to CRLF on the way out.
// Split into adjacent literals only because MSVC caps one at 16380 bytes;
// the concatenation is a single continuous string and the seams carry no bytes.
inline const char* DefaultIniText() {
    return R"INI(; TombRaiderVR.ini -- place next to TombRaiderVR.dll
;
; If the very first run looks wrong, the fix is almost always one of the two
; sign toggles below. The engine renders Y-flipped (ogl_setPerspAngles writes
; e11 = -1/tanY) and TR world space is Y-down, while OpenVR is Y-up. That is two
; independent flips, so they are exposed rather than hard-coded.
;
;   image upside down in the headset ....... FlipProjectionY
;   world tilts the wrong way when you lean  FlipViewY
;   depth inverted / eyes crossed .......... SwapEyes

[VR]
Enabled=1

; Mode = mono | stereo
;
; mono is the bring-up mode. It composes the head pose onto the game camera and
; renders normally to the monitor: no stereo target, no draw duplication, no
; compositor submission, and the engine keeps its own projection so the field of
; view is unchanged. OpenVR runs as a Background app, so SteamVR just needs to be
; running and tracking -- the game does not become the VR scene application.
;
; Prove tracking, handedness and world scale here first, then switch to stereo.
Mode=stereo

; Rotation-only head tracking. The safe first test: the camera can pivot but
; never be displaced, so a wrong WorldUnitsPerMetre cannot put you inside a wall.
; Set to 1 once looking around behaves correctly.
PositionalTracking=1

; Seated tracking origin rather than standing. Standing space is absolute room
; coordinates -- your head at ~1.6 m becomes ~680 TR units of camera offset
; before you have moved at all. Leave this at 1.
SeatedOrigin=1

; TR world units per metre -- the main scale control.
;
; RAISE IT if the world feels too big and depth feels flat. Those are the same
; symptom: your IPD is fixed, so a world rendered too large leaves the eye
; separation proportionally too small, which reads as both "enormous" and
; "no depth". Lowering it grows the world and weakens depth.
;
; 423 comes from Lara's 762-unit height at ~1.8 m, which is a starting point,
; not gospel. Tune it live with the hotkeys below and copy back what feels right.
;
; NOTE: "too big and flat" was ALSO the signature of the uViewMatrix translation
; bug -- see EyeOffsetMode below. Rule that out before chasing this number.
WorldUnitsPerMetre=423

; Multiplier on eye separation only, leaving world size alone. Use this only if
; the size feels right but depth still reads flat. 1.0 = physically honest.
IpdScale=1.0

; Live tuning hotkeys, logged so you can copy the final value back here.
;   numpad +      SMALLER world, STRONGER depth   (raises WorldUnitsPerMetre)
;   numpad -      BIGGER world, WEAKER depth
;   numpad * / /  IpdScale up / down
;   numpad 0      reset both to the values in this file
;
; Step per press. 1.25 = 25%, so three presses roughly double. Scale differences
; are hard to judge below about 2x, which is why this is not a fine adjustment.
ScaleStep=1.25
ScaleUpKey=0x6B
ScaleDownKey=0x6D
IpdUpKey=0x6A
IpdDownKey=0x6F
ResetTuningKey=0x60

; 0 = use the size OpenVR recommends.
EyeWidth=0
EyeHeight=0
SuperSample=1.0

FlipProjectionY=1
FlipViewY=1
SwapEyes=0

; Vertically flip the eye texture when handing it to the compositor.
; Leave at 0. The engine's Y-negated projection already cancels TR's Y-down
; world, so the eye texture is an ordinary GL render and the compositor wants
; default V bounds.
;   upside down in the HMD but fine on the monitor .. set this to 1
;   upside down in BOTH ......................... change FlipProjectionY instead
FlipSubmitV=0

; --- per-eye rendering ------------------------------------------------------

; Where the per-eye TRANSLATION is applied. Leave this at 3.
;
;   0 = uViewMatrix translation column. BROKEN: 38 of the engine's 125 vertex
;       shaders transform position as dot(uViewMatrix[i].xyz, p.xyz) -- rotation
;       ONLY, never reading the translation column. Invisible to them, so: no
;       parallax, no depth, everything at infinity (reads oversized), and dead
;       IpdScale / WorldUnitsPerMetre keys.
;   1 = uModelMatrix translation column. ALSO BROKEN: skinned geometry goes
;       through uJoints[72*3], not uModelMatrix, so characters do not move with
;       the world and float outside the map.
;   2 = uProjMatrix + translate(d). Depth and scale work, but the world swims
;       when you rotate your head: d = (R_e - I)*t_g + t_e, and t_g is ~82000
;       units, so any head rotation makes the first term thousands of units.
;   3 = uProjMatrix composed with the WHOLE eye transform, view matrix left
;       untouched. THE FIX. Untouched, both shader families agree on the
;       view-space position; E is metres-scale so head rotation cannot blow up.
EyeOffsetMode=3

; Apply the per-eye view matrix at all. 0 leaves the game's own camera in both
; eyes, so the only remaining difference is the frustum shear -- a constant
; sideways shift that carries no depth. Diagnostic only.
PerEyeView=1

; Per-eye projection:
;   0 = the engine's own. NOT a clean control -- it also drops the HMD field of
;       view and squeezes a 1.78-aspect frustum into a 0.93-aspect viewport, so
;       it changes two things at once.
;   1 = the HMD's true asymmetric frustum. Correct. Leave it here.
;   2 = HMD field of view, symmetrised: same total extent per axis, shear forced
;       to zero. Single-variable test for the shear. Expect double vision -- the
;       headset optics assume the asymmetric render and misplace each eye by
;       ~15 degrees without it.
PerEyeProjection=1

; --- diagnostics (all off) --------------------------------------------------

; Red bar burned into the left half of the eye target, blue into the right.
; Reads eye mapping directly:
;   red left, blue right ... halves map to eyes correctly
;   both bars in both eyes . submit bounds are being ignored
;   same colour both eyes .. both eyes are getting the same half
;   no bars at all ......... what you see is not this texture
EyeMarkers=0

; Yaw the RIGHT eye's view by this many degrees. 0 = off.
; Tests whether the view matrix drives rendering at all, with an effect far too
; large to mistake for subtlety. At 20 the two eyes visibly look in different
; directions. Rotation is honoured by every shader; translation is not.
DebugEyeYawDegrees=0

; Leave the 2D layer (HUD, menus, subtitles, fades) on the flat path. It is
; still drawn into both eyes, just without per-eye matrices.
FlatHud=1

; Distance in metres at which the flat 2D panel sits. Raise it to push the HUD
; and menus further away.
;
; 0 = leave the layer exactly as the engine drew it, which double-visions: the
; 2D layer uses the engine's ortho projection, identical in both eyes, but the
; headset optics apply a fixed ~15-degree per-eye correction assuming an
; asymmetric render, so an unshifted image gets pulled apart.
HudDepthMetres=4.0

; Horizontal angular width of the 2D panel in degrees, as seen from the game
; camera. The engine's 2D layer fills the flat screen; across the headset's ~94
; degree field of view that would be overwhelming, so it goes on a panel this
; wide instead. Vertical follows the screen aspect. Raise for a bigger panel,
; lower for a smaller one. Ignored when HudLockToHead=1.
HudSizeDegrees=55.0

; 0 = the 2D layer is a quad fixed in the GAME camera's frame, so it stays put
;     in the world while your head turns. This is what you want.
; 1 = old behaviour: welded to your head, swings with every rotation.
HudLockToHead=0

; Negate Y when lifting the 2D layer onto the panel. Leave at 1.
; The engine's ortho already flips Y (TR 2D space is Y-down), and the per-eye
; projection expects Y-DOWN input, so without this the layer is flipped twice.
; That inverts it AND reverses triangle winding, so backface culling removes the
; whole HUD -- leaving only elements drawn with culling off, upside down.
HudFlipY=1

; --- placement carried in the projection ------------------------------------

; Carry the engine's own projection OFFSET through the per-eye substitution.
;
)INI"
           R"INI(; vid_setPerspOffset writes e02/e12 -- the two shear terms -- and nothing else.
; A shear of s shifts the image by -s at every depth, so it is how the engine
; places a draw on screen without moving its geometry.
;
; The inventory is laid out entirely with it. Measured, one frame: nine item
; draws, each with an identity model matrix, an identity view matrix and one
; joint, differing ONLY in the shear -- 2.0250, 1.3500, 0.6750, 0.0000, -0.6750,
; -1.3500, evenly spaced 0.675 apart. That spacing IS the horizontal bar, and
; the values past +/-1 are the items scrolled off the sides.
;
; Writing the eye frustum over the top discarded all of it, so every item got
; the same shear and landed in the same place. That was the stacked inventory.
;
; The offset is re-applied as a SKEW of the engine's camera space, not as a
; shift of the headset's NDC. A projection carrying shear s is exactly the same
; projection without it applied to space pre-skewed by (s/scale)*z, so the skew
; reproduces the engine's placement whatever else we have done to the matrix --
; and it has to go innermost, after the per-eye transform, because the engine's
; shear multiplies the ENGINE's view-space z.
;
; Both details were got wrong first time round and both were visible. Adding the
; shear onto the eye frustum instead put it on the far side of the head
; transform, so the offset swung with head orientation and the row sheared as
; you looked around; and it matched the NDC coordinate rather than the ANGLE,
; which matters because the frustums differ -- measured, engine tanX 1.119 vs
; eye 1.108, but engine tanY 0.629 vs eye 1.197. The row came out right to 1%
; while the vertical placement was thrown 1.9x too far, into the lens
; distortion. That was the fishbowl.
;
; ogl_setPersp and ogl_setPerspAngles both zero the shear explicitly, so this
; adds exactly zero during gameplay. 0 = old behaviour, for A/B.
PreserveProjOffset=1

; Trim on the whole offset. 1.0 reproduces the engine's layout exactly, so the
; inventory row spans the same ANGLE it does flat -- and on a 96 degree game
; frustum that is a wide row to sweep your eyes across in a headset. Lower it to
; pull the items in toward the centre (0.7 is a reasonable first try); 0
; collapses them back into one stack, which is what the bug looked like.
ProjOffsetScale=1.0

; --- the ortho-3D layer (the inventory) -------------------------------------

; vid_setOrtho3D copies mProj[0] -- the ORTHO matrix -- into mProj[1] and
; repoints vid_state.proj at it, so by pointer alone (which is all the world/2D
; test has) such a pass looks world-space. Handing it a perspective frustum
; would divide an ortho layout by a depth it was never built for.
;
; NOT what caused the stacked inventory -- that was PreserveProjOffset above.
; This path has never been observed to fire in TR4 or TR5; the projoffset= and
; ortho3D= counters in the health report say whether it ever does.
;
; 1 = also check the matrix itself (e32/e33 tell ortho from perspective apart)
;     and keep the engine's own projection for those passes.
; 0 = old behaviour, for A/B.
Ortho3D=1

; Distance in metres at which the ortho-3D layer converges. 0 = no shift at all:
; the layer is left exactly as the engine drew it, which still fixes the
; stacking but double-visions the way an untouched HUD does.
Ortho3DDepthMetres=2.0

; 1 = head-locked convergence, and the default HERE even though the HUD defaults
;     the other way. An ortho projection has no perspective divide, so shifting
;     it is pure convergence: relative x, y AND z all survive untouched. The
;     HUD's world-locked panel discards its z input, which is harmless for flat
;     overlays and wrong for these -- they are meshes with real depth, and
;     flattening them makes every item z-fight with itself.
; 0 = world-locked panel, same construction as the HUD's but with the z column
;     filled in so ortho depth lands in a slab instead of on one plane.
Ortho3DLockToHead=1

; Horizontal angular width of the world-locked panel, in degrees. Ignored when
; Ortho3DLockToHead=1.
Ortho3DSizeDegrees=55.0

; Thickness of the world-locked panel's depth slab, in metres: ortho depth maps
; onto Ortho3DDepthMetres plus or minus half of this. Too thin and the item
; meshes z-fight; too thick and they visibly separate in depth. Ignored when
; Ortho3DLockToHead=1.
Ortho3DSlabMetres=0.5

; Distance in metres for full-screen passes that bypass uProjMatrix entirely.
; Five of the engine's shaders write gl_Position = vec4(aCoord, 1.0) -- straight
; to clip space, no matrix at all -- and pre-rendered video runs through that
; path. No matrix can move them, so the viewport is shifted per eye instead.
; 0 disables the shift entirely.
VideoDepthMetres=6.0

; 0 = the video panel is anchored to the GAME camera, so it stays put in the
;     world while your head turns, and leaves view if you turn away.
; 1 = welded to your head.
VideoLockToHead=0

; Horizontal angular width of the video panel in degrees, seen from the game
; camera. Also controls how flat it looks: the panel's four corners are
; projected and the viewport fitted to their bounding box, which gives correct
; position and perspective SIZE. What is left is keystone -- an off-axis quad
; should go trapezoidal and a bounding box stays rectangular -- and that error
; grows with width. 50-70 is a good cinema size; above ~90 it shows at the edges.
VideoSizeDegrees=60.0

; 1 = capture the video pass into an offscreen texture and replay it as a REAL
;     quad. Removes keystone entirely: it foreshortens, keystones and rolls like
;     the world does, because it is world geometry. Costs one framebuffer bind
;     and two quad draws per video frame, and only during video -- the branch
;     never runs in gameplay. It also rasterises the video once instead of
;     twice, so it nearly pays for itself.
; 0 = viewport fit (corner bounding box). Correct position and size, residual
;     keystone off-axis.
VideoOffscreen=1

; Flip the captured video vertically on replay. Only if it comes out upside down.
VideoFlipV=0

; In TR6, use the offscreen video panel ONLY while a video is actually playing.
; Leave at 1.
;
; TR6 composites its offscreen scene to the backbuffer through a
; clip-space-direct shader -- the same signature as an FMV quad -- so the shader
; test alone would capture the whole game and turn it into a floating panel.
; Hooking fmvShow gives an exact "a video is on screen this frame" signal, so
; TR6's cutscenes get the real-geometry panel (no keystone, no tilt on head
; roll) while its gameplay and menus are left alone.
VideoSkipGame6=1

; Alternate-eye rendering for TR6 (Angel of Darkness). Leave at 1.
;
; TR4/TR5 draw straight to the backbuffer, so every world draw can be issued
; twice into the two halves of one double-wide target. TR6 cannot work that way:
; it renders the scene into the engine's own 2560x1440 offscreen textures and
; composites at the end, and the composite samples those whole with 0-1 UVs.
; Measured: 572970 of 579945 world draws offscreen, against ~9% in TR4/5.
;
; So each FRAME is rendered entirely as one eye and lands in that eye's half.
; Nothing is resized, nothing is duplicated, and both eyes are stereo-correct.
;
; The cost is real: each eye updates at HALF the frame rate (45 Hz of a 90 Hz
; headset). If that judders too much for you, set this to 0 -- TR6 then renders
; flat but at full rate.
; ON. This was switched off only to test the depth-reprojection warp against
; it. That warp was abandoned -- its colour fetch returned black and was never
; explained -- and its settings have since been removed, but this was never
; switched back on until now.
AlternateEyeGame6=1

; How many offscreen world draws a frame needs before alternate-eye engages.
;
; AER is only worth its half-rate cost when there is an offscreen 3D scene to
; reach. The TR6 main menu and the FMVs are 2D -- they draw straight to the
)INI"
           R"INI(; backbuffer, so ordinary per-draw duplication handles them at FULL rate and
; looks better doing it. Gameplay runs ~800 offscreen world draws a frame;
; menus and video are near zero, so the count separates them cleanly.
;
; Raise it if a menu still drops to half rate; lower it if gameplay does not
; engage. 0 would force AER on for every TR6 frame.
AlternateEyeMinOffscreen=50

; --- controllers ------------------------------------------------------------

; Present the Oculus Touch controllers to the game as an Xbox pad.
;
; The game resolves XInput through GetProcAddress into a function-pointer
; global, so there is no import to hook and no virtual-pad driver (ViGEm etc.)
; needed -- we overwrite that pointer. inputUpdate() polls it every frame and
; switches to INPUT_TYPE_XB on the first input, so the game adopts the Xbox
; control scheme and button prompts on its own. A real pad, if plugged in, is
; merged rather than replaced.
;
;   Move    left stick              Jump    A  (right hand, LOWER)
;   Look    right stick             Roll    B  (right hand, UPPER)
;   Action  Y  (left hand, UPPER)   System  X  (left hand, LOWER)
;   Walk    left stick + RIGHT GRIP Duck    LEFT GRIP
;   Equip   left trigger (hold)     Shoot   right trigger
;   Sprint  left stick click        Photo   both grips
;
; Walk is the right grip rather than a face button so it can be held while the
; left thumb keeps moving. The game binds Walk to XInput X, so the grip emits X
; -- and RIGHT_SHOULDER as well, so the Photo Mode chord (LB+RB) still works.
GamepadEnabled=1

; Log the raw OpenVR legacy button masks whenever they change. Touch's button
; ids differ between runtimes, so if a button lands in the wrong place this says
; which mask it actually set. Leave at 0 for normal play.
GamepadLogButtons=0

; Hold R3 (right stick click) to turn the LEFT stick into a D-pad.
;
; R3 is the modifier rather than L3 because L3 is Sprint, and sprint here is
; held WHILE running forward -- "hold L3, push the stick forward" is already a
; gesture in play, so reusing it for D-pad up would mean choosing between them.
; R3 was the only spare input on the pad.
;
; A plain R3 click, with the stick centred, still emits RIGHT_THUMB exactly as
; before -- the D-pad only appears once the left stick is actually deflected,
; so nothing that relied on R3 is lost. The left stick's movement axes are
; zeroed while shifted, so you do not walk and press a direction at once.
;
; Only the dominant axis fires, so a flick cannot emit up and left together and
; move a menu selection twice.
DpadShift=1

; Hold Y + LT for this many seconds to send the Xbox Menu button (XInput START).
;
; Touch has no Start or Back of its own, so both have to come from somewhere.
; The System button on the left hand's lower face sends one of them (see
; GamepadMenuUsesBack); this chord reaches the other without spending a button.
;
; The hold is what separates the chord from real play: Y is Action and LT is
; Equip, so the pair does occur naturally. One second is comfortable but IS
; within reach of normal play -- raise this if it ever fires unintentionally.
;
; It sends a PRESS, not a latch -- Menu toggles the pause screen itself, and a
; held START would never look like a clean press to the game. It fires once per
; hold; release and re-hold to send another. Y and LT are suppressed from the
; moment it fires until you let go, so it does not keep grabbing afterwards.
;
; 0 disables the chord entirely.
MenuChordSeconds=1.0

; How long the synthesised Menu press is held, in seconds.
MenuChordPressSeconds=0.15

; How far the left stick must travel before a shifted press registers, 0-1.
; Raise it if directions trigger too easily, lower it if they feel stiff.
DpadShiftDeadzone=0.5

; Which XInput button the left hand's LOWER face button sends.
;   1 = BACK  -- the System menu. Default.
;   0 = START -- the pause/inventory menu.
; inputUpdate() decodes BACK to internal key 0x62 and START to 0x63; which one a
; given screen treats as System lives in the game DLLs, so this stays switchable.
GamepadMenuUsesBack=1

; --- reverse engineering ----------------------------------------------------

; Log the DLL-side return address of each distinct call into vid_setPass and
; ogl_drawVB, as "module+RVA".
;
; The game DLLs ship without PDBs, so this is how we find their render code
; without searching ~1800 unnamed functions: the DLL must call across into the
; engine to draw, and the return address at our hook is a code address inside
; the DLL. One gameplay frame gives exact RVAs to open in Ghidra.
;
; Deduplicated and capped at 64 sites per hook, but still verbose. Off for play.
LogCallsites=0

; --- measured, and no longer behind a key ------------------------------------
;
; Findings from tuning the room culling whose own keys have since been removed.
; Each one rules something OUT, and without them the next person looking at a
; phantom texture re-runs these by hand.
;
; HOP DEPTH IS CHEAP AND SATURATES. Going from 2 hops to 3 on a 242-room TR5
; level added about ten rooms -- 30-45 became 42-54 -- and moved the frame rate
; not at all, 44.5-45.0 fps either way. Connectivity runs out long before the
; draw list does, so PortalHops is not the setting to be stingy with.
;
; DRAWALLROOMS IS THE EXPENSIVE ONE. TR5 Streets of Rome has 116 rooms; the
; traversal finds 3 to 12 of them, and forcing the rest drew 115 every frame --
; about ten times the engine's own working set, doubled again for stereo.
; Affordable with stock textures, a slideshow with the HD-texture build.
;
; ROOM 215 IS NOT A FLIP ROOM, and flip detection does not catch it. Confirmed
; by a run where dropping a forced-room cap from 20 to 19 was what removed it
; from the list. Nor is it explained by its flags: its only unusual one (0x4000)
; is a lighting bit shared by 19 well-behaved rooms. The cause is still unknown,
; which is why DrawAllRoomsExclude below is per-index rather than a rule.
;
; MATCHING FLIP PAIRS BY WORLD POSITION IS WRONG. It claimed 78 of 242 rooms in
; Streets of Rome. The engine's own flipped_room field replaced it -- the live
; room names its storage copy, the copy holds -1 -- and skipping storage copies
; is now unconditional in the code rather than a switch.
;
; THE CULLING LIVES IN tomb4.dll AND tomb5.dll, NOT THE EXE. Widening the
; projection the DLL hands the game was measured to change nothing.

; Diagnostic. DrawAllRooms makes two separate changes: it appends rooms to the
; draw list, and it widens every listed room's clip rect to the full screen.
; Set this to 0 to keep the first and drop the second. Forced rooms then will
; not render at all -- that is expected; it is there to tell the two apart.
DrawAllRoomsClipRect=1

; Issue every draw twice, once per eye. Turn off to keep matrix injection but
; draw once -- useful for telling a matrix problem from a duplication problem.
DuplicateDraws=1

MirrorToWindow=1

; 0 = keep whatever near/far the current pass set.
NearClip=0
FarClip=0

; Log the first injected projection and view to TombRaiderVR.log.
VerboseFirstFrame=1

; Frame-graph trace. Logs every render-target transition and the world/2D draw
; counts against each. This is what tells us where the 3D scene is actually
; drawn -- the one thing stereo depends on.
;
; Set TraceFrames=0, get into ACTUAL GAMEPLAY (not a menu or loading screen),
; then press the TraceKey. A menu frame has a handful of draws; gameplay has
; hundreds, and the log says so if it looks like a menu.
TraceFrames=0

; Virtual-key code for the capture hotkey. 0x79 = F10, 0x78 = F9, 0x7A = F11.
TraceKey=0x79

; Optional frame-number trigger instead of the hotkey. 0 = hotkey only.
TraceStartFrame=0

; Expand the engine's visible set along PORTAL CONNECTIVITY by this many hops.
; 0 = off. This is what fixes geometry going missing when you turn your head.
;
; The engine's traversal runs in the GAME CAMERA's space -- VR is injected at
)INI"
           R"INI(; the shader uniform, so the engine's own view matrix never rotates with your
; head. A portal beside or behind the camera fails the near-plane test inside
; the clipper, so widening its rectangle does nothing (measured: 200% of screen
; each way changed neither geometry nor frame rate). Connectivity has no
; orientation bias -- a room through the door behind you is one hop away
; whichever way the camera happens to face.
PortalHops=3

; Before adding a hop room, test the portal we would reach it through against
; the ACTUAL HEADSET FRUSTUM.
;
; Hop expansion adds any portal-connected room and draws it WITHOUT a portal
; clip, so a room that is connected but not actually visible through that
; doorway still gets drawn -- and where it shares world space with somewhere you
; can see, you get foreign geometry over your own. That is the room-215 bug.
;
; The engine already does this test; it just does it from the game camera. This
; does it from the head, which is the piece that was missing. Set to 0 to get
; the old behaviour back for comparison.
; OFF until proven. The first version transformed portals using the view
; matrix translation column, which is not what a view matrix carries here --
; the error scaled with world coordinates, so it behaved on a 116-room level
; and rejected essentially every portal on a 242-room one, which is what took
; your geometry. Now uses rotation only with the camera position from the
; engine globals. Set to 1 to try it; watch the "head test" ratio in the log.
PortalHeadTest=0

; How far outside the frustum a portal may sit and still count as visible, in
; NDC (0.35 = 35% of half-width). Generous on purpose -- losing geometry is the
; worse failure. Raise it if anything vanishes at the edges, lower it if
; unwanted rooms still get through.
PortalHeadMargin=0.35

; Legacy: append rooms by DISTANCE, ignoring portals. Superseded by PortalHops
; and kept only for A/B. Proximity is the wrong criterion -- it will add a
; stacked room sharing world space with the one you are standing in.
DrawAllRooms=0

; Room indices hop expansion must never add. Comma or space separated, per level.
;
; Not a fix -- what makes room 215 special has never been identified, and three
; general mechanisms (flip detection, portal-rect widening, AABB overlap) all
; failed to characterise it. But it is one line and it demonstrably works, so it
; stays until the head test above is proven.
;
; 215 and 12 are also the BUILT-IN default, so deleting this line leaves them in
; force rather than clearing them. To run with no exclusions at all, keep the
; line and give it no value: an empty DrawAllRoomsExclude= means none, while an
; absent one means "the ini has no opinion" and the built-in list stands.
DrawAllRoomsExclude=215,12

; Dump the current room draw list to the log. One block per press. 0 = off.
;
; Default is F8. This is how you find the number to put in
; DrawAllRoomsExclude above: the per-frame log lines report counts only
; ("drawing 43 of 242"), and a count names nothing.
;
; Stand where the bad geometry is visible and press it. The block lists the
; rooms the engine's own traversal reached -- never the culprit, they carry
; real portal clip rects -- and then the rooms expansion APPENDED, which is
; the only set this exclude list can remove from. Appended rooms are printed
; nearest first, each with the room it was hopped from, which hop wave added
; it, and its world BOUNDING BOX, built the way the engine's own per-room AABB
; test builds it.
;
; Three flags say where you are standing relative to a room's box:
;   CONTAINS THE CAMERA  the room's box encloses you and the engine's own
;                        traversal did not reach it. Drawn with a full-screen
;                        clip rect, so its walls land across your whole view.
;                        This is the strongest phantom signal there is.
;   STACKED OVERHEAD     your XZ is inside its footprint, its floor is above
;                        you -- the room over a ceiling.
;   STACKED UNDERFOOT    the same below you. This is the grate case: a room
;                        under a floor grating is one hop away through a floor
;                        portal and renders across your view instead of through
;                        the opening.
;
; The room ORIGIN is not usable for any of this. Its Y is zero on every room
; measured -- TR carries absolute Y in the vertex data rather than a room
; origin -- so a room's height comes from its box, not its position.
;
; Take the dump BEFORE editing the exclude list, and note that indices are per
; level -- a number that helps one level means nothing in another.
;
; A function key on purpose. Do not move this to the numpad: with Num Lock OFF
; the physical numpad keys emit navigation codes instead (numpad 3 becomes
; VK_NEXT), GetAsyncKeyState never sees the press, and the dump does nothing at
; all with no clue as to why. The startup line in the log names the key actually
; in force -- "dump key 0x77" -- so a dump that does not appear can be told from
; a key that was never armed.
RoomDumpKey=0x77

; Per-draw state dump: which matrix carries the difference between one drawn
; element and the next -- projection, view, or model. Get the screen in
; question up, then press DumpKey. One line per draw, logged before any of our
; substitutions, so what appears is what the ENGINE set. 0 = disabled.
DumpDraws=0

; Virtual-key code that arms the dump. 0x78 = F9.
DumpKey=0x78
)INI";
}

} // namespace tr
