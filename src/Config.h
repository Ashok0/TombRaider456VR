// Config.h -- INI-backed tunables, read once at startup from TombRaiderVR.ini
// next to the DLL.
//
// The sign conventions are exposed deliberately. The engine renders Y-flipped
// (ogl_setPerspAngles writes e11 = -1/tanY, ogl_setScissor flips Y against
// gTargetHeight) and TR world space is Y-down, while OpenVR is Y-up in metres.
// That is two independent flips, and which way they compose is the single most
// likely thing to be wrong on first run -- so it is a config toggle rather than
// a recompile.
#pragma once

namespace tr {

struct Config {
    bool  enabled          = true;

    // Mono head-tracking bring-up mode.
    //
    // No stereo target, no FBO_default redirection, no draw duplication, no
    // compositor submission. The head pose is composed onto the game camera and
    // the frame renders normally to the monitor. The engine keeps its own
    // projection, so field of view is untouched.
    //
    // This is the mode to prove tracking, handedness and world scale in before
    // turning stereo on -- it isolates the view maths from everything else. It
    // also injects on every world-space pass rather than only backbuffer-
    // targeted ones, so it still works if the scene renders offscreen first.
    //
    // OpenVR is initialised as a Background app in this mode, so it reads poses
    // without becoming the VR scene application.
    bool  monoTracking     = false;

    // Rotation-only head tracking. The safest possible first test: the camera
    // can pivot but can never be displaced into geometry, so a wrong world
    // scale cannot put you inside a wall. Turn positional on once looking
    // around behaves.
    bool  positionalTracking = false;

    // Seated tracking origin rather than standing.
    //
    // This matters more than it looks. Standing space is absolute room
    // coordinates: your head is ~1.6 m off the floor, which at 423 units/m is
    // ~680 TR units of vertical offset applied to the camera before you have
    // moved at all. Seated space is relative to the seated zero, so at rest the
    // offset is ~0 and the view starts where the game put it.
    bool  seatedOrigin     = true;

    // TR world units per real-world metre. Lara is ~762 units tall and reads as
    // roughly 1.8 m, giving ~423. Raise it to shrink the world (and the
    // apparent IPD), lower it to grow the world.
    float worldUnitsPerMetre = 423.0f;

    // Multiplier on the eye separation ONLY, leaving world size alone.
    //
    // WorldUnitsPerMetre is the physically honest control: it changes apparent
    // size and stereo depth together, because they are the same thing. IpdScale
    // is the cheat -- it strengthens depth without shrinking the world. Reach
    // for it only if the scale feels right but the depth still reads flat.
    float ipdScale         = 1.0f;

    // Step per hotkey press, multiplicative. 1.25 = 25% a press, so three
    // presses roughly double. 5% steps proved far too small to judge against --
    // scale differences only become obvious at something like 2x.
    float scaleStep        = 1.25f;

    // Live tuning hotkeys. Numpad +/- adjust world scale, Numpad * / adjust
    // IpdScale, Numpad 0 resets both to the ini values. Every change is logged
    // so the value that felt right can be copied back into this file.
    // 0 disables a key.
    int   scaleUpKey       = 0x6B;   // VK_ADD       (numpad +)
    int   scaleDownKey     = 0x6D;   // VK_SUBTRACT  (numpad -)
    int   ipdUpKey         = 0x6A;   // VK_MULTIPLY  (numpad *)
    int   ipdDownKey       = 0x6F;   // VK_DIVIDE    (numpad /)
    int   resetTuningKey   = 0x60;   // VK_NUMPAD0

    // Per-eye render target size. 0 = ask OpenVR for the recommended size.
    int   eyeWidth         = 0;
    int   eyeHeight        = 0;
    float superSample      = 1.0f;

    // Sign conventions -- flip these first if the image is inverted or the eyes
    // are swapped. See the header comment.
    bool  flipProjectionY  = true;   // match the engine's e11 = -1/tanY
    bool  flipViewY        = true;   // OpenVR Y-up  ->  TR Y-down
    bool  swapEyes         = false;

    // Vertically flip the V texture bounds when handing the eye texture to the
    // compositor.
    //
    // Default OFF, and the reasoning matters. It is tempting to tie this to
    // FlipProjectionY on the grounds that "the engine renders Y-flipped", but
    // that gets it exactly backwards: the engine's e11 = -1/tanY exists to
    // CANCEL TR's Y-down world, so the result is an ordinary, correctly
    // oriented GL render with the origin at lower-left -- which is what the
    // compositor expects with default bounds. Inverting V on top of that turns
    // a correct image upside down.
    //
    // Symptom guide:
    //   upside down in the HMD but fine on the monitor .. this setting
    //   upside down in BOTH ......................... FlipProjectionY
    bool  flipSubmitV      = false;

    // Keep the 2D/ortho layer (HUD, menus, subtitles, fades) on the flat path
    // rather than reprojecting it. vid_setPass tells us which passes those are.
    bool  flatHud          = true;

    // Distance in metres at which the 2D layer (HUD, menus, subtitles) sits.
    // 0 = leave it alone, which double-visions: see VRSystem::HudNdcShiftX.
    float hudDepthMetres   = 4.0f;

    // Horizontal angular width of the 2D panel, in degrees, as seen from the
    // game camera. The engine's 2D layer fills the flat screen; mapped onto the
    // headset's ~94 degree field of view that would be overwhelming, so it is
    // placed on a panel of this width instead. Vertical follows the screen
    // aspect. Only used when hudLockToHead is false.
    float hudSizeDegrees   = 55.0f;

    // false (default): the 2D layer is a quad fixed in the GAME camera's frame,
    //   so it stays put in the world while your head turns. Built as
    //       Q = P_persp * E * L * P_o
    //   where L lifts ortho NDC onto a plane at hudDepthMetres, E is the per-eye
    //   head transform, and P_persp is the real per-eye projection.
    //
    // true: the old behaviour -- a flat per-eye NDC shift, so the panel is
    //   welded to your head and swings with every rotation. Kept because a few
    //   passes (fades, full-screen effects) genuinely want to be screen-locked.
    bool  hudLockToHead    = false;

    // Distance in metres for full-screen passes that bypass uProjMatrix.
    //
    // Five of the engine's shaders write gl_Position = vec4(aCoord, 1.0) --
    // straight to clip space, no matrix at all. Pre-rendered video cutscenes go
    // through that path, which is why they stay double-visioned when everything
    // else is fixed: identical in both eyes, no frustum shear, so the headset
    // optics pull the two copies apart.
    //
    // No matrix can move them, so the viewport is shifted per eye instead.
    // 0 disables the shift entirely.
    float videoDepthMetres = 6.0f;

    // false (default): the panel is anchored to the GAME camera. A viewport
    //   shift cannot reproject a quad, but it can put it where the panel centre
    //   belongs: transform a point straight ahead at videoDepthMetres by the eye
    //   transform, project it, and place the viewport there. Head rotation then
    //   moves it across your view, and it leaves view if you turn away -- which
    //   is what world-locked means. Roll and foreshortening are not
    //   representable this way, but for a flat video panel they do not read.
    //
    // true: welded to your head, the previous behaviour.
    bool  videoLockToHead  = false;

    // Horizontal angular width of the video panel, in degrees, seen from the
    // game camera.
    //
    // This also decides how much the panel can distort. Projecting only the
    // panel's CENTRE gives translation and nothing else, so the quad keeps a
    // constant angular size wherever it sits -- at the edge of a ~94 degree
    // field of view it should shrink and foreshorten, and instead it stretches.
    // Projecting the four CORNERS and fitting the viewport to their bounding
    // box recovers the size term, so the panel behaves like real geometry.
    //
    // What remains is keystone: a quad seen off-axis should go trapezoidal, and
    // a bounding box stays rectangular. That error grows with panel width, so a
    // narrower panel is also a flatter-looking one. 50-70 is a good cinema size;
    // much above 90 and the keystone starts to show at the edges.
    float videoSizeDegrees = 60.0f;

    // Capture the video pass into an offscreen texture and replay it as a real
    // quad, instead of shifting the viewport.
    //
    // The viewport fit leaves keystone: an off-axis quad should be trapezoidal
    // and a bounding box is rectangular. Replaying as actual geometry removes
    // that completely -- the quad keystones, foreshortens and rolls exactly like
    // the world, because it IS world geometry.
    //
    // Costs one framebuffer bind and two quad draws per video frame, and only
    // during video: the branch is gated on a pass having no uProjMatrix at all,
    // which never happens in gameplay. It also rasterises the video once rather
    // than twice, which nearly pays for itself. Falls back to the viewport fit
    // if the shader API or the framebuffer is unavailable.
    bool  videoOffscreen   = true;

    // Flip the captured video vertically on replay. Only needed if the cutscene
    // comes out upside down.
    bool  videoFlipV       = false;

    // Alternate-eye rendering for TR6 (Angel of Darkness).
    //
    // TR4 and TR5 draw straight to the backbuffer, so every world draw can be
    // issued twice into the two halves of one double-wide target. TR6 cannot
    // work that way: it renders the scene into the engine's own 2560x1440
    // offscreen textures and composites at the end, and the composite samples
    // those whole with 0-1 UVs. Widening them would break that sampling, and
    // duplicating individual draws into halves of a target the composite reads
    // entire would corrupt it. Measured: 572970 of 579945 world draws offscreen,
    // against roughly 9% in TR4/5.
    //
    // So each FRAME is rendered entirely as one eye, mono, through the engine's
    // normal path, and lands in that eye's half of the stereo target. The other
    // half keeps the previous frame. Nothing is resized and nothing is
    // duplicated.
    //
    // The cost is honest and unavoidable: each eye updates at half the frame
    // rate. At 90 Hz that is 45 Hz per eye. Full-rate stereo would mean running
    // the whole offscreen chain twice into parallel target sets, which is a much
    // larger piece of work.
    bool  alternateEyeGame6 = true;

    // How many offscreen world draws a frame needs before alternate-eye engages.
    //
    // AER is only worth its half-rate cost when there is an offscreen 3D scene
    // to reach. The TR6 main menu and the FMVs are 2D: they draw straight to the
    // backbuffer, so the ordinary per-draw duplication handles them at FULL rate
    // and looks better doing it. Gameplay runs ~800 offscreen world draws a
    // frame; menus and video are near zero, so the count separates them cleanly
    // with no extra symbols to find.
    //
    // The decision is latched once per frame from the previous frame's count, so
    // it stays stable for the whole frame and costs one frame at a transition.
    int   alternateEyeMinOffscreen = 50;

    // Do not use the offscreen video panel in TR6.
    //
    // TR6 renders the scene into custom offscreen targets and composites to the
    // backbuffer through a clip-space-direct shader -- indistinguishable from an
    // FMV quad by the uid[0] < 0 test. Capturing it turns the whole game into a
    // small floating panel. TR6's actual FMVs use the viewport-shift path
    // instead, which suits a full-screen video regardless.
    bool  videoSkipGame6   = true;


    // Negate Y when lifting the 2D layer onto the panel. Leave at 1.
    //
    // The engine's ortho matrix already flips Y, because TR's 2D space is
    // Y-down, so P_o emits ordinary GL NDC with Y up. The per-eye projection is
    // built with FlipProjectionY and expects Y-DOWN input, so feeding it Y-up
    // flips the image a second time -- and reverses triangle winding, which
    // backface culling then removes entirely. That reads as "the HUD vanished
    // except the one element drawn with culling off, and that one is upside
    // down". This cancels the extra flip.
    bool  hudFlipY         = true;

    // Duplicate every world-space draw into both viewport halves. Turning this
    // off leaves per-eye matrix injection running but draws once -- useful for
    // isolating whether a problem is in the matrices or in the duplication.
    bool  duplicateDraws   = true;

    // Mirror the left eye back to the game window.
    bool  mirrorToWindow   = true;

    float nearClip         = 0.0f;   // 0 = keep whatever the engine set
    float farClip          = 0.0f;

    bool  verboseFirstFrame = true;

    // Present the Oculus Touch controllers to the game as an Xbox pad.
    //
    // The game resolves XInput through GetProcAddress into a function-pointer
    // global, so there is no import to hook and no virtual-pad driver needed --
    // we overwrite that pointer. inputUpdate() polls it every frame and switches
    // app.input_type to INPUT_TYPE_XB on the first input, so the game adopts the
    // Xbox control scheme and prompts by itself. A real pad, if plugged in, is
    // merged rather than replaced.
    bool  gamepadEnabled   = true;

    // Log the raw OpenVR legacy button masks whenever they change. Touch's
    // button ids differ between runtimes, so if something lands in the wrong
    // place this says which mask it actually set.
    bool  gamepadLogButtons = false;

    // Which XInput button the left hand's lower face button sends.
    //
    // true  = BACK  -- the System menu. This is the default.
    // false = START -- the pause/inventory menu.
    //
    // inputUpdate() decodes BACK to internal key 0x62 and START to 0x63; which
    // one a given game screen treats as "System" lives in the game DLLs, so this
    // stays switchable.
    bool  gamepadMenuUsesBack = true;

    // Draw every room in the level, defeating TR4's portal culling.
    //
    // The engine only submits rooms reached through a camera-facing portal, so
    // looking away from the game camera finds nothing drawn. The culling is in
    // tomb4.dll -- the exe has none, and widening the projection it hands the
    // game was measured to change nothing.
    //
    // This appends every room to the draw list after traversal (exactly what the
    // engine already does for rooms flagged 0x40000) and gives them full-screen
    // clip rects. TR4 only: the addresses are that DLL's.
    //
    // Cost: no visibility culling at all. TR4 rooms are small, but a large level
    // will cost frame rate. Off by default -- turn it on and watch the log line
    // that reports how many rooms were forced.
    // Expand the engine's visible set along PORTAL CONNECTIVITY by this many
    // hops. 0 = off. This is the mechanism that fixes geometry going missing
    // when you turn your head.
    //
    // The engine's traversal runs in the GAME CAMERA's space -- VR is injected
    // at the shader uniform, so the engine's own view matrix never rotates with
    // your head. A portal beside or behind the camera fails the near-plane test
    // inside the clipper, so no amount of widening its rectangle helps (that
    // was measured: 200% of screen each way changed nothing). Connectivity has
    // no orientation bias: a room through the door behind you is one hop away
    // whichever way the camera faces.
    //
    // 2 is conservative, 3 covers deeper sightlines and costs about ten more
    // rooms and no measurable frame rate. Deeper than that is where rooms start
    // appearing that you cannot actually see -- which is what the head test
    // below exists to catch.
    int   portalHops       = 3;

    // Room indices hop expansion must never add. Comma or space separated.
    // Per level, so a list that helps one means nothing in another.
    int   excludeRooms[64] = {};
    int   excludeCount     = 0;

    // Before adding a hop room, test the portal we would reach it through
    // against the ACTUAL HEADSET FRUSTUM.
    //
    // Hop expansion adds any portal-connected room and draws it WITHOUT a
    // portal clip, so a room that is connected but not actually visible through
    // that doorway still gets drawn -- and if it happens to share world space
    // with somewhere you can see, you get foreign geometry laid over your own.
    // That is the room-215 class of bug.
    //
    // The engine performs exactly this test already; it just performs it from
    // the game camera. Doing it from the head is the piece that was missing.
    // The portal's four corners go world -> eye -> clip using the view matrix
    // we actually injected and the VR projection, so handedness and field of
    // view come from the projection rather than being restated here.
    // DEFAULT OFF until it is proven. The first version transformed portals
    // with the view matrix's translation column, which is not what a textbook
    // view matrix carries; the error scaled with world coordinates, so it
    // behaved on a 116-room level and rejected essentially every portal on a
    // 242-room one. Fixed to use rotation only, with the camera position taken
    // from the engine's own globals -- but unproven, so opt in.
    bool  portalHeadTest   = false;

    // How far outside the frustum a portal may sit and still count as visible,
    // in NDC (0.35 = 35% of half-width). Generous on purpose: losing geometry
    // is the worse failure, so raise this if anything vanishes at the edges and
    // lower it if unwanted rooms get through.
    float portalHeadMargin = 0.35f;

    // Give every listed room a full-screen clip rect. Leave on.
    //
    // Required, not lazy: the engine's portal-clipped rects are screen boxes
    // computed for the game camera's view, and we render from the HMD's, so
    // under head rotation they scissor the wrong part of the screen. Off is a
    // diagnostic that separates "the list changed" from "the rects changed".
    bool  drawAllRoomsClip = true;

    // Legacy: append rooms by DISTANCE from the camera, ignoring portals.
    //
    // Superseded by portalHops and kept only for A/B. Proximity is the wrong
    // criterion -- it will happily add a stacked room that shares world space
    // with the one you are standing in and that no portal reaches.
    bool  drawAllRooms     = false;

    // Log the DLL-side return address of each distinct call into vid_setPass and
    // ogl_drawVB, as "module+RVA".
    //
    // The game DLLs ship without PDBs, so this is how we locate their render
    // code without searching 1791 unnamed functions: the DLL has to call across
    // into the engine to draw, and the return address at our hook is a code
    // address inside the DLL. One gameplay frame gives the exact RVAs to open in
    // Ghidra. Deduplicated and capped, but still verbose -- leave off for play.
    bool  logCallsites     = false;

    // Frame-graph tracer. Logs every render-target transition and how many
    // world-space vs 2D draws happen against each, for TraceFrames frames
    // starting at TraceStartFrame.
    //
    // This is how you find out where the 3D scene is actually drawn. Stereo
    // splits draws into two halves of one target and redirects FBO_default, and
    // both of those are only correct if the scene really is rendered to the
    // backbuffer rather than into one of the engine's offscreen array textures
    // and composited afterwards. Measure before assuming.
    int   traceFrames      = 0;

    // Virtual-key code that triggers a capture, so the trace can be taken
    // during actual gameplay instead of at a guessed frame number. Default
    // 0x79 = F10. Set to 0 to disable and use traceStartFrame instead.
    int   traceKey         = 0x79;

    // Fallback frame-number trigger. 0 = only the hotkey fires a capture.
    int   traceStartFrame  = 0;

    // Split the two independent sources of per-eye difference so they can be
    // tested apart. Both default on; turning either off is a diagnostic.
    //
    //   parallax  -- the +-13.4 unit view offset. THIS is depth.
    //   shear     -- proj[8] = -+0.2425, the asymmetric frustum. This is a
    //                constant sideways shift and carries NO depth at all.
    //
    // Every measurement so far confirms only that the halves DIFFER, which the
    // shear alone is enough to cause. Isolating them says which one is real:
    //
    //   PerEyeView=1 PerEyeProjection=0 -> if depth appears, the shear was
    //       fighting the parallax, and that configuration is the fix.
    //   PerEyeView=0 PerEyeProjection=1 -> if the halves still differ, the
    //       difference is pure shear and parallax is not reaching the pixels.
    //   PerEyeView=1 PerEyeProjection=1 -> current behaviour.
    bool  perEyeView       = true;

    // 0 = the engine's own projection. NOT a clean control: it also drops the
    //     HMD field of view and feeds a 1.78-aspect frustum into a 0.93-aspect
    //     viewport, so it changes two things at once and proves nothing about
    //     the shear on its own.
    // 1 = the HMD's true asymmetric frustum (correct, and the default).
    // 2 = the HMD's field of view, symmetrised: same total extent per axis,
    //     shear forced to zero. This is the clean single-variable test, because
    //     FOV and aspect stay exactly as in mode 1.
    //
    // Mode 2 exists because the shear contributes a CONSTANT disparity of
    // (r+l)/(r-l) - the pedestal - which for this HMD is 0.485 NDC, about 326 px.
    // The headset optics are meant to cancel it exactly. The depth cue riding on
    // top is only ~32 px at 2000 units. If that cancellation is not happening,
    // the eyes converge on the pedestal and everything collapses to one plane --
    // flat, oversized, and immune to IpdScale.
    int   perEyeProjection = 1;

    // Where the per-eye TRANSLATION is applied.
    //
    //   0 = uViewMatrix translation column (the original).
    //       BROKEN: 38 of the engine's 125 vertex shaders transform position as
    //       dot(uViewMatrix[i].xyz, p.xyz) -- rotation ONLY, never reading the
    //       translation column. For those passes the camera offset is baked into
    //       uModelMatrix on the CPU instead. So a per-eye translation here is
    //       invisible to them: no parallax, no depth, everything at infinity (so
    //       it reads oversized), IpdScale and WorldUnitsPerMetre inert. A per-eye
    //       ROTATION was always visible, which is why a 20-degree yaw test worked
    //       while every translation test failed.
    //
    //   1 = uModelMatrix translation column.
    //       ALSO BROKEN, differently: skinned geometry is transformed by
    //       uJoints[72*3], not uModelMatrix, so characters do not move with the
    //       world and end up floating outside the map. Chasing every transform
    //       path is a losing game.
    //
    //   3 = uProjMatrix composed with the WHOLE per-eye transform, and the view
    //       matrix left completely untouched. THE FIX.
    //
    //       With the game's own view matrix in place both shader families agree
    //       exactly: full-dot gives R_g*w + t_g, and rotation-only gives R_g*p
    //       with p = w + R_g^T*t_g, which is the same R_g*w + t_g. They only
    //       diverge once the view matrix is modified. So don't modify it --
    //       fold the eye transform E = [R_e | t_e] into the projection:
    //           clip = P * (R_e*v + t_e) = (P * E) * v
    //       One multiply, rotation and translation together, correct for both
    //       families by construction.
    //
    //       Mode 2's head-movement blow-up came from d = (R_e - I)*t_g + t_e:
    //       t_g is ~82000 units, so any head rotation made the first term
    //       thousands of units. E is metres-scale, so this cannot happen here.
    //
    //   2 = uProjMatrix, post-multiplied by translate(d). Gives correct depth and
    //       working scale keys, but the world swims when you rotate your head,
    //       for the reason above. Superseded by 3.
    //       Every one of the 125 shaders ends with uProjMatrix * vec4(view, 1.0),
    //       so this is the one matrix nothing can bypass -- joints included. A
    //       view-space shift d is exactly equivalent to P * translate(d):
    //           clip.x += m[0]*dx + m[4]*dy + m[8]*dz
    //       which yields the same delta/depth parallax as shifting the view.
    int   eyeOffsetMode = 2;

    // Yaw the RIGHT eye's view by this many degrees. Diagnostic only.
    //
    // Every link from the injection to the draw has been verified correct, and
    // the headset still shows no parallax. One of those verifications must be
    // wrong, so this stops arguing about a 26-unit translation and applies an
    // effect that cannot be subtle or misjudged: at 20 degrees the two halves
    // are looking in visibly different directions.
    //
    //   halves differ wildly -> the view matrix DOES drive rendering, and the
    //       fault is specific to the translation column.
    //   halves stay identical -> the view matrix reaches the GPU (proved by
    //       glGetUniformfv) but does not affect the draw at all, and the whole
    //       injection point is wrong however correct it reads.
    float debugEyeYawDegrees = 0.0f;

    // Burn a red bar into the left half of the eye target and a blue bar into
    // the right half. A direct eye-mapping check for when every measurable
    // thing upstream reads correct and the headset still shows mono. See
    // StereoRenderer::MarkEyes for how to read the result.
    bool  eyeMarkers       = false;
};

const Config& Cfg();
void LoadConfig(const wchar_t* iniPath);

// --- live tuning ------------------------------------------------------------
// Scale is a perceptual judgement, so it is adjustable in the headset rather
// than through an edit-restart cycle. These start from the ini values.
float LiveWorldUnitsPerMetre();
float LiveIpdScale();
void  AdjustWorldScale(float factor);   // multiplicative, e.g. 1.05f
void  AdjustIpdScale(float factor);
void  LogTuning(const char* why);

// Log any ini option that the current EyeOffsetMode silently ignores.
void  WarnIgnoredOptions();
void  ResetTuning();

} // namespace tr
