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
    TraceDraw(g_worldPass);

    const bool inject = VrLive()
                     && VR().poseValid()
                     && g_worldPass
                     && (Cfg().monoTracking || TargetIsBackbuffer());

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
    const bool doProj = !Cfg().monoTracking;

    // Snapshot, substitute, upload, restore.
    std::memcpy(&g_savedView, liveVw, sizeof(mat4));
    if (doProj) std::memcpy(&g_savedProj, livePr, sizeof(mat4));

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
    const Affine gameView  = ReadPackedView(g_savedView);
    const Affine finalView = Mul(VR().EyeView(eye), gameView);
    WritePackedView(*liveVw, finalView);

    // Force the uploads. The engine's own dirty tracking would skip them: the
    // proj bit is set on a POINTER comparison in vid_setPass, so writing new
    // contents into the same mProj[1] would not trip it.
    vs.consts |= kView;
    if (doProj) vs.consts |= kProj;

    if (Cfg().verboseFirstFrame && !g_loggedFrame) {
        g_loggedFrame = true;
        if (doProj) {
            LogF("inject: eye=%d proj[0]=%.4f proj[5]=%.4f proj[8]=%.4f proj[9]=%.4f",
                 g_currentEye, eyeProj.m[0], eyeProj.m[5], eyeProj.m[8], eyeProj.m[9]);
        } else {
            Log("inject: MONO -- view only, engine projection untouched");
        }
        LogF("inject: game view row0 = %.3f %.3f %.3f %.1f",
             gameView.r[0][0], gameView.r[0][1], gameView.r[0][2], gameView.r[0][3]);
        LogF("inject: final view row0 = %.3f %.3f %.3f %.1f",
             finalView.r[0][0], finalView.r[0][1], finalView.r[0][2], finalView.r[0][3]);
    }

    g_hValidateDraw.Original<Fn_validate_draw>()();

    // Put the engine's matrices back. It never observes the substitution, so
    // the next pass composes from pristine state rather than compounding.
    std::memcpy(liveVw, &g_savedView, sizeof(mat4));
    if (doProj) std::memcpy(livePr, &g_savedProj, sizeof(mat4));
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
