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
#include "VRSystem.h"

#include <cstring>
#include <cmath>

namespace tr {
namespace {

// --- hook objects -----------------------------------------------------------
hook::InlineHook g_hSetPass;
hook::InlineHook g_hValidateDraw;
hook::InlineHook g_hDraw;
hook::InlineHook g_hDrawVB;
hook::InlineHook g_hPresent;

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
bool  g_reportedSeparation = false;
// Per-eye injection counts. If eye 1 never gets injected, the duplication loop
// is not reaching validate_draw with the right eye selected.
unsigned g_injectCount[2] = { 0, 0 };
unsigned g_dupCount = 0;
unsigned g_classifyMismatch = 0;   // live vs cached world-pass disagreements
unsigned g_lastReportFrame = 0;
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

// ---------------------------------------------------------------------------
// vid_setPass -- pass classification
// ---------------------------------------------------------------------------
void __cdecl Detour_vid_setPass(int shader, float* params, int cull, int blend) {
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
                     && (Cfg().monoTracking || TargetIsBackbuffer());

    // Count every world-space draw, injected or not, and separately the ones
    // rejected purely because the engine was drawing offscreen. Those two
    // numbers are what the health report needs to say something falsifiable.
    if (worldPass) {
        ++g_worldDraws;
        if (!inject && VrLive() && VR().poseValid()
            && !Cfg().monoTracking && !TargetIsBackbuffer()) {
            ++g_worldOffscreen;
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
        g_gpuLoc[eyeIdx]  = sh.uid[1];
        ReadBackViewTranslation(sh.id, sh.uid[1], g_gpuViewT[eyeIdx]);
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

    ++g_dupCount;
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
    DuplicatePerEye<Fn_ogl_drawVB>(g_hDrawVB, fvf, vb, ib, stride, first, count, strip);
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
            LogF("verify: uViewMatrix ON GPU  eye0=(%.1f, %.1f, %.1f) prog=%d loc=%d  "
                 "eye1=(%.1f, %.1f, %.1f) prog=%d loc=%d  separation=%.2f world units",
                 g_gpuViewT[0][0], g_gpuViewT[0][1], g_gpuViewT[0][2],
                 g_gpuProg[0], g_gpuLoc[0],
                 g_gpuViewT[1][0], g_gpuViewT[1][1], g_gpuViewT[1][2],
                 g_gpuProg[1], g_gpuLoc[1], sep);
            if (!gl::GetUniformfv) {
                Log("verify: glGetUniformfv unavailable -- GPU-side check skipped, "
                    "the numbers above are meaningless.");
            } else if (sep < 0.01f) {
                Log("verify: *** THE GPU HOLDS THE SAME VIEW MATRIX FOR BOTH EYES. "
                    "The per-eye write reaches mView_packed but not the shader, so "
                    "the upload is being skipped or overwritten between our write "
                    "and the draw. Depth is impossible and IpdScale is inert until "
                    "this line shows a non-zero separation. ***");
            } else {
                Log("verify: per-eye view matrices ARE reaching the GPU. If there is "
                    "still no depth, the fault is downstream: eye viewport, render "
                    "target, or compositor submit -- capture a frame trace (F10).");
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
            LogF("halves: %d of %d sampled pixels differ between the left and "
                 "right halves of the eye target", differing, samples);
            if (samples > 0 && differing == 0) {
                Log("halves: *** THE TWO HALVES ARE THE SAME IMAGE. Per-eye "
                    "matrices reach the GPU but both eyes render identical "
                    "pixels, so the duplication is drawing both eyes into the "
                    "same region. Depth is impossible regardless of scale. ***");
            }
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
    FboDefault() = Stereo().fbo();

    // Latch poses for the frame we are about to render, then re-arm the target.
    VR().BeginFrame();
    Stereo().BeginFrame();

    if (!g_loggedStereoFrame) {
        g_loggedStereoFrame = true;
        Log("present: first stereo frame submitted");
    }
}

} // namespace

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
    };

    bool allOk = true;
    for (const Target& t : targets) {
        if (!t.hook->Install(Fn(t.rva), t.detour, t.stolen, t.expect, t.expectLen, t.name))
            allOk = false;
    }

    if (!allOk) {
        Log("hooks: at least one hook failed -- rolling all of them back");
        RemoveHooks();
        return false;
    }

    Log("hooks: all five installed");
    return true;
}

void RemoveHooks() {
    // Reverse order of installation.
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
