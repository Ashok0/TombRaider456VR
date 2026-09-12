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
// Source: TombRaiderVR.ini, 34059 bytes, 676 lines.
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

; Stop the tracked head rising through the ceiling in low tunnels and
; crawlspaces. 1 = on and the default; 0 = the stock uncapped head.
;
; The game camera already sits near the ceiling in those places, and the
; positional offset pushes the eye straight through it.
;
; This CLAMPS, it never moves you. The head's height is capped so the eye stays
; CeilingMarginUnits below the room's ceiling, and everything below that cap
; behaves exactly as before -- ducking, leaning and every rotation pass through
; untouched. That is deliberate: a camera that drops to torso height on its own
; would be unrequested vertical motion, which is a comfort problem, and a clamp
; can only ever subtract motion you would have had.
;
; The headroom comes from the camera room's BOUNDING BOX, so it is exact in a
; uniformly low room -- which is where a head clips through in the first place
; -- and over-generous in a low pocket off a tall hall, where the clamp simply
; does not engage. Wrong in the safe direction: it can fail to clamp, but it can
; never clamp somewhere roomy.
;
; Logs once the first time it bites:
;   vr: ceiling clamp active -- headroom 892 units, head capped at 0.24 m ...
CeilingClearance=1

; How close the eye may get to the ceiling, in world units. 128 is about 0.30 m
; at the default scale. Raise it if you still clip through, lower it if the cap
; feels like it arrives too early.
CeilingMarginUnits=128

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
)INI"
           R"INI(; 2D layer uses the engine's ortho projection, identical in both eyes, but the
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
; vid_setPerspOffset writes e02/e12 -- the two shear terms -- and nothing else.
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
)INI"
           R"INI(; roll) while its gameplay and menus are left alone.
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
; backbuffer, so ordinary per-draw duplication handles them at FULL rate and
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

; Right stick turns only. Its vertical axis is dropped, so the game camera never
; pitches from the stick. 1 = on and the default; 0 = the stock two-axis stick.
;
; In VR the headset already owns pitch: you look up by looking up. Stick pitch is
; then a second source for the same axis, fighting the first, and it is the
; uncomfortable one -- a vertical rotation your inner ear did not ask for is the
; strongest simulator-sickness trigger there is, worse than yaw. Yaw is left
; alone because turning on the spot with the stick is how you play seated.
;
; It does genuinely take something away: the engine aims and reads its look
; camera from the pitch this suppresses, so a shot lined up by tilting the stick
; has to be lined up by tilting your head instead. It is ON by default anyway,
; because DecoupledPitchChord below hands the stick pitch back whenever you hold
; RT+RB -- nothing is out of reach, and comfort is the right default in a
; headset. Set this to 0 for the stock two-axis stick.
;
; Applied to whatever right stick reaches the game, including a physical pad
; merged in alongside the Touch controllers. The binding line in the log says
; "look=Rstick(yaw only; hold RT+RB for pitch)" when both are on.
DecoupledPitch=1

; Hold RT + RB to get stick pitch BACK for as long as both are held. 1 = on,
; and it costs nothing when unused.
;
; This only ever hands pitch back -- it never takes it away. With
; DecoupledPitch=0 the stick already pitches and the chord does nothing at all;
; with DecoupledPitch=1 you keep the comfortable head-only default and can still
; reach for the stick for the one shot that wants it, then let go.
;
; A momentary control has to mean ONE thing. An earlier version inverted the
; setting instead, which made the same chord decouple pitch or restore it
; depending on a value you cannot see while playing.
;
; KNOW WHAT RB IS HERE. Touch has no physical shoulder buttons: the RIGHT GRIP
; synthesises XB_X and XB_RIGHT_SHOULDER together, because X is what the game
; binds Walk to and RIGHT_SHOULDER is what keeps the LB+RB Photo Mode chord
; reachable. So this chord is right grip + right trigger, which in play reads as
; "walk and shoot" -- a combination people genuinely use, on a ledge especially,
; and it WILL engage the chord. Set this to 0 if you would rather walk-and-shoot
; leave pitch alone.
;
; The chord only ever ADDS the suppression. Shoot and Walk still do their jobs
; while it is held; nothing is taken away to pay for it.
DecoupledPitchChord=1

; Hand stick pitch back automatically while Lara is in the water. Leave at 1.
;
; The one place DecoupledPitch=1 is not a comfort win but a straight loss: TR
; steers the swim with the LOOK axis, so suppressing pitch removes the ability
; to dive or surface at all -- and the head cannot stand in for it, because the
; head turns the VIEW while the stick turns LARA herself.
;
; Read from Lara's own water_status in the game DLL, so it follows HER and not
; the camera. An earlier attempt read the camera room's water flag instead and
; failed exactly where it mattered: during a surface swim the camera sits in
; the air room above the water and reports dry for the whole swim.
DecoupledPitchWaterOff=1

; Hand stick pitch back automatically while looking through an optic --
; binoculars, or a weapon combined with a laser sight. Leave at 1.
;
; The same failure as swimming, for the same reason: the game's own zoom
; camera reads the right stick's Y axis directly for vertical aim once it has
; taken over, and the head cannot stand in for it there either. Without this,
; holding the zoom (hold the right stick click) gets you a scope that pans
; sideways but never up or down.
;
; Read from the same two fields the game's own look-handling code checks to
; decide whether the stick means "look around" or "aim the zoom camera", so
; this follows the game's own notion of "currently zoomed" rather than a guess.
DecoupledPitchZoomOff=1

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

)INI"
           R"INI(; Hold Y + LT for this many seconds to send the Xbox Menu button (XInput START).
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

; --- room culling -----------------------------------------------------------
;
; THE PROBLEM. The engine decides what to draw by walking portals out from the
; room the GAME CAMERA is in, carrying a screen rectangle that is clipped at
; every doorway. A room is submitted only if some chain of doorways lands on the
; game camera's screen. That is exactly right for a monitor and exactly wrong
; for a headset: you see wider than the game camera, and you can look somewhere
; it is not pointing at all. Turn far enough and the geometry that should be
; there was never drawn.
;
; THE FIX. The same traversal is run a second time from the tracked HEAD, in
; world space, with the headset's frustum, and anything it finds is appended.
; At every doorway the frustum is clipped to the opening -- the portal quad is
; cut against the incoming planes and a new plane is built from the head through
; each surviving edge -- so a room is added only if it can really be seen
; through that chain of doorways.
;
; Nothing the engine listed is ever removed, so PortalCulling=0 gives the stock
; behaviour exactly, and with your head aligned to the game camera the result is
; what the engine would have drawn anyway.
;
; WHAT HAPPENED TO PortalHops AND FRIENDS. They are gone, and so is the whole
; problem they were managing. Hop expansion added every room N doorways away
; with no visibility test, so N had to be tuned per level, rooms you could not
; actually see got drawn anyway (the room-215 bug, and DrawAllRoomsExclude with
; it), and because it reached rooms the traversal never would, it also reached
; flip-map STORAGE rooms and needed a flipped_room check to stop them being
; drawn over the live geometry. A real traversal cannot reach a storage room --
; no live room's portals name one -- so none of that machinery survives. If your
; ini still sets those keys the log will say so once, on startup.
;
; Watch it work in TombRaiderVR.log:
;   gamedll: bound to tomb4.dll (Tomb Raider IV, build 0x696B4999, game=0) ...
;   cull: hooked tomb4.dll (Tomb Raider IV)
;   cull: head-frustum portal traversal live on Tomb Raider IV -- ...
;   cull: 31.2 rooms/frame from the engine + 8.4 added by the head frustum, ...
PortalCulling=1

; Angle added to each half of the culling frustum, in degrees.
;
; Covers two small things: the traversal runs once from the head rather than
; once per eye, so the couple of degrees a canted display puts between the two
; frusta has to be allowed for, and the pose that culls a frame is a few
; milliseconds older than the pose that renders it.
;
; Raise it if geometry pops in at the very edge of vision when you turn quickly.
; Every degree costs a little more draw.
CullFovMarginDegrees=8

; How many doorways deep the traversal may go, and how many portals it may look
; at in one frame.
;
; NEITHER IS A VISIBILITY CRITERION. The frustum shrinking at every doorway is
; what stops the traversal; these only bound the worst case so a pathological
; level cannot spend the whole frame in here. If the log ever says
;   *** A BUDGET WAS HIT -- raise CullMaxPortals/CullMaxDepth ***
; during normal play, raise them. TR5's largest level is 242 rooms, so the
; defaults have a lot of headroom.
CullMaxDepth=16
CullMaxPortals=4096

; Optional distance limit in world units, 0 for none. One sector is 1024.
;
; Off by default on purpose. The engine's own phd_zfar is 65536 units, which
; culls nothing, and a room you can see down a long corridor is a room you
; should be able to see. This is a frame-rate lever, not a fix -- set it and
; distant rooms will pop.
CullFarUnits=0

; Widen every listed room's clip rectangle to the whole target. Leave at 1.
;
; This is the old DrawAllRoomsClipRect and it is still required for the same
; reason: the engine's portal-clipped rects are screen boxes computed for the
; GAME CAMERA's view, and we render from the HMD's, so under head rotation they
; scissor the wrong part of the screen entirely.
CullWidenBounds=1

; Extend the same fix to items: furniture, enemies, pickups, statics.
;
; The engine rejects an item whose bounding box misses the game camera's screen
; rect or sits behind its near plane -- which is every item in every room this
; feature adds. Hop expansion never touched this, so the rooms it forced in drew
; EMPTY. With CullObjects=0 you get that behaviour back.
;
; Answers are only ever promoted from "invisible" to "visible, clip it", never
; the other way. No menu gate is needed: the only callers of the engine's test
; are world-drawing paths, and the inventory ring does not go through it.
CullObjects=1

; Virtual-key code that dumps the current draw list to the log, 0 = disabled.
; Replaces RoomDumpKey. Prints which rooms the engine found and which the head
; frustum added, starred -- which turns "that wall is missing" into a room
; number.
;
; Each added room is then listed again with the doorway it came through and how
; many doorways deep it was, so a room that should not be there can be traced
; back along the sightline that let it in:
;
;   cull dump:   room 215   via room 188   3 doorway(s) deep
;
; A function key on purpose. Do not move this to the numpad: with Num Lock OFF
; the physical numpad keys emit navigation codes instead (numpad 3 becomes
; VK_NEXT), GetAsyncKeyState never sees the press, and the dump does nothing at
; all with no clue as to why.
CullDumpKey=0x77

; Room indices to watch, comma or space separated. Empty = off.
;
; Any listed room the traversal appends is reported once per distinct route:
;
;   cull watch: room 215 reached via room 188, 3 doorway(s) deep -- the head
;   frustum can see through that chain of openings
;
; This is the targeted form of the dump key. A room that only misbehaves for a
; moment is hard to catch with a hotkey, and "does the traversal ever reach 215,
)INI"
           R"INI(; and how" should not need good reflexes to answer. Silence means it never got
; added, which is itself the answer.
CullWatchRooms=

; --- measured, and no longer behind a key ------------------------------------
;
; Findings from the culling work whose own keys have since been removed. Each
; one rules something OUT, and without them the next person looking at a
; phantom texture re-runs them by hand.
;
; WIDENING THE PROJECTION DOES NOTHING. Swept to 200% of screen each way, and
; 20x on TR6: neither extra geometry nor a frame-rate change. The game DLL
; builds its cull planes from its own matrices and never consults the engine's
; projection matrix, and a portal beside or behind the camera fails the
; near-plane test inside SetRoomBounds before any rectangle is looked at.
;
; PROXIMITY IS THE WRONG CRITERION. Appending rooms by distance from the camera
; happily adds a stacked room that shares world space with the one you are
; standing in and that no portal reaches, which draws foreign geometry over your
; own. This is why the fix is a traversal and not a radius.
;
; ROOM BOXES OVERLAP NORMALLY. Rejecting an added room whose world bounding box
; overlaps one already drawn looked principled and was wrong: a TR room box is a
; rectangle around an irregular interior, and ordinary neighbours share a border
; of wall sectors, so plain adjacency shows a 2048x2048 overlap. It rejected six
; legitimate pairs at once.
;
; THE VIEW MATRIX'S TRANSLATION COLUMN IS THE CAMERA POSITION. It is not the
; -R*p a textbook view matrix carries. vid_setViewMatrix (tomb456.exe RVA
; 0x0000B960) scales the nine rotation terms by 1/16384, negates exactly the
; third row, and writes m[3], m[7] and m[11] through RAW. Using it as though it
; were -R*p produces an error that scales with world coordinates, so it behaves
; on a small level and fails on a large one.

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
