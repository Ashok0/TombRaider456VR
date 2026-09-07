// Hooks.cpp -- the stereo injection layer.
//
// Five hooks. Two of them do the VR work; three are plumbing that stereo
// cannot function without.
//
//   vid_setPass     REQUIRED WORK. Classifies each pass as world-space or 2D by
//                   reading which matrix vid_state.proj was pointed at. This is
//                   the only signal in the engine that distinguishes "3D scene"
//                   from "HUD/menu/subtitle", and it is the gate for whether a
//                   pass gets per-eye matrices at all.
//
//   validate_draw   REQUIRED WORK. The single choke point where every uniform
//                   reaches the GPU. Per-eye projection and view are written
//                   into mProj[1] / mView_packed here, the dirty bits are
//                   forced, the original uploads, and the engine's own matrices
//                   are put back so it never observes the substitution.
//
//   ogl_draw        PLUMBING. validate_draw cannot issue a draw -- its two
//   ogl_drawVB      callers do, immediately after it returns. Duplicating the
//                   draw per eye therefore has to happen one level up.
//
//   ogl_present     PLUMBING. The frame boundary: submit to the compositor,
//                   mirror to the window, then WaitGetPoses for the next frame.
//
// The engine is never asked to render the scene twice. Each draw call is issued
// twice into the two halves of one double-wide target, with different matrices
// and a different viewport. That needs no shader changes, which matters: this
// build has 202 shader pairs and a GL 3.2 Core context, so GL_OVR_multiview2
// would mean editing 404 GLSL sources.
#include "Hooks.h"
#include "Engine.h"
#include "StereoMath.h"
#include "Config.h"
#include "GL.h"
#include "Log.h"
#include "InlineHook.h"
#include "StereoRenderer.h"
#include "VideoPanel.h"
#include "Gamepad.h"
#include "Callsite.h"
#include "RoomCull.h"
#include "VRSystem.h"

#include <cstring>
#include <intrin.h>
#include <cmath>

namespace tr {
namespace {

// --- hook objects -----------------------------------------------------------
hook::InlineHook g_hSetPass;
hook::InlineHook g_hValidateDraw;
hook::InlineHook g_hDraw;
hook::InlineHook g_hDrawVB;
hook::InlineHook g_hPresent;
hook::InlineHook g_hFmvShow;

typedef void(__cdecl* Fn_vid_setPass)(int shader, float* params, int cull, int blend);
typedef void(__cdecl* Fn_validate_draw)();
typedef void(__cdecl* Fn_ogl_draw)(void* vb, unsigned firstIndex, unsigned count, int strip);
typedef void(__cdecl* Fn_ogl_drawVB)(int fvf, void* vb, void* ib, int stride,
                                     unsigned first, unsigned count, int strip);
typedef void(__cdecl* Fn_ogl_present)();

// --- verified prologue bytes ------------------------------------------------
// Read out of Ghidra's disassembly of this exact build. Install() refuses to
// patch if these do not match, so a game update degrades to "VR did not start"
// rather than a corrupted instruction stream.

// 48 8B C4              mov  rax, rsp
// 48 81 EC 88 00 00 00  sub  rsp, 0x88          -> 10 bytes, both PIC
const uint8_t kSetPassPrologue[]  = { 0x48, 0x8B, 0xC4, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00 };

// 40 53                 push rbx
// 48 83 EC 30           sub  rsp, 0x30          -> 6 bytes, both PIC
const uint8_t kValidatePrologue[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x30 };

// 48 89 5C 24 08        mov  [rsp+8], rbx       -> 5 bytes, PIC
const uint8_t kDrawPrologue[]     = { 0x48, 0x89, 0x5C, 0x24, 0x08 };

// 48 83 EC 28           sub  rsp, 0x28
// 33 C9                 xor  ecx, ecx           -> 6 bytes, both PIC
// (the next instruction is a RIP-relative CMP, so we stop here)
const uint8_t kPresentPrologue[]  = { 0x48, 0x83, 0xEC, 0x28, 0x33, 0xC9 };

// MOV RAX,RSP ; MOV [RAX+8],RBX  -- 3 + 4 = 7 bytes, no RIP-relative operand.
const uint8_t kFmvShowPrologue[]  = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08 };

// --- per-frame state --------------------------------------------------------

bool  g_worldPass    = false;   // set by the vid_setPass hook
int   g_currentEye   = 0;
bool  g_inDuplicate  = false;   // guards against recursive duplication
bool  g_ready        = false;   // GL objects created, VR live
bool  g_loggedFrame  = false;
// One-shot per-eye diagnostics. The single-shot log only ever showed eye 0,
// which hid the only question that matters: do the two eyes actually differ?
bool  g_loggedEye[2] = { false, false };
float g_eyeViewT[2][3] = {};

// The world -> eye matrix last injected, for RoomCull's head test.
Affine g_lastHeadView      = Affine::Identity();
bool   g_lastHeadViewValid = false;
bool  g_reportedSeparation = false;
// Per-eye injection counts. If eye 1 never gets injected, the duplication loop
// is not reaching validate_draw with the right eye selected.
unsigned g_injectCount[2] = { 0, 0 };
unsigned g_dupCount = 0;
unsigned g_classifyMismatch = 0;   // live vs cached world-pass disagreements
unsigned g_lastReportFrame = 0;
// Wall-clock at the last health report, for a real frame-rate figure. Under
// alternate-eye each eye updates at HALF this, so it is the number that decides
// whether the judder is worth chasing.
LARGE_INTEGER g_lastReportTime = {};
unsigned g_prevDup = 0, g_prevInj0 = 0, g_prevInj1 = 0;

// World-space draws seen at validate_draw, counted per eye-pass, so a
// duplicated draw contributes two. This is the honest denominator for the
// health report. The previous one divided injections by duplications, but
// injection and duplication are gated on the SAME condition, so it read 100%
// by construction and could never detect the failure it warned about.
unsigned g_worldDraws     = 0;
unsigned g_worldOffscreen = 0;   // world draws skipped: target was not the backbuffer
unsigned g_prevWorld = 0, g_prevOffscreen = 0;

// GPU-side verification of the per-eye view matrix. Re-armed at every health
// report rather than latched once, so it tracks live IpdScale and
// WorldUnitsPerMetre changes instead of only ever sampling frame 1.
// Shader ids seen with no uProjMatrix at all -- the clip-space-direct passes.
// Logged once each so that if something still refuses to fuse, the log names
// the shader instead of costing another play session to find.
int   g_bypassSeen[8]  = { -1, -1, -1, -1, -1, -1, -1, -1 };

// Alternate-eye rendering: which eye this whole frame belongs to.
int  g_frameEye = 0;
bool g_loggedAltEye = false;

// Offscreen world draws, this frame and last. This is how we tell TR6 gameplay
// (an offscreen 3D scene, ~800 a frame) from its menus and FMVs (2D, near zero).
unsigned g_offscreenWorld     = 0;
unsigned g_offscreenWorldPrev = 0;

// Latched once per frame. AlternateEyeActive() is consulted from the draw path,
// the injection gate and present, so it MUST NOT change mid-frame -- flipping
// halfway would leave the frame split across two targets.
bool g_aerLatched = false;

bool AlternateEyeCapable() {
    return Cfg().alternateEyeGame6
        && !Cfg().monoTracking
        && CurrentGame() == 2
        && Stereo().monoValid();
}

// TR6 renders its scene offscreen, so per-draw duplication cannot reach it.
// Each frame is one eye instead -- but only while there IS an offscreen scene.
// See Config::alternateEyeMinOffscreen.
bool AlternateEyeActive() {
    return g_aerLatched;
}

bool  g_verifyArmed    = true;
bool  g_verifyDone[2]  = { false, false };
float g_gpuViewT[2][3] = {};
int   g_gpuProg[2]     = { 0, 0 };
int   g_gpuLoc[2]      = { -1, -1 };

// Histogram of the actual vid_state.proj pointers seen at draw time.
// The world/2D test assumes proj is only ever &mProj[0] or &mProj[1]; if the
// engine points it somewhere else during gameplay the test silently says "2D"
// for everything and no per-eye matrices are ever applied.
struct ProjSeen { const void* ptr; unsigned count; };
ProjSeen g_projSeen[6] = {};

void NoteProjPointer(const void* p) {
    for (auto& e : g_projSeen) {
        if (e.ptr == p) { ++e.count; return; }
        if (e.ptr == nullptr) { e.ptr = p; e.count = 1; return; }
    }
}

void LogProjHistogram() {
    const void* ortho = &Proj()[0];
    const void* persp = &Proj()[1];
    LogF("proj pointers: mProj[0](ortho)=%p  mProj[1](persp)=%p", ortho, persp);
    for (const auto& e : g_projSeen) {
        if (!e.ptr) break;
        const char* what = (e.ptr == ortho) ? "ortho/2D"
                         : (e.ptr == persp) ? "perspective/world"
                         : "UNRECOGNISED";
        LogF("  proj=%p  draws=%-7u  %s", e.ptr, e.count, what);
    }
}
GLuint g_realDefaultFbo = 0;
unsigned g_frameIndex = 0;

// Deferred OpenVR bring-up (see BringUpVR).
unsigned g_vrAttempts        = 0;
unsigned g_vrLastAttemptFrame = 0;
bool     g_vrGaveUp          = false;
bool     g_loggedStereoFrame = false;

// --- frame-graph tracer -----------------------------------------------------
// Answers the one question stereo depends on: where is the 3D scene actually
// drawn? Reads the ogl_rt latch before each draw and logs every transition,
// with how many world-space vs 2D draws happened against each target. No extra
// hook needed -- validate_draw already runs before every single draw.
struct TraceRun {
    int32_t colorId = -99, depthId = -99, colorIndex = -99;
    GLint   fbo     = -1;
    int     world   = 0;
    int     flat    = 0;
};
TraceRun g_run;
bool     g_tracing        = false;
int      g_traceEmitted   = 0;
int      g_frameWorld     = 0;   // per-frame totals, to tell gameplay from a menu
int      g_frameFlat      = 0;
unsigned g_traceUntilFrame = 0;  // set by the hotkey
bool     g_traceKeyWasDown = false;

// Poll the capture hotkey. Guessing a frame number is unreliable -- frame 600
// can easily still be a title screen -- so the trace is armed by a keypress
// while the game is actually being played.
void PollTraceKey() {
    const auto& c = Cfg();
    if (c.traceKey == 0 || c.traceFrames <= 0) return;

    const bool down = (GetAsyncKeyState(c.traceKey) & 0x8000) != 0;
    if (down && !g_traceKeyWasDown) {
        g_traceUntilFrame = g_frameIndex + static_cast<unsigned>(c.traceFrames);
        LogF("trace: hotkey pressed, capturing %d frame(s)", c.traceFrames);
    }
    g_traceKeyWasDown = down;
}

// --- live tuning hotkeys ----------------------------------------------------
bool g_tuneWasDown[5] = { false, false, false, false, false };

void PollTuningKeys() {
    const auto& c = Cfg();
    const int keys[5] = { c.scaleUpKey, c.scaleDownKey,
                          c.ipdUpKey,   c.ipdDownKey, c.resetTuningKey };
    const float step = c.scaleStep;
    for (int i = 0; i < 5; ++i) {
        if (keys[i] == 0) continue;
        const bool down = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
        if (down && !g_tuneWasDown[i]) {
            switch (i) {
            case 0: AdjustWorldScale(step);        break;  // smaller world, stronger depth
            case 1: AdjustWorldScale(1.0f / step); break;  // bigger world, weaker depth
            case 2: AdjustIpdScale(step);          break;
            case 3: AdjustIpdScale(1.0f / step);   break;
            case 4: ResetTuning();                 break;
            }
        }
        g_tuneWasDown[i] = down;
    }
}

bool TraceActive() {
    const auto& c = Cfg();
    if (c.traceFrames <= 0) return false;
    if (g_frameIndex < g_traceUntilFrame) return true;
    if (c.traceStartFrame > 0) {
        const unsigned start = static_cast<unsigned>(c.traceStartFrame);
        return g_frameIndex >= start &&
               g_frameIndex < start + static_cast<unsigned>(c.traceFrames);
    }
    return false;
}

void FlushTraceRun() {
    if (g_run.world == 0 && g_run.flat == 0) return;
    LogF("  target color=%-3d depth=%-3d layer=%-2d glFBO=%-3d size=%dx%d "
         "| draws world=%-4d flat=%-4d",
         g_run.colorId, g_run.depthId, g_run.colorIndex, g_run.fbo,
         TargetWidth(), TargetHeight(), g_run.world, g_run.flat);
    ++g_traceEmitted;
    g_run.world = g_run.flat = 0;
}

// Called from validate_draw, before every draw.
void TraceDraw(bool worldPass) {
    if (!g_tracing) return;

    const OglRenderTarget& rt = Rt();
    if (rt.color_id != g_run.colorId || rt.depth_id != g_run.depthId ||
        rt.color_index != g_run.colorIndex) {
        FlushTraceRun();
        GLint fbo = -1;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        g_run.colorId    = rt.color_id;
        g_run.depthId    = rt.depth_id;
        g_run.colorIndex = rt.color_index;
        g_run.fbo        = fbo;
    }
    if (worldPass) { ++g_run.world; ++g_frameWorld; }
    else           { ++g_run.flat;  ++g_frameFlat;  }
}

// Saved copies of the engine's own matrices, restored after each upload so the
// engine never sees our per-eye substitution.
mat4 g_savedProj{};
mat4 g_savedView{};
mat4 g_savedModel{};
mat4 g_savedHud{};
bool g_modelPatched = false;

bool VrLive() {
    if (!Cfg().enabled || !VR().active() || !g_ready) return false;
    // Mono owns no render target, so it has no stereo objects to require.
    return Cfg().monoTracking || Stereo().valid();
}

// The engine draws its post-processing chain into its own offscreen array
// textures before compositing to the backbuffer. Only the backbuffer-targeted
// work should be split per eye; an offscreen pass wants its own full viewport.
//
// ogl_rt is the render-target latch ogl_setRenderTarget writes; colour and
// depth both zero is the "back to the default framebuffer" case.
// vid_setPass assigns vid_state.shader before its own range check, so anything
// indexing shaders[] has to clamp first -- the array is only 0xCA entries.
bool vs_shader_in_range(int shader) {
    return shader >= 0 && shader <= 0xC9;
}

// True while the video pass is being captured offscreen, so the per-eye
// machinery stands aside and lets it render plainly.
bool g_inVideoCapture = false;

// A pass with no uProjMatrix at all writes clip space directly and cannot be
// moved by any uniform. That is the video path.
// Is the offscreen video panel actually going to handle this frame's bypass
// passes? Both the capture path and the viewport-shift fallback must agree: if
// they disagree, a bypass pass gets NEITHER, which is head-locked and unfused.
// That is exactly what VideoSkipGame6 caused in TR6 -- capture correctly
// skipped, fallback still suppressed by "the panel exists".
// Frame on which fmvShow was last called. The engine calls it once per frame
// for as long as a video is on screen.
unsigned g_fmvFrame = 0;

bool FmvActive() {
    return g_fmvFrame != 0 && (g_frameIndex - g_fmvFrame) <= 2;
}

bool VideoOffscreenActive() {
    if (!Cfg().videoOffscreen || !Video().valid()) return false;

    // TR6's scene composite goes through a clip-space-direct shader, the same
    // signature as an FMV quad, so the shader test alone would capture the whole
    // game. Require an actual video there. TR4/5 keep the original behaviour,
    // which is working.
    if (CurrentGame() == 2 && Cfg().videoSkipGame6 && !FmvActive()) return false;

    return true;
}

bool IsBypassPass() {
    const int sid = VidState().shader;
    return vs_shader_in_range(sid) && Shaders()[sid].uid[0] < 0;
}

// Model-view-projection for the video quad in one eye: the unit quad is scaled
// to the panel, placed at videoDepthMetres in the game camera's frame, and then
// put through the real per-eye transform and projection.
void BuildVideoMvp(int eye, mat4& out) {
    const Eye e = (eye == 0) ? Eye::Left : Eye::Right;
    const float Z = Cfg().videoDepthMetres * LiveWorldUnitsPerMetre();

    float zn = 16.0f, zf = 32768.0f;
    ExtractNearFar(Proj()[1], zn, zf);
    if (Z <= zn) zn = Z * 0.5f;
    if (Z >= zf) zf = Z * 2.0f;

    mat4 P{};
    VR().EyeProjection(e, zn, zf, P);

    const float halfFov = Cfg().videoSizeDegrees * 0.5f * 3.14159265358979f / 180.0f;
    const float W = Z * std::tan(halfFov);
    const int   sw = ScreenWidth();
    const int   sh = ScreenHeight();
    const float aspect = (sh > 0) ? (float)sw / (float)sh : 1.7778f;
    const float H = (aspect > 0.0f) ? W / aspect : W;

    // Unit quad -> panel. Y negated for the same reason as the HUD: the per-eye
    // projection is built for Y-down input (see Config::hudFlipY).
    mat4 L{};
    L.m[0]  = W;
    L.m[5]  = Cfg().hudFlipY ? -H : H;
    L.m[10] = 1.0f;
    L.m[14] = -Z;
    L.m[15] = 1.0f;

    out = Mul4(P, Mul4(AffineToMat4(VR().EyeView(e)), L));
}

bool TargetIsBackbuffer() {
    const OglRenderTarget& rt = Rt();
    return rt.color_id == 0 && rt.depth_id == 0;
}

// Read uViewMatrix back off the live GL program.
//
// The in-memory readback in the injection proves our write landed in
// mView_packed. It does NOT prove the engine uploaded it: validate_draw only
// uploads when consts has kView set and shaders[].uid[1] >= 0, and everything
// downstream of that is invisible from the CPU side. Asking the GPU what it
// actually holds is the only way to tell a failed upload from a failed
// viewport, target or submit -- which is the whole remaining question when the
// eyes are provably different in memory and still identical on screen.
//
// uViewMatrix is declared `uniform vec4 uViewMatrix[4]` and uploaded with
// glUniform4fv(loc, 4, ...), so each array element carries its own location.
// The translation lives in the .w of the first three rows; that is the part
// that differs between the eyes and the only part worth reading.
void ReadBackViewTranslation(uint32_t program, int loc, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!gl::GetUniformfv || program == 0 || loc < 0) return;
    for (int row = 0; row < 3; ++row) {
        float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        gl::GetUniformfv(program, loc + row, v);
        out[row] = v[3];
    }
}

// uProjMatrix is a real mat4, so one location returns all 16 floats. In
// EyeOffsetMode 3 the per-eye transform lives here rather than in the view
// matrix, so this is what has to be verified.
void ReadBackProjTranslation(uint32_t program, int loc, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!gl::GetUniformfv || program == 0 || loc < 0) return;
    float m[16] = {};
    gl::GetUniformfv(program, loc, m);
    out[0] = m[12];
    out[1] = m[13];
    out[2] = m[14];
}

// ---------------------------------------------------------------------------
// vid_setPass -- pass classification
// ---------------------------------------------------------------------------
void __cdecl Detour_vid_setPass(int shader, float* params, int cull, int blend) {
    if (Cfg().logCallsites) NoteCallsite("vid_setPass", _ReturnAddress());

    // The engine has a real robustness gap here: vid_setPass assigns
    // vid_state.shader = shader BEFORE its range check, and a shader id above
    // 201 then skips all configuration and falls through, leaving validate_draw
    // to index shaders[] out of bounds. We do not synthesise pass ids, but
    // since we are already in the path it costs nothing to notice.
    if (shader < 0 || shader > 0xC9) {
        LogF("setPass: out-of-range shader id %d (engine would index shaders[] OOB)", shader);
    }

    g_hSetPass.Original<Fn_vid_setPass>()(shader, params, cull, blend);

    // After the original runs, vid_state.proj points at mProj[0] for the 2D
    // ortho layer (logos, menus, subtitles, fades, the title art in cases
    // 0x3B-0x3D) and at mProj[1] for world-space 3D.
    //
    // Some cases never assign proj at all (0x3F, 0x40, 0x48, 0xC0, 0xC1,
    // 0xC4-0xC7) and inherit whatever the previous pass left, so reading the
    // pointer after the call is the only correct way to classify -- a static
    // table of shader ids would get the sticky ones wrong.
    g_worldPass = IsWorldPass();
}

// ---------------------------------------------------------------------------
// validate_draw -- per-eye matrix injection
// ---------------------------------------------------------------------------
void __cdecl Detour_validate_draw() {
    // Mono redirects no targets and resizes no viewport, so it can and should
    // inject on every world-space pass -- including ones the engine renders
    // offscreen before compositing. Restricting mono to backbuffer-targeted
    // passes would silently produce no head-tracking at all if the scene is
    // drawn offscreen first, which is exactly the ambiguity this mode exists
    // to remove.
    // Read the world/2D classification LIVE, from vid_state.proj, rather than
    // trusting the flag cached by the vid_setPass hook.
    //
    // vid_setPass is called once to configure a pass and then many draws follow,
    // and anything that repoints vid_state.proj in between (vid_setOrtho3D does
    // exactly that) leaves the cached flag stale. The pointer in vid_state at
    // the moment of the draw is the actual truth about which matrix is about to
    // be uploaded, so use that.
    const bool worldPass = IsWorldPass();
    if (worldPass != g_worldPass) ++g_classifyMismatch;
    NoteProjPointer(VidState().proj);

    TraceDraw(worldPass);

    const bool inject = VrLive()
                     && VR().poseValid()
                     && worldPass
                     && (Cfg().monoTracking || TargetIsBackbuffer()
                         || AlternateEyeActive());

    // Count every world-space draw, injected or not, and separately the ones
    // rejected purely because the engine was drawing offscreen. Those two
    // numbers are what the health report needs to say something falsifiable.
    if (worldPass) {
        ++g_worldDraws;
        if (!TargetIsBackbuffer()) ++g_offscreenWorld;
        if (!inject && VrLive() && VR().poseValid()
            && !Cfg().monoTracking && !TargetIsBackbuffer()) {
            ++g_worldOffscreen;
        }
    }

    // Full-screen passes that bypass uProjMatrix entirely: uid[0] < 0 means the
    // program has no such uniform, so the shader writes clip space directly.
    // Pre-rendered video runs through here. Nothing we can put in a matrix will
    // move it, so shift the VIEWPORT for this draw instead -- glDrawElements
    // runs right after validate_draw returns, and the next draw's
    // SetEyeViewport puts it back.
    if (VrLive() && !Cfg().monoTracking && VR().poseValid()
        && Cfg().videoDepthMetres > 0.0f && TargetIsBackbuffer()
        && Stereo().valid() && g_inDuplicate && !g_inVideoCapture
        && !VideoOffscreenActive()
        && IsBypassPass()) {

        const int sid = VidState().shader;
        for (auto& e : g_bypassSeen) {
            if (e == sid) break;
            if (e == -1) {
                e = sid;
                LogF("bypass: shader %d (0x%02X) has no uProjMatrix -- writes clip "
                     "space directly. Shifting its viewport per eye.", sid, sid);
                break;
            }
        }

        const Eye be = (g_currentEye == 0) ? Eye::Left : Eye::Right;
        const GLsizei ew = static_cast<GLsizei>(Stereo().eyeWidth());
        const GLsizei eh = static_cast<GLsizei>(Stereo().eyeHeight());
        const GLint   x0 = (g_currentEye == 0) ? 0 : static_cast<GLint>(ew);

        bool  fitted = false;
        GLint vx = x0, vy = 0;
        GLsizei vw = ew, vh = eh;

        if (!Cfg().videoLockToHead) {
            // World-locked: project the panel's four CORNERS and fit the
            // viewport to their bounding box. The quad fills whatever viewport
            // it is given, so this lands it where real geometry would --
            // position AND perspective size. Projecting only the centre gives
            // translation alone, which is what made it stretch off-axis.
            const float Z = Cfg().videoDepthMetres * LiveWorldUnitsPerMetre();
            float zn = 16.0f, zf = 32768.0f;
            ExtractNearFar(Proj()[1], zn, zf);
            if (Z <= zn) zn = Z * 0.5f;
            if (Z >= zf) zf = Z * 2.0f;

            mat4 P{};
            VR().EyeProjection(be, zn, zf, P);
            const Affine E = VR().EyeView(be);

            const float halfFov = Cfg().videoSizeDegrees * 0.5f
                                * 3.14159265358979f / 180.0f;
            const float W = Z * std::tan(halfFov);
            const int   sw = ScreenWidth();
            const int   sh = ScreenHeight();
            const float aspect = (sh > 0) ? (float)sw / (float)sh : 1.7778f;
            const float H = (aspect > 0.0f) ? W / aspect : W;

            // Engine camera looks down -Z (ogl_setPerspAngles writes e32 = -1).
            const float corner[4][3] = {
                { -W, -H, -Z }, { +W, -H, -Z }, { -W, +H, -Z }, { +W, +H, -Z }
            };
            float xmin = 1e30f, xmax = -1e30f, ymin = 1e30f, ymax = -1e30f;
            bool ok = true;
            for (int k = 0; k < 4 && ok; ++k) {
                float ce[3];
                for (int i = 0; i < 3; ++i) {
                    ce[i] = E.r[i][0] * corner[k][0] + E.r[i][1] * corner[k][1]
                          + E.r[i][2] * corner[k][2] + E.r[i][3];
                }
                float clip[4];
                for (int r = 0; r < 4; ++r) {
                    clip[r] = P.m[0 * 4 + r] * ce[0] + P.m[1 * 4 + r] * ce[1]
                            + P.m[2 * 4 + r] * ce[2] + P.m[3 * 4 + r];
                }
                if (clip[3] <= 1e-6f) { ok = false; break; }   // behind the eye
                const float nx = clip[0] / clip[3];
                const float ny = clip[1] / clip[3];
                if (nx < xmin) xmin = nx;
                if (nx > xmax) xmax = nx;
                if (ny < ymin) ymin = ny;
                if (ny > ymax) ymax = ny;
            }

            if (ok && xmax > xmin && ymax > ymin) {
                const float pxMin = (xmin + 1.0f) * 0.5f * (float)ew;
                const float pxMax = (xmax + 1.0f) * 0.5f * (float)ew;
                const float pyMin = (ymin + 1.0f) * 0.5f * (float)eh;
                const float pyMax = (ymax + 1.0f) * 0.5f * (float)eh;
                vx = x0 + (GLint)pxMin;
                vy = (GLint)pyMin;
                vw = (GLsizei)(pxMax - pxMin);
                vh = (GLsizei)(pyMax - pyMin);
                if (vw > 0 && vh > 0) fitted = true;
            }
        }

        if (!fitted) {
            // Head-locked, or the panel is behind the eye: constant alignment.
            const float ndcX = VR().HudNdcShiftX(be, Cfg().videoDepthMetres);
            vx = x0 + (GLint)(ndcX * (float)ew * 0.5f);
            vy = 0; vw = ew; vh = eh;
        }

        glViewport(vx, vy, vw, vh);
        // Keep the scissor on this eye's half so the shifted quad cannot bleed
        // into the other eye. BeginFrame disables scissor for its full clear, so
        // it has to be enabled explicitly here.
        glScissor(x0, 0, ew, eh);
        glEnable(GL_SCISSOR_TEST);
    }

    // The flat 2D layer (HUD, menus, subtitles, fades) is drawn with the
    // engine's ortho projection, identical in both eyes. The headset optics
    // apply a fixed per-eye correction assuming an asymmetric render, so an
    // unshifted image is pulled apart and cannot fuse -- which is the double
    // vision on menus. Give it the same shear the 3D layer gets, plus enough
    // convergence to sit at HudDepthMetres.
    if (!inject && !worldPass && VrLive() && !Cfg().monoTracking
        && VR().poseValid() && Cfg().hudDepthMetres > 0.0f
        && TargetIsBackbuffer()) {
        mat4* hudProj = VidState().proj;
        if (hudProj) {
            const Eye hudEye = (g_currentEye == 0) ? Eye::Left : Eye::Right;
            std::memcpy(&g_savedHud, hudProj, sizeof(mat4));

            if (Cfg().hudLockToHead) {
                // Screen-locked: a flat per-eye NDC shift. Ortho output has
                // w == 1, so ndc.x moves with m[12] directly.
                hudProj->m[12] += VR().HudNdcShiftX(hudEye, Cfg().hudDepthMetres);
            } else {
                // World-locked: turn the 2D layer into a quad sitting in the
                // GAME camera's frame, then look at it through the per-eye
                // transform. Head rotation lands in E, so the panel stays put
                // while you look around it.
                //
                //   Q = P_persp * E * L * P_o
                //
                // L maps ortho NDC (x, y, *, 1) to camera space
                // (W*x, H*y, -Z, 1): the z input is deliberately discarded, so
                // every 2D element lands on the one plane.
                const float scale = LiveWorldUnitsPerMetre();
                const float Z = Cfg().hudDepthMetres * scale;
                const float halfFov = Cfg().hudSizeDegrees * 0.5f
                                    * 3.14159265358979f / 180.0f;
                const float W = Z * std::tan(halfFov);
                const int   sw = ScreenWidth();
                const int   sh = ScreenHeight();
                const float aspect = (sh > 0) ? (float)sw / (float)sh : 1.7778f;
                const float H = (aspect > 0.0f) ? W / aspect : W;

                float zn = 16.0f, zf = 32768.0f;
                ExtractNearFar(Proj()[1], zn, zf);
                if (Z <= zn) zn = Z * 0.5f;
                if (Z >= zf) zf = Z * 2.0f;

                mat4 Ppersp{};
                VR().EyeProjection(hudEye, zn, zf, Ppersp);

                mat4 L{};
                L.m[0]  = W;
                // Y-down, to match what the per-eye projection expects. See
                // Config::hudFlipY -- getting this wrong both inverts the image
                // and reverses winding, so culling removes the whole layer.
                L.m[5]  = Cfg().hudFlipY ? -H : H;
                L.m[14] = -Z;
                L.m[15] = 1.0f;

                const mat4 E = AffineToMat4(VR().EyeView(hudEye));
                *hudProj = Mul4(Ppersp, Mul4(E, Mul4(L, g_savedHud)));
            }

            VidState().consts |= kProj;
            g_hValidateDraw.Original<Fn_validate_draw>()();
            std::memcpy(hudProj, &g_savedHud, sizeof(mat4));
            return;
        }
    }

    if (!inject) {
        g_hValidateDraw.Original<Fn_validate_draw>()();
        return;
    }

    RenderState& vs = VidState();

    // vid_state.proj should be &mProj[1] here (that is what g_worldPass means)
    // and vid_state.view should be &mView_packed, but both are pointers the
    // engine is free to repoint, so work through them rather than assuming.
    mat4* livePr = vs.proj;
    mat4* liveVw = vs.view;
    if (!livePr || !liveVw) {
        g_hValidateDraw.Original<Fn_validate_draw>()();
        return;
    }

    // Mono keeps the engine's own projection untouched: field of view stays
    // exactly as the game intended, so anything that looks wrong on screen is
    // the view maths and nothing else.
    const bool doProj = !Cfg().monoTracking && Cfg().perEyeProjection != 0;

    // Snapshot, substitute, upload, restore.
    std::memcpy(&g_savedView, liveVw, sizeof(mat4));
    if (doProj || Cfg().eyeOffsetMode >= 2) {
        std::memcpy(&g_savedProj, livePr, sizeof(mat4));
    }

    const Eye eye = (g_currentEye == 0) ? Eye::Left : Eye::Right;

    mat4 eyeProj{};
    if (doProj) {
        // Preserve the engine's own near/far -- vid_setPass picks them per pass
        // and clobbering them would break fog, depth precision and the shadow
        // chain.
        float zn = 0.0f, zf = 0.0f;
        if (!ExtractNearFar(*livePr, zn, zf)) {
            zn = 16.0f;
            zf = 32768.0f;
        }
        if (Cfg().nearClip > 0.0f) zn = Cfg().nearClip;
        if (Cfg().farClip  > 0.0f) zf = Cfg().farClip;

        VR().EyeProjection(eye, zn, zf, eyeProj);
        std::memcpy(livePr, &eyeProj, sizeof(mat4));
    }

    // finalView = eyeView * gameView, both as row-major 3x4 affines.
    //
    // With PerEyeView off the game's own view is used unchanged, so the only
    // remaining per-eye difference is the frustum shear. If the two halves
    // still differ in that configuration, the difference was never parallax.
    const Affine gameView  = ReadPackedView(g_savedView);
    Affine finalView = Cfg().perEyeView
                     ? Mul(VR().EyeView(eye), gameView)
                     : gameView;

    // Publish it for RoomCull's head-oriented portal test. This is the only
    // place the head's world-space orientation exists: the engine's own view
    // matrix is the game camera's, and mView_packed is never rotated by VR.
    // Either eye will do -- they differ by an IPD, which is nothing against a
    // 1024-unit sector, and the test carries a margin far larger.
    g_lastHeadView      = finalView;
    g_lastHeadViewValid = true;

    // Diagnostic: swing the right eye's view by a large, unmistakable angle.
    if (Cfg().debugEyeYawDegrees != 0.0f && eye == Eye::Right) {
        finalView = Mul(RotY(Cfg().debugEyeYawDegrees), finalView);
    }

    // Move the per-eye TRANSLATION out of the view matrix and into the model
    // matrix, where both shader families read it. The rotation stays in the
    // view matrix -- rotation-only shaders honour that, as the yaw test showed.
    //
    // A shift d in view space is a shift R^T * d in the model matrix's space,
    // because those shaders compute R * p and we need R * (p + R^T d) = R p + d.
    g_modelPatched = false;
    float dView[3] = { 0.0f, 0.0f, 0.0f };
    if (Cfg().eyeOffsetMode != 0 && Cfg().eyeOffsetMode != 3) {
        // The view-space translation the eye/head offset introduces.
        dView[0] = finalView.r[0][3] - gameView.r[0][3];
        dView[1] = finalView.r[1][3] - gameView.r[1][3];
        dView[2] = finalView.r[2][3] - gameView.r[2][3];

        // Leave the view matrix's translation exactly as the game set it; the
        // rotation stays, because rotation-only shaders do honour that.
        finalView.r[0][3] = gameView.r[0][3];
        finalView.r[1][3] = gameView.r[1][3];
        finalView.r[2][3] = gameView.r[2][3];
    }

    if (Cfg().eyeOffsetMode == 1) {
        mat4* liveMd = vs.model;
        if (liveMd) {
            float w[3];
            for (int i = 0; i < 3; ++i) {
                w[i] = gameView.r[0][i] * dView[0]
                     + gameView.r[1][i] * dView[1]
                     + gameView.r[2][i] * dView[2];
            }
            std::memcpy(&g_savedModel, liveMd, sizeof(mat4));
            liveMd->m[3]  += w[0];
            liveMd->m[7]  += w[1];
            liveMd->m[11] += w[2];
            vs.consts |= kModel;
            g_modelPatched = true;
        }
    }

    // Mode 3 leaves the view matrix exactly as the engine set it. That is the
    // whole point: untouched, both shader families agree on the view-space
    // position, and the per-eye transform goes into the projection below.
    if (Cfg().eyeOffsetMode != 3) {
        WritePackedView(*liveVw, finalView);
    }

    // Mode 2: fold the view-space offset into the projection as P * translate(d).
    //
    // Every shader ends with uProjMatrix * vec4(viewpos, 1.0), so this reaches
    // paths the view and model matrices cannot -- skinned geometry through
    // uJoints included. Applied to whatever is already in livePr, which is the
    // per-eye projection when doProj is on and the engine's own when it is not.
    if (Cfg().eyeOffsetMode == 2 &&
        (dView[0] != 0.0f || dView[1] != 0.0f || dView[2] != 0.0f)) {
        mat4& P = *livePr;
        for (int row = 0; row < 4; ++row) {
            P.m[12 + row] += P.m[0 + row] * dView[0]
                           + P.m[4 + row] * dView[1]
                           + P.m[8 + row] * dView[2];
        }
        vs.consts |= kProj;
    }

    // Mode 3: livePr <- livePr * E, with E the eye-from-game-camera transform.
    // Column-major, so (P*E)[c][r] = sum_k P[k][r] * E[c][k]; E's bottom row is
    // (0,0,0,1), which is why column 3 picks up P's own column 3.
    if (Cfg().eyeOffsetMode == 3) {
        const Affine E = VR().EyeView(eye);
        const mat4 P = *livePr;
        mat4 out{};
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                float sum = 0.0f;
                for (int k = 0; k < 3; ++k) {
                    sum += P.m[k * 4 + r] * ((c < 3) ? E.r[k][c] : E.r[k][3]);
                }
                if (c == 3) sum += P.m[3 * 4 + r];
                out.m[c * 4 + r] = sum;
            }
        }
        *livePr = out;
        vs.consts |= kProj;
    }

    // Force the uploads. The engine's own dirty tracking would skip them: the
    // proj bit is set on a POINTER comparison in vid_setPass, so writing new
    // contents into the same mProj[1] would not trip it.
    if (Cfg().eyeOffsetMode != 3) vs.consts |= kView;
    if (doProj) vs.consts |= kProj;

    // Log each eye exactly once, then report the separation between them. If
    // the two translations are identical the eyes are rendering the same image
    // and there is no stereo, no matter what the scale is set to.
    const int eyeIdx = (g_currentEye == 0) ? 0 : 1;
    ++g_injectCount[eyeIdx];
    if (Cfg().verboseFirstFrame && !g_loggedEye[eyeIdx]) {
        g_loggedEye[eyeIdx] = true;
        g_loggedFrame = true;
        if (doProj) {
            LogF("inject: eye=%d proj[0]=%.4f proj[5]=%.4f proj[8]=%.4f proj[9]=%.4f",
                 eyeIdx, eyeProj.m[0], eyeProj.m[5], eyeProj.m[8], eyeProj.m[9]);
        } else {
            Log("inject: MONO -- view only, engine projection untouched");
        }
        if (!Cfg().perEyeView || Cfg().perEyeProjection != 1) {
            static const char* kProjMode[3] = { "OFF (engine projection)",
                                                "HMD asymmetric",
                                                "HMD FOV, SYMMETRIC (no shear)" };
            const int pm = (Cfg().perEyeProjection >= 0 && Cfg().perEyeProjection <= 2)
                         ? Cfg().perEyeProjection : 1;
            LogF("inject: DIAGNOSTIC MODE -- per-eye view=%s projection=%s",
                 Cfg().perEyeView ? "on" : "OFF", kProjMode[pm]);
        }
        if (Cfg().debugEyeYawDegrees != 0.0f) {
            LogF("inject: DEBUG YAW -- right eye view rotated %.1f degrees",
                 Cfg().debugEyeYawDegrees);
        }
        const Affine ev = VR().EyeView(eye);
        LogF("inject: eye=%d eyeView translation = (%+.1f, %+.1f, %+.1f) world units",
             eyeIdx, ev.r[0][3], ev.r[1][3], ev.r[2][3]);
        LogF("inject: eye=%d final view translation = (%.1f, %.1f, %.1f)",
             eyeIdx, finalView.r[0][3], finalView.r[1][3], finalView.r[2][3]);
        for (int k = 0; k < 3; ++k) g_eyeViewT[eyeIdx][k] = finalView.r[k][3];

        // Read back what actually landed in the engine's memory, rather than
        // trusting the Affine we computed. The translation lives at float
        // indices 3, 7 and 11 of the packed view; if those differ between the
        // eyes then our write is correct and any missing parallax is downstream
        // (upload or shader). If they match, the write itself is wrong.
        LogF("inject: eye=%d packed view translation IN MEMORY = (%.1f, %.1f, %.1f) "
             "[floats 3,7,11 at %p]",
             eyeIdx, liveVw->m[3], liveVw->m[7], liveVw->m[11], (void*)liveVw);
        LogF("inject: eye=%d uid[1] (uViewMatrix location) = %d, shader=%d",
             eyeIdx, Shaders()[vs.shader].uid[1], vs.shader);
    }
    if (!g_reportedSeparation && g_loggedEye[0] && g_loggedEye[1]) {
        g_reportedSeparation = true;
        const float dx = g_eyeViewT[0][0] - g_eyeViewT[1][0];
        const float dy = g_eyeViewT[0][1] - g_eyeViewT[1][1];
        const float dz = g_eyeViewT[0][2] - g_eyeViewT[1][2];
        const float sep = std::sqrt(dx*dx + dy*dy + dz*dz);
        LogF("inject: SEPARATION between eyes = %.2f world units (dx=%.2f dy=%.2f dz=%.2f)",
             sep, dx, dy, dz);
        if (sep < 0.01f) {
            Log("inject: *** ZERO SEPARATION -- both eyes are rendering the SAME view. "
                "This is why depth is flat and why world scale changes nothing. ***");
        }
    }

    g_hValidateDraw.Original<Fn_validate_draw>()();

    // The upload has now happened (or been skipped). Ask the GPU what it holds,
    // once per eye per report window. glGetUniformfv forces a pipeline sync, so
    // this must stay rare -- twice per 1800 frames is free.
    if (g_verifyArmed && !g_verifyDone[eyeIdx]) {
        g_verifyDone[eyeIdx] = true;
        const Shader& sh = Shaders()[vs.shader];
        g_gpuProg[eyeIdx] = static_cast<int>(sh.id);
        if (Cfg().eyeOffsetMode == 3) {
            // Mode 3 leaves uViewMatrix identical in both eyes ON PURPOSE, so
            // reading it would report zero separation and look like a failure.
            g_gpuLoc[eyeIdx] = sh.uid[0];
            ReadBackProjTranslation(sh.id, sh.uid[0], g_gpuViewT[eyeIdx]);
        } else {
            g_gpuLoc[eyeIdx] = sh.uid[1];
            ReadBackViewTranslation(sh.id, sh.uid[1], g_gpuViewT[eyeIdx]);
        }
    }

    if (g_modelPatched && vs.model) {
        std::memcpy(vs.model, &g_savedModel, sizeof(mat4));
        g_modelPatched = false;
    }

    // Put the engine's matrices back. It never observes the substitution, so
    // the next pass composes from pristine state rather than compounding.
    if (Cfg().eyeOffsetMode != 3) {
        std::memcpy(liveVw, &g_savedView, sizeof(mat4));
    }
    if (doProj || Cfg().eyeOffsetMode >= 2) {
        std::memcpy(livePr, &g_savedProj, sizeof(mat4));
    }
}

// ---------------------------------------------------------------------------
// ogl_draw / ogl_drawVB -- per-eye duplication
// ---------------------------------------------------------------------------
//
// Every draw is issued twice, into the two halves of the double-wide target.
// Matrix injection is gated on the pass being world-space, so 2D/HUD geometry
// is drawn twice at the same screen position -- a flat overlay locked to both
// eyes, which is what you want before there is a proper world-space HUD.

template <typename Fn, typename... Args>
void DuplicatePerEye(const hook::InlineHook& h, Args... args) {
    if (!VrLive() || g_inDuplicate || Cfg().monoTracking
        || !Cfg().duplicateDraws || !TargetIsBackbuffer()) {
        h.Original<Fn>()(args...);
        return;
    }

    // Alternate-eye: draw ONCE. The whole frame is one eye, so there is nothing
    // to duplicate -- only the backbuffer-targeted draws need the eye viewport,
    // and the offscreen passes keep the engine's own full-size viewport.
    if (AlternateEyeActive()) {
        // One draw, and no eye viewport: the whole frame goes into the scratch
        // target at the engine's own size, and present blits it into one half.
        g_currentEye = g_frameEye;
        h.Original<Fn>()(args...);
        return;
    }

    ++g_dupCount;

    // Clip-space-direct passes (video) cannot be moved by any matrix, so
    // capture the draw once offscreen and replay it as real geometry per eye.
    // That is the only way to get correct keystone and roll.
    // TR6 composites its offscreen scene to the backbuffer through a
    // clip-space-direct shader -- the same signature as an FMV quad. Capturing
    // that and replaying it as a 60-degree panel is what turned TR6 into a small
    // floating rectangle. Its FMVs use the viewport-shift path instead, which
    // VideoOffscreenActive() keeps consistent with this decision.
    if (VideoOffscreenActive() && IsBypassPass()) {
        GLint prevFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

        g_inVideoCapture = true;
        Video().BeginCapture();
        h.Original<Fn>()(args...);
        g_inVideoCapture = false;

        gl::BindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);

        g_inDuplicate = true;
        for (int eye = 0; eye < 2; ++eye) {
            g_currentEye = eye;
            Stereo().SetEyeViewport(eye);
            mat4 mvp{};
            BuildVideoMvp(eye, mvp);
            Video().Replay(mvp, Cfg().videoFlipV);
        }
        g_currentEye = 0;
        g_inDuplicate = false;
        return;
    }

    g_inDuplicate = true;
    for (int eye = 0; eye < 2; ++eye) {
        g_currentEye = eye;
        Stereo().SetEyeViewport(eye);
        h.Original<Fn>()(args...);
    }
    g_currentEye = 0;
    g_inDuplicate = false;
}

void __cdecl Detour_ogl_draw(void* vb, unsigned firstIndex, unsigned count, int strip) {
    DuplicatePerEye<Fn_ogl_draw>(g_hDraw, vb, firstIndex, count, strip);
}

void __cdecl Detour_ogl_drawVB(int fvf, void* vb, void* ib, int stride,
                               unsigned first, unsigned count, int strip) {
    if (Cfg().logCallsites) NoteCallsite("ogl_drawVB", _ReturnAddress());

    DuplicatePerEye<Fn_ogl_drawVB>(g_hDrawVB, fvf, vb, ib, stride, first, count, strip);
}

// ---------------------------------------------------------------------------
// fmvShow -- "a video is on screen this frame"
// ---------------------------------------------------------------------------
typedef void (__cdecl* Fn_fmvShow)(void);

void __cdecl Detour_fmvShow() {
    g_fmvFrame = g_frameIndex;
    g_hFmvShow.Original<Fn_fmvShow>()();
}

// ---------------------------------------------------------------------------
// ogl_present -- frame boundary
// ---------------------------------------------------------------------------
void LazyInit() {
    if (g_ready) return;
    if (!VR().active()) return;          // nothing to set up until OpenVR is live
    if (!wglGetCurrentContext()) return;

    // Mono touches no GL objects at all: no eye target, no FBO_default
    // redirection, nothing to fail. It just needs poses.
    if (Cfg().monoTracking) {
        g_ready = true;
        VR().BeginFrame();
        Log("present: MONO head-tracking active (no stereo target, no submit)");
        return;
    }

    if (!gl::Load()) return;

    uint32_t w = 0, h = 0;
    VR().GetEyeSize(w, h);
    if (!Stereo().Create(w, h)) return;

    // Make the engine's notion of "the backbuffer" be our eye target.
    //
    // FBO_default is a single global that init_ogl latches from
    // GL_FRAMEBUFFER_BINDING, and ogl_setRenderTarget binds it whenever the
    // game asks for colour_id == 0 && depth_id == 0. Overwriting it redirects
    // every "back to the screen" in the engine into the stereo target without
    // hooking ogl_setRenderTarget at all.
    g_realDefaultFbo = FboDefault();
    FboDefault() = Stereo().fbo();
    LogF("present: redirected FBO_default %u -> %u", g_realDefaultFbo, Stereo().fbo());

    // Alternate-eye scratch, at the engine's own render size.
    if (Cfg().alternateEyeGame6) {
        const int mw = ScreenWidth()  > 0 ? ScreenWidth()  : (int)(w * 2);
        const int mh = ScreenHeight() > 0 ? ScreenHeight() : (int)h;
        if (!Stereo().CreateMono((uint32_t)mw, (uint32_t)mh)) {
            Log("stereo: mono scratch unavailable -- TR6 alternate-eye will "
                "flicker, because the engine's clears reach both halves");
        }
    }

    // Offscreen video panel. Optional: if it fails to build, the viewport-fit
    // path still handles the video, just with residual keystone.
    if (Cfg().videoOffscreen) {
        const int vw = ScreenWidth()  > 0 ? ScreenWidth()  : (int)w;
        const int vh = ScreenHeight() > 0 ? ScreenHeight() : (int)h;
        if (!Video().Create((uint32_t)vw, (uint32_t)vh)) {
            Log("video: offscreen panel unavailable, falling back to the "
                "viewport fit (residual keystone off-axis)");
        }
    }

    g_ready = true;
    VR().BeginFrame();
    Stereo().BeginFrame();
}

// Bring OpenVR up from inside the render loop rather than at process start, and
// retry on a slow cadence.
//
// This buys resilience: SteamVR can be started after the game, and a runtime
// that is mid-startup gets another chance instead of losing the session. If it
// never comes up, every detour early-outs through VrLive() and the game runs
// exactly as it would without the mod.
bool BringUpVR() {
    if (VR().active()) return true;
    if (g_vrGaveUp || !Cfg().enabled) return false;

    // Retry on a slow cadence: SteamVR may still be starting up.
    if (g_vrLastAttemptFrame != 0 && (g_frameIndex - g_vrLastAttemptFrame) < 120) return false;
    g_vrLastAttemptFrame = g_frameIndex;
    ++g_vrAttempts;

    LogF("vr: initialisation attempt %u at frame %u", g_vrAttempts, g_frameIndex);
    if (VR().Init()) {
        Log("vr: up and running");
        return true;
    }

    if (g_vrAttempts >= 10) {
        g_vrGaveUp = true;
        Log("vr: giving up after 10 attempts -- the game continues unmodified");
    }
    return false;
}

void __cdecl Detour_ogl_present() {
    ++g_frameIndex;

    // Frame-graph trace bookkeeping, around the frame boundary.
    if (g_tracing) {
        FlushTraceRun();
        LogF("trace: --- frame %u totals: world=%d flat=%d across %d target run(s) ---",
             g_frameIndex - 1, g_frameWorld, g_frameFlat, g_traceEmitted);
        if (g_frameWorld + g_frameFlat < 30) {
            Log("trace: NOTE very few draws -- this is a menu or loading screen, "
                "not gameplay. Re-capture with a level running.");
        }
    }

    PollTraceKey();
    PollTuningKeys();

    // Re-assert the XInput pointer every frame: cheap, and it self-heals if the
    // game re-resolves XInput or if we got here before WinMain had.
    GamepadUpdate();

    // The game DLLs load after we do, so this hook installs lazily.
    RoomCullUpdate();

    // Periodic health report, in deltas. A one-shot report at frame 300 only
    // ever sampled the menus, where almost everything legitimately is 2D -- it
    // said "2 injections out of 725 draws" and looked like a bug when it was
    // just a title screen.
    if (VrLive() && g_frameIndex >= g_lastReportFrame + 1800) {
        const unsigned dDup   = g_dupCount       - g_prevDup;
        const unsigned dInj0  = g_injectCount[0]  - g_prevInj0;
        const unsigned dInj1  = g_injectCount[1]  - g_prevInj1;
        const unsigned dWorld = g_worldDraws      - g_prevWorld;
        const unsigned dOff   = g_worldOffscreen  - g_prevOffscreen;
        g_lastReportFrame = g_frameIndex;
        g_prevDup = g_dupCount; g_prevInj0 = g_injectCount[0]; g_prevInj1 = g_injectCount[1];
        g_prevWorld = g_worldDraws; g_prevOffscreen = g_worldOffscreen;

        // Against total world draws, not against duplications. See g_worldDraws.
        const unsigned pct = dWorld ? (100u * (dInj0 + dInj1) / dWorld) : 0u;
        // Measured frame rate, and what each eye actually gets.
        LARGE_INTEGER now{}, freq{};
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&freq);
        if (g_lastReportTime.QuadPart != 0 && freq.QuadPart != 0) {
            const double secs = double(now.QuadPart - g_lastReportTime.QuadPart)
                              / double(freq.QuadPart);
            if (secs > 0.0) {
                const double fps = 1800.0 / secs;
                LogF("perf: %.1f fps rendered%s", fps,
                     AlternateEyeActive()
                       ? " -- alternate-eye, so each EYE updates at half that"
                       : "");
            }
        }
        g_lastReportTime = now;

        LogF("stereo health @frame %u: world draws=%u  duplicated=%u  "
             "injected eye0=%u eye1=%u  (%u%% of world draws got per-eye matrices)  "
             "offscreen-skipped=%u  classify-mismatch=%u",
             g_frameIndex, dWorld, dDup, dInj0, dInj1, pct, dOff, g_classifyMismatch);

        // What the GPU actually held, per eye. This is the load-bearing line:
        // if the two translations match, the per-eye view never reached the
        // shader and nothing downstream can produce depth. If they differ, the
        // matrices are on the GPU and the fault is viewport, target or submit.
        if (g_verifyDone[0] && g_verifyDone[1]) {
            const float dx = g_gpuViewT[0][0] - g_gpuViewT[1][0];
            const float dy = g_gpuViewT[0][1] - g_gpuViewT[1][1];
            const float dz = g_gpuViewT[0][2] - g_gpuViewT[1][2];
            const float sep = std::sqrt(dx*dx + dy*dy + dz*dz);
            LogF("verify: %s ON GPU  eye0=(%.1f, %.1f, %.1f) prog=%d loc=%d  "
                 "eye1=(%.1f, %.1f, %.1f) prog=%d loc=%d  separation=%.2f",
                 (Cfg().eyeOffsetMode == 3) ? "uProjMatrix translation column"
                                            : "uViewMatrix translation",
                 g_gpuViewT[0][0], g_gpuViewT[0][1], g_gpuViewT[0][2],
                 g_gpuProg[0], g_gpuLoc[0],
                 g_gpuViewT[1][0], g_gpuViewT[1][1], g_gpuViewT[1][2],
                 g_gpuProg[1], g_gpuLoc[1], sep);
            if (!gl::GetUniformfv) {
                Log("verify: glGetUniformfv unavailable -- GPU-side check skipped, "
                    "the numbers above are meaningless.");
            } else if (sep < 0.01f) {
                Log("verify: *** the two eyes hold the SAME matrix. The per-eye "
                    "transform is not reaching the shader. ***");
            }
        } else if (VrLive() && !Cfg().monoTracking) {
            Log("verify: no per-eye GPU sample taken this window -- injection is "
                "not running on both eyes.");
        }
        g_verifyArmed = true;
        g_verifyDone[0] = g_verifyDone[1] = false;

        // The frame just rendered is still sitting in the eye target -- Submit
        // and the clear both happen later in this function. Compare the two
        // halves pixel for pixel. Every upstream stage has now been verified
        // correct, so this is the measurement that says whether the halves are
        // genuinely two different images or one image twice.
        if (VrLive() && !Cfg().monoTracking) {
            int samples = 0, differing = 0;
            Stereo().CompareHalves(samples, differing);
            // NOT a depth measure. It compares the SAME pixel coordinate in
            // both halves, so it reports image SHIFT: a large constant offset
            // (the frustum shear) lights it up, while correct parallax of a few
            // pixels across smooth texture falls under the threshold and reads
            // as zero. A low number here means nothing is wrong.
            LogF("halves: %d of %d sampled pixels differ (image shift indicator "
                 "only -- not a depth measure)", differing, samples);
        }

        LogProjHistogram();

        if (dWorld > 200 && pct < 25) {
            LogF("stereo health: *** only %u%% of world draws got per-eye matrices "
                 "(%u of %u were skipped as offscreen), so most of the scene renders "
                 "the same view into both halves -- that is why there is no depth. ***",
                 pct, dOff, dWorld);
        }
    }

    const bool wantTrace = TraceActive();
    if (wantTrace && !g_tracing) {
        LogF("trace: BEGIN frame %u  screen=%dx%d  FBO_default=%u  FBO_custom=%u",
             g_frameIndex, ScreenWidth(), ScreenHeight(), FboDefault(), FboCustom());
    }
    if (!wantTrace && g_tracing) {
        Log("trace: done");
    }
    if (wantTrace != g_tracing) {
        g_tracing = wantTrace;
        g_run = TraceRun{};
    }
    g_traceEmitted = 0;
    g_frameWorld = g_frameFlat = 0;

    BringUpVR();
    LazyInit();

    if (!VrLive()) {
        g_hPresent.Original<Fn_ogl_present>()();
        return;
    }

    // Mono: swap normally, then sample the head pose for the next frame. No
    // compositor involvement whatsoever.
    if (Cfg().monoTracking) {
        g_hPresent.Original<Fn_ogl_present>()();
        VR().BeginFrame();
        return;
    }

    // Hand both halves to the compositor before the swap so the runtime gets
    // the frame as early as possible.
    // AER: fold this frame's eye into its half. The other half is untouched and
    // keeps the frame it was given.
    if (AlternateEyeActive() && Stereo().monoValid()) {
        Stereo().BlitMonoToHalf(g_frameEye);
    }

    if (Cfg().eyeMarkers) Stereo().MarkEyes();

    uint32_t w = 0, h = 0;
    VR().GetEyeSize(w, h);
    VR().Submit(Stereo().texture(), w, h);

    if (Cfg().mirrorToWindow) {
        Stereo().MirrorToWindow(ScreenWidth(), ScreenHeight(), g_realDefaultFbo);
    }

    // The original swaps the window and resets the vb/ib cache. Point
    // FBO_default back at the real window for the duration so anything inside
    // it that touches the default framebuffer behaves.
    FboDefault() = g_realDefaultFbo;
    g_hPresent.Original<Fn_ogl_present>()();
    FboDefault() = (AlternateEyeActive() && Stereo().monoValid())
                 ? Stereo().monoFbo()
                 : Stereo().fbo();

    // Latch poses for the frame we are about to render, then re-arm the target.
    VR().BeginFrame();

    // Decide whether the coming frame uses alternate-eye, from what the frame
    // just drawn contained. Menus and FMVs fall back to ordinary per-draw
    // duplication, which runs at full rate and looks better for 2D.
    {
        g_offscreenWorldPrev = g_offscreenWorld;
        g_offscreenWorld = 0;
        const bool want = AlternateEyeCapable()
            && g_offscreenWorldPrev >=
               static_cast<unsigned>(Cfg().alternateEyeMinOffscreen < 0
                                     ? 0 : Cfg().alternateEyeMinOffscreen);
        if (want != g_aerLatched) {
            LogF("tr6: alternate-eye %s (%u offscreen world draws last frame)",
                 want ? "ON -- offscreen 3D scene" : "off -- 2D, full rate",
                 g_offscreenWorldPrev);
        }
        g_aerLatched = want;
    }

    if (AlternateEyeActive()) {
        // Flip eyes, then render the whole frame into the single-eye scratch.
        // FBO_default points there, so the engine's own clears cannot reach the
        // half holding the other eye.
        g_frameEye = 1 - g_frameEye;
        g_currentEye = g_frameEye;
        Stereo().BeginMono();
        FboDefault() = Stereo().monoFbo();
        if (!g_loggedAltEye) {
            g_loggedAltEye = true;
            Log("tr6: alternate-eye rendering active -- the scene is drawn "
                "offscreen and composited, so each FRAME is one eye. Both eyes "
                "are stereo-correct; each updates at half the frame rate.");
        }
    } else {
        Stereo().BeginFrame();
    }

    if (!g_loggedStereoFrame) {
        g_loggedStereoFrame = true;
        Log("present: first stereo frame submitted");
    }
}

} // namespace

// Hand RoomCull the head's world-space orientation. False until the first
// stereo frame has been injected, so the caller can fall back to adding rooms
// unconditionally rather than rejecting everything before VR is up.
bool LastHeadView(Affine& out) {
    if (!g_lastHeadViewValid) return false;
    out = g_lastHeadView;
    return true;
}

bool InstallHooks() {
    if (!Bind()) return false;

    struct Target {
        hook::InlineHook* hook;
        uint32_t          rva;
        void*             detour;
        size_t            stolen;
        const uint8_t*    expect;
        size_t            expectLen;
        const char*       name;
        const int*        rip;       // RIP-relative disp32 offsets, or nullptr
        size_t            ripCount;
    };

    const Target targets[] = {
        { &g_hSetPass,      rva::vid_setPass,   reinterpret_cast<void*>(&Detour_vid_setPass),
          10, kSetPassPrologue,  sizeof(kSetPassPrologue),  "vid_setPass"   },
        { &g_hValidateDraw, rva::validate_draw, reinterpret_cast<void*>(&Detour_validate_draw),
           6, kValidatePrologue, sizeof(kValidatePrologue), "validate_draw" },
        { &g_hDraw,         rva::ogl_draw,      reinterpret_cast<void*>(&Detour_ogl_draw),
           5, kDrawPrologue,     sizeof(kDrawPrologue),     "ogl_draw"      },
        { &g_hDrawVB,       rva::ogl_drawVB,    reinterpret_cast<void*>(&Detour_ogl_drawVB),
           5, kDrawPrologue,     sizeof(kDrawPrologue),     "ogl_drawVB"    },
        { &g_hPresent,      rva::ogl_present,   reinterpret_cast<void*>(&Detour_ogl_present),
           6, kPresentPrologue,  sizeof(kPresentPrologue),  "ogl_present"   },
        { &g_hFmvShow,      rva::fmvShow,       reinterpret_cast<void*>(&Detour_fmvShow),
           7, kFmvShowPrologue,  sizeof(kFmvShowPrologue),  "fmvShow"       },
    };

    bool allOk = true;
    for (const Target& t : targets) {
        if (!t.hook->Install(Fn(t.rva), t.detour, t.stolen, t.expect, t.expectLen,
                             t.name, t.rip, t.ripCount))
            allOk = false;
    }

    if (!allOk) {
        Log("hooks: at least one hook failed -- rolling all of them back");
        RemoveHooks();
        return false;
    }

    Log("hooks: all six installed");
    return true;
}

void RemoveHooks() {
    GamepadShutdown();
    RoomCullShutdown();

    // Reverse order of installation.
    g_hFmvShow.Remove();
    g_hPresent.Remove();
    g_hDrawVB.Remove();
    g_hDraw.Remove();
    g_hValidateDraw.Remove();
    g_hSetPass.Remove();

    if (g_ready && g_realDefaultFbo != 0) {
        FboDefault() = g_realDefaultFbo;
    }
    g_ready = false;
}

} // namespace tr
