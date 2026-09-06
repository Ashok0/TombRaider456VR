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

    // Duplicate every world-space draw into both viewport halves. Turning this
    // off leaves per-eye matrix injection running but draws once -- useful for
    // isolating whether a problem is in the matrices or in the duplication.
    bool  duplicateDraws   = true;

    // Mirror the left eye back to the game window.
    bool  mirrorToWindow   = true;

    float nearClip         = 0.0f;   // 0 = keep whatever the engine set
    float farClip          = 0.0f;

    bool  verboseFirstFrame = true;

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
};

const Config& Cfg();
void LoadConfig(const wchar_t* iniPath);

} // namespace tr
