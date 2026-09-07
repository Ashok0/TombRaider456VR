// Engine.h -- Verified layout of tomb456.exe's OpenGL renderer.
//
// Every address and offset below was read out of the Ghidra database for
//   tomb456.exe  (Tomb Raider IV-VI Remastered v1.0.2a, 2026-01-17, with PDB)
// and re-verified against the decompiler/disassembler before being written
// here. Do not "clean up" a number without re-checking it in the DB.
//
// Image base is 0x140000000. The .data section is enormous:
//   .data  0x140167000 .. 0x14E9BDBDF   (232 MB, mostly zero-filled BSS)
// so globals at 0x14E5xxxxx are legitimate image-relative addresses even
// though the file on disk is only 1.6 MB. Everything here is expressed as an
// RVA and rebased onto the real module base at runtime.
#pragma once

#include <cstdint>

namespace tr {

// Ghidra reported image base for the build these RVAs came from.
constexpr uint64_t kReferenceImageBase = 0x140000000ull;

// ---------------------------------------------------------------------------
// Function RVAs
// ---------------------------------------------------------------------------
namespace rva {

// --- hook targets -----------------------------------------------------------

// void vid_setPass(int shader, float* params, int cull, int blend)
// 2944 bytes. The pass dispatcher: a 202-case switch that decides, per pass,
// which projection matrix is live (mProj[0] ortho vs mProj[1] perspective),
// depth/blend/cull state and the seven texture slots. Installed as app.setPass
// by vidInit; the game DLLs only ever reach it through the app vtable.
constexpr uint32_t vid_setPass        = 0x0000C4C0;

// void validate_draw(void)
// 1967 bytes. The single choke point where every uniform reaches the GPU.
// Called immediately before glDrawElements by ogl_draw and ogl_drawVB.
constexpr uint32_t validate_draw      = 0x00011CC0;

// void ogl_draw(void* vb, uint firstIndex, uint count, int strip)
// void ogl_drawVB(int fvf, void* vb, void* ib, int stride, uint first,
//                 uint count, int strip)
// The only two callers of validate_draw. Each does:
//     ...bind vao/vbo...  validate_draw();  glDrawElements(...);
// which is why duplicating a draw per eye has to happen here, not inside
// validate_draw -- the draw call itself is issued by the caller.
constexpr uint32_t ogl_draw           = 0x00012BD0;
constexpr uint32_t ogl_drawVB         = 0x00012CF0;

// void ogl_present(void)
// 83 bytes. wglSwapIntervalEXT(app.dbg_vsync); SwapBuffers(hDC); clears the
// vb/ib cache; toggles GL_FRAMEBUFFER_SRGB (0x8F9D) on game == 2.
// The engine's own frame boundary -- a better hook than wglSwapBuffers.
constexpr uint32_t ogl_present        = 0x00012600;

// --- called, not hooked -----------------------------------------------------

// void vid_setPerspOffset(float x, float y)
// Writes mProj[1].e02/.e12 (the frustum shear terms) and sets consts |= 1.
// This is the engine's own asymmetric-frustum lever, already exposed as API.
constexpr uint32_t vid_setPerspOffset = 0x0000B8D0;

// void ogl_setPerspAngles(float tanX, float tanY, float zNear, float zFar)
// Builds a symmetric frustum into mProj[1]. Note e11 = -1/tanY: the engine
// projection is Y-flipped relative to a textbook GL frustum.
// NOT hooked, and never called on this build: the game reaches the perspective
// through ogl_setPersp instead. Verified by hooking it and logging -- zero calls.
constexpr uint32_t ogl_setPerspAngles = 0x00012840;

// void ogl_getPerspAngles(float tanX, float tanY, float zNear, float zFar,
//                         mat4* out)
// Misnamed by the PDB: it does not GET anything, it BUILDS a projection matrix
// into the caller's own buffer. init_ogl installs it in the app vtable, so the
// game DLL calls into the engine to construct a projection for its own use --
// deriving cull planes being the plausible reason. NOT hooked, and never called
// either; verified the same way.
constexpr uint32_t ogl_getPerspAngles = 0x000127D0;

// void ogl_setPersp(float tanY, int width, int height, float zNear, float zFar)
// e11 = -1/tanY, e00 = height / (width * tanY). THIS is what the game calls.
//
// Widening it does NOT affect culling. Measured: hooking this and scaling tanY
// by 2 widened the projection from 64.4 to 103.1 degrees and produced no extra
// geometry at all, so the DLL builds its cull planes independently of the
// engine's projection. Fixing the geometry that goes missing when you look away
// from the game camera means finding the frustum/portal test inside
// tomb4/5/6.dll. Do not re-run the widening experiment; it is settled.
constexpr uint32_t ogl_setPersp       = 0x000128E0;

// void vid_setPerspMatrix(const float m[16])
// The game hands over a complete projection. Not called on this build.
constexpr uint32_t vid_setPerspMatrix = 0x0000B920;

// void vid_setViewMatrix(int* m)  -- 3x4 fixed-point (16384 = 1.0) row-major
constexpr uint32_t vid_setViewMatrix  = 0x0000B960;

// void ogl_setViewport(int x, int y, int w, int h)  -- tail-jmp to glViewport
constexpr uint32_t ogl_setViewport    = 0x00012AE0;
// void ogl_setScissor(int x, int y, int w, int h)
// -> glScissor(x, gTargetHeight - y - h, w, h)   (Y is flipped here too)
constexpr uint32_t ogl_setScissor     = 0x00012AC0;

// void ogl_setRenderTarget(int colorId, int depthId, int colorIndex,
//                          int depthIndex, int colorMip, int depthMip)
constexpr uint32_t ogl_setRenderTarget = 0x00013C30;

constexpr uint32_t vidInit            = 0x0000D750;
constexpr uint32_t init_ogl           = 0x00015BA0;

} // namespace rva

// ---------------------------------------------------------------------------
// Data RVAs
// ---------------------------------------------------------------------------
namespace drva {

// _XInputGetState / _XInputSetState: function-pointer globals WinMain fills in
// from GetProcAddress. There is no XInput import to hook -- overwriting these is
// how we present the Touch controllers to the game as an Xbox pad.
constexpr uint32_t XInputGetState  = 0x00692858;
constexpr uint32_t XInputSetState  = 0x006928B0;

constexpr uint32_t vid_state       = 0x0E51C900;  // RenderState, 240 bytes
constexpr uint32_t vid_state_prev  = 0x0E51CA00;  // shadow copy, redundancy filter
constexpr uint32_t mProj           = 0x0E51C6F0;  // mat4[2]: [0]=ortho, [1]=perspective
constexpr uint32_t mView           = 0x0E51C8C0;  // rotation-only view (separate consumer)
constexpr uint32_t mView_packed    = 0x0E51CB30;  // what vid_state.view points at
constexpr uint32_t mShadow         = 0x0E51CB70;  // mat4[6]
constexpr uint32_t mInverseCulling = 0x0E5186C8;  // int, set by vid_setInverseCullingFlag
constexpr uint32_t shaders         = 0x0E9AE040;  // Shader[202]
constexpr uint32_t ogl_textures    = 0x0E9ADF30;  // GLuint[]
constexpr uint32_t FBO_custom      = 0x0E9ADF28;  // the ONE offscreen FBO
constexpr uint32_t FBO_default     = 0x0E9ADF2C;  // latched at init from GL_FRAMEBUFFER_BINDING
constexpr uint32_t ogl_rt          = 0x0E9AE000;  // 24-byte render-target latch
constexpr uint32_t app             = 0x005833E0;  // the renderer/game API struct
constexpr uint32_t gWidth          = 0x00698664;
constexpr uint32_t gHeight         = 0x00698660;
constexpr uint32_t gTargetHeight   = 0x00698678;
// NOTE: gTargetWidth sits 44 MB away from gTargetHeight at RVA 0x3298680 even
// though ogl_setRenderTarget writes the pair together. Confirmed by xrefs from
// both appGetWidth and ogl_setRenderTarget, so it is a real location, just an
// odd one. Treat with suspicion if you ever write to it.
constexpr uint32_t gTargetWidth    = 0x03298680;

} // namespace drva

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------

// Column-major, 16 floats, uploaded straight to GL with transpose = GL_FALSE.
// Field naming follows the PDB: eRC == row R, column C, at float index C*4 + R.
struct mat4 {
    float m[16];

    float&       at(int row, int col)       { return m[col * 4 + row]; }
    const float& at(int row, int col) const { return m[col * 4 + row]; }
};
static_assert(sizeof(mat4) == 64, "mat4 must be 16 floats");

// vid_state.consts dirty-flag bits.
//
// Derived directly from validate_draw's upload block, NOT from the earlier
// hand-written notes (which had `joints` wrong). Cross-checked against the
// 0xff07bf mask that a shader switch forces, which sets exactly bits
// {0,1,2,3,4,5,7,8,9,10,16..23} -- matching this table.
enum ConstBits : uint32_t {
    kProj          = 1u << 0,   // uid[0]  glUniformMatrix4fv(loc, 1, GL_FALSE, proj)
    kView          = 1u << 1,   // uid[1]  glUniform4fv(loc, 4, view)   <-- not Matrix4fv
    kShadow        = 1u << 2,   // uid[2]  glUniformMatrix4fv(loc, 6, GL_FALSE, shadow)
    kShadowFalloff = 1u << 3,   // uid[3]  glUniform4fv(loc, 6, ...)
    kShadowPos     = 1u << 4,   // uid[4]  glUniform4fv(loc, 6, ...)
    kFogColor      = 1u << 5,   // uid[8]
    kFogPlane      = 1u << 8,   // uid[15]
    kDofPlane      = 1u << 9,   // uid[16]
    kEffectOffsets = 1u << 10,  // uid[17] glUniform4fv(loc, 3, ...)
    kModel         = 1u << 16,  // uid[5]  glUniform4fv(loc, 4, model)  <-- not Matrix4fv
    kParams        = 1u << 17,  // uid[6]
    kJoints        = 1u << 18,  // uid[10] glUniform4fv(loc, num_joints*3, ...)
    kLPos          = 1u << 19,  // uid[11]
    kLCol          = 1u << 20,  // uid[12]
    kLDir          = 1u << 21,  // uid[13]
    kAmbient       = 1u << 22,  // uid[14]
    kColor         = 1u << 23,  // uid[7]
    kAllOnShaderSwitch = 0x00ff07bfu,
};

// 240 bytes, 29 members. This is the engine's entire "camera": there is no
// Camera or Frustum object anywhere in the binary.
struct RenderState {
    void*    vb;              // 0
    void*    ib;              // 8
    int32_t  shader;          // 16
    int32_t  depthTest;       // 20
    int32_t  depthWrite;      // 24
    int32_t  depthSlope;      // 28
    int32_t  blend;           // 32
    int32_t  cull;            // 36
    uint32_t tex[7];          // 40
    uint32_t smp[7];          // 68
    uint32_t consts;          // 96   <- ConstBits
    int32_t  num_joints;      // 100
    mat4*    proj;            // 104  <- &mProj[0] (ortho) or &mProj[1] (persp)
    mat4*    view;            // 112  <- &mView_packed
    mat4*    shadow;          // 120
    float*   shadowFalloff;   // 128
    float*   shadowPos;       // 136
    mat4*    model;           // 144
    float*   params;          // 152
    float*   color;           // 160
    float*   fogColor;        // 168
    float*   fogPlane;        // 176
    float*   dofPlane;        // 184
    float*   effectOffsets;   // 192
    float*   joints;          // 200
    float*   LPos;            // 208
    float*   LCol;            // 216
    float*   LDir;            // 224
    float*   ambient;         // 232
};
static_assert(sizeof(RenderState) == 240, "RenderState must be 240 bytes");

// 80 bytes. shaders[0..201].
struct Shader {
    uint32_t id;        // GL program object
    int32_t  uid[18];   // uniform locations; -1 means "not present in this program"
    int32_t  fvf;       // vertex format
};
static_assert(sizeof(Shader) == 80, "Shader must be 80 bytes");

// 24 bytes. Written only by ogl_setRenderTarget, read only by ogl_texCopy.
// It is a single latch, not a stack -- nested target changes do not restore.
struct OglRenderTarget {
    int32_t color_id;      // 0
    int32_t color_mip;     // 4
    int32_t color_index;   // 8   <- array layer
    int32_t depth_id;      // 12
    int32_t depth_mip;     // 16
    int32_t depth_index;   // 20
};
static_assert(sizeof(OglRenderTarget) == 24, "OglRenderTarget must be 24 bytes");

// ---------------------------------------------------------------------------
// Runtime module binding
// ---------------------------------------------------------------------------

// Resolves the base of tomb456.exe. Returns false if this DLL was loaded into
// something else. Must be called before any of the accessors below.
bool Bind();

uint64_t Base();

inline void* Fn(uint32_t r)  { return reinterpret_cast<void*>(Base() + r); }
inline void* Var(uint32_t r) { return reinterpret_cast<void*>(Base() + r); }

RenderState&     VidState();
RenderState&     VidStatePrev();

// Address of the engine's _XInputGetState function-pointer global, or nullptr
// if the module is not bound yet.
void*            XInputGetStateSlot();
mat4*            Proj();          // mat4[2]
mat4&            ViewPacked();
Shader*          Shaders();
uint32_t*        OglTextures();
uint32_t&        FboCustom();
uint32_t&        FboDefault();
OglRenderTarget& Rt();
int32_t&         ScreenWidth();
int32_t&         ScreenHeight();
int32_t&         TargetWidth();
int32_t&         TargetHeight();

// True when vid_state.proj points at mProj[1] -- i.e. the pass currently being
// configured is world-space 3D rather than the 2D/UI ortho layer.
bool IsWorldPass();

} // namespace tr
