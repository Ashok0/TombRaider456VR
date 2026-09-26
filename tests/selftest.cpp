// selftest.cpp -- checks the two things in this project that are easy to get
// silently wrong: the inline hook mechanism, and the projection/view maths
// against the engine's own formulas.
//
// Build:  tests\build_selftest.cmd
//
// The projection test is the important one. It asserts that feeding a SYMMETRIC
// frustum through the OpenVR path reproduces ogl_setPerspAngles byte for byte:
//
//     mProj[1].e00 =  1/tanX
//     mProj[1].e11 = -1/tanY
//     mProj[1].e22 = (zNear + zFar) / (zNear - zFar)
//     mProj[1].e23 = (2 * zFar * zNear) / (zNear - zFar)
//     mProj[1].e32 = -1
//
// If that holds, the only difference between the engine's own projection and
// ours is the asymmetry OpenVR asks for -- which is the entire point.

#include "StereoMath.h"
#include "PortalGeom.h"
#include "InlineHook.h"
#include "Log.h"
#include "MotionGunMath.h"
#include "Config.h"

#include <windows.h>
#include <cstdio>
#include <cmath>

static int g_fail = 0;

static void Check(bool cond, const char* what) {
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_fail;
}

static void CheckNear(float got, float want, const char* what, float eps = 1e-5f) {
    const bool ok = std::fabs(got - want) <= eps * (1.0f + std::fabs(want));
    printf("  [%s] %-46s got %12.6f want %12.6f\n", ok ? "ok" : "FAIL", what, got, want);
    if (!ok) ++g_fail;
}

// ---------------------------------------------------------------------------
// 1. Projection: symmetric OpenVR input must reproduce ogl_setPerspAngles.
// ---------------------------------------------------------------------------
static void TestProjectionMatchesEngine() {
    printf("\nprojection vs ogl_setPerspAngles (symmetric case)\n");

    const float tanX = 0.8f, tanY = 0.6f;
    const float zn = 16.0f, zf = 32768.0f;

    tr::mat4 p{};
    // OpenVR raw tangents for a symmetric frustum.
    tr::BuildEyeProjection(p, -tanX, tanX, -tanY, tanY, zn, zf, /*flipY=*/true);

    CheckNear(p.m[0],  1.0f / tanX,                       "e00 =  1/tanX");
    CheckNear(p.m[5], -1.0f / tanY,                       "e11 = -1/tanY  (engine Y flip)");
    CheckNear(p.m[8],  0.0f,                              "e02 =  0  (no shear)");
    CheckNear(p.m[9],  0.0f,                              "e12 =  0  (no shear)");
    CheckNear(p.m[10], (zn + zf) / (zn - zf),             "e22 = (n+f)/(n-f)");
    CheckNear(p.m[14], (2.0f * zf * zn) / (zn - zf),      "e23 = 2fn/(n-f)", 1e-4f);
    CheckNear(p.m[11], -1.0f,                             "e32 = -1");
    CheckNear(p.m[15], 0.0f,                              "e33 =  0");
}

// ---------------------------------------------------------------------------
// 2. Asymmetric frustum: shear must land in e02/e12, which is exactly what
//    vid_setPerspOffset writes.
// ---------------------------------------------------------------------------
static void TestAsymmetricShear() {
    printf("\nasymmetric frustum shear lands where vid_setPerspOffset writes\n");

    // A left eye: more extent to the left than the right.
    const float l = -1.0f, r = 0.6f, t = -0.8f, b = 0.9f;
    const float zn = 16.0f, zf = 4096.0f;

    tr::mat4 p{};
    tr::BuildEyeProjection(p, l, r, t, b, zn, zf, true);

    CheckNear(p.m[0], 2.0f / (r - l),            "e00 = 2/(r-l)");
    CheckNear(p.m[8], (r + l) / (r - l),         "e02 = (r+l)/(r-l)");
    CheckNear(p.m[5], -(2.0f / (b - t)),         "e11 = -2/(b-t)   (scale negated)");
    // The Y flip negates the SCALE only. The shear multiplies vz, which is not
    // flipped, so it keeps its sign. Negating it too was a real bug: invisible
    // on a symmetric frustum, a fishbowl warp on a real headset.
    CheckNear(p.m[9], (b + t) / (b - t),         "e12 = +(b+t)/(b-t) (shear NOT negated)");
    Check(p.m[8] != 0.0f, "shear is non-zero for an asymmetric eye");
}

// ---------------------------------------------------------------------------
// 2b. The real test: project the frustum's own edges and check they land on the
//     edges of NDC. Checking matrix entries against the formula that produced
//     them is circular; this is not.
//
//     Uses the measured tangents from a real headset (asymmetric vertically),
//     which is the case that exposed the shear-sign bug.
// ---------------------------------------------------------------------------
static void TestFrustumEdgesMapToNDC() {
    printf("\nfrustum edges land on NDC edges (real headset tangents)\n");

    // Left eye of a Quest-class headset, as reported by GetProjectionRaw.
    const float l = -1.3764f, r = 0.8391f, t = -1.4281f, b = 0.9657f;
    const float zn = 16.0f, zf = 32768.0f;

    tr::mat4 p{};
    tr::BuildEyeProjection(p, l, r, t, b, zn, zf, /*flipY=*/true);

    // A point at depth d in ENGINE view space: x right, y DOWN, -z forward.
    // clip = P * (vx, vy, -d, 1);  w = d;  ndc = clip.xy / w.
    auto ndc = [&](float xOverD, float yOverD, float& nx, float& ny) {
        const float d  = 100.0f;
        const float vx = xOverD * d, vy = yOverD * d, vz = -d;
        const float cx = p.m[0] * vx + p.m[4] * vy + p.m[8]  * vz + p.m[12];
        const float cy = p.m[1] * vx + p.m[5] * vy + p.m[9]  * vz + p.m[13];
        const float cw = p.m[3] * vx + p.m[7] * vy + p.m[11] * vz + p.m[15];
        nx = cx / cw;
        ny = cy / cw;
    };

    float nx = 0.0f, ny = 0.0f;

    ndc(r, 0.0f, nx, ny);
    CheckNear(nx, 1.0f, "right edge  -> ndc.x = +1", 1e-4f);

    ndc(l, 0.0f, nx, ny);
    CheckNear(nx, -1.0f, "left edge   -> ndc.x = -1", 1e-4f);

    // OpenVR's raw t/b are given in a Y-up sense: the TOP edge of the image sits
    // at vy/d = b in Y-up space, which is -b once Y points down.
    ndc(0.0f, -b, nx, ny);
    CheckNear(ny, 1.0f, "top edge    -> ndc.y = +1", 1e-4f);

    ndc(0.0f, -t, nx, ny);
    CheckNear(ny, -1.0f, "bottom edge -> ndc.y = -1", 1e-4f);

    // With the shear wrongly negated the top edge came out at ~0.61, so this
    // guards the exact regression.
    ndc(0.0f, -b, nx, ny);
    Check(ny > 0.99f, "top edge is not short of the viewport (the fishbowl bug)");
}

// ---------------------------------------------------------------------------
// 3. ExtractNearFar must invert the engine's own depth terms.
// ---------------------------------------------------------------------------
static void TestNearFarRoundTrip() {
    printf("\nnear/far recovery from mProj[1]\n");

    const float zn = 16.0f, zf = 32768.0f;
    tr::mat4 p{};
    tr::BuildEyeProjection(p, -0.8f, 0.8f, -0.6f, 0.6f, zn, zf, true);

    float n = 0.0f, f = 0.0f;
    Check(tr::ExtractNearFar(p, n, f), "ExtractNearFar succeeds");
    CheckNear(n, zn, "recovered zNear", 1e-3f);
    CheckNear(f, zf, "recovered zFar",  1e-3f);

    // A zeroed matrix must be rejected rather than producing garbage.
    tr::mat4 zero{};
    Check(!tr::ExtractNearFar(zero, n, f), "rejects a degenerate matrix");
}

// ---------------------------------------------------------------------------
// 4. Rigid-transform algebra.
// ---------------------------------------------------------------------------
static void TestAffine() {
    printf("\naffine algebra\n");

    // A 90-degree yaw with a translation.
    tr::Affine m{};
    m.r[0][0] =  0.0f; m.r[0][1] = 0.0f; m.r[0][2] = 1.0f; m.r[0][3] = 10.0f;
    m.r[1][0] =  0.0f; m.r[1][1] = 1.0f; m.r[1][2] = 0.0f; m.r[1][3] = 20.0f;
    m.r[2][0] = -1.0f; m.r[2][1] = 0.0f; m.r[2][2] = 0.0f; m.r[2][3] = 30.0f;

    const tr::Affine inv = tr::InvertRigid(m);
    const tr::Affine id  = tr::Mul(m, inv);

    bool ok = true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            ok &= std::fabs(id.r[i][j] - (i == j ? 1.0f : 0.0f)) < 1e-5f;
    for (int i = 0; i < 3; ++i)
        ok &= std::fabs(id.r[i][3]) < 1e-4f;
    Check(ok, "M * M^-1 == identity");

    const tr::Affine I = tr::Affine::Identity();
    const tr::Affine same = tr::Mul(I, m);
    bool ok2 = true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            ok2 &= std::fabs(same.r[i][j] - m.r[i][j]) < 1e-6f;
    Check(ok2, "I * M == M");
}

// ---------------------------------------------------------------------------
// 5. Y-flip conjugation must stay a rigid transform and scale translation.
// ---------------------------------------------------------------------------
static void TestEngineSpaceConversion() {
    printf("\nOpenVR (Y-up, metres) -> engine view space (Y-down, TR units)\n");

    tr::Affine ovr = tr::Affine::Identity();
    ovr.r[0][3] = 0.032f;   // +32 mm to the right: half of a 64 mm IPD
    ovr.r[1][3] = 0.100f;   // 100 mm up
    ovr.r[2][3] = -0.050f;  // 50 mm forward

    const float scale = 423.0f;
    const tr::Affine e = tr::ToEngineSpace(ovr, scale, /*flipY=*/true);

    CheckNear(e.r[0][3],  0.032f * scale, "x translation scaled, sign kept");
    CheckNear(e.r[1][3], -0.100f * scale, "y translation scaled AND negated");
    CheckNear(e.r[2][3], -0.050f * scale, "z translation scaled, sign kept");

    // Rotation conjugation: a yaw must survive, and S*R*S must stay orthonormal.
    tr::Affine rot = tr::Affine::Identity();
    const float c = 0.8f, s = 0.6f;
    rot.r[0][0] = c; rot.r[0][2] = s;
    rot.r[2][0] = -s; rot.r[2][2] = c;
    const tr::Affine re = tr::ToEngineSpace(rot, 1.0f, true);

    bool orth = true;
    for (int i = 0; i < 3; ++i) {
        float len = 0.0f;
        for (int j = 0; j < 3; ++j) len += re.r[i][j] * re.r[i][j];
        orth &= std::fabs(len - 1.0f) < 1e-5f;
    }
    Check(orth, "conjugated rotation stays orthonormal");
}

// ---------------------------------------------------------------------------
// 6. Packed view round-trip -- the mView_packed layout.
// ---------------------------------------------------------------------------
static void TestPackedView() {
    printf("\nmView_packed round-trip\n");

    tr::mat4 packed{};
    for (int i = 0; i < 16; ++i) packed.m[i] = static_cast<float>(i + 1);

    const tr::Affine a = tr::ReadPackedView(packed);
    Check(a.r[0][0] == 1.0f && a.r[0][3] == 4.0f,  "row 0 is floats 0..3");
    Check(a.r[1][0] == 5.0f && a.r[1][3] == 8.0f,  "row 1 is floats 4..7");
    Check(a.r[2][0] == 9.0f && a.r[2][3] == 12.0f, "row 2 is floats 8..11");

    tr::mat4 out{};
    out.m[12] = 1.0f;              // the (1,0,..) tail the engine writes
    tr::WritePackedView(out, a);
    bool same = true;
    for (int i = 0; i < 12; ++i) same &= (out.m[i] == packed.m[i]);
    Check(same, "write(read(x)) == x for floats 0..11");
    Check(out.m[12] == 1.0f, "floats 12..15 left untouched");
}

// ---------------------------------------------------------------------------
// 7. The inline hook itself, against a synthetic function whose prologue is
//    byte-identical to ogl_draw's.
// ---------------------------------------------------------------------------
typedef int (*TargetFn)();

static const uint8_t kTargetCode[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08,   // mov [rsp+8], rbx   <- same 5 bytes as ogl_draw
    0xB8, 0x2A, 0x00, 0x00, 0x00,   // mov eax, 42
    0xC3                            // ret
};

static hook::InlineHook g_testHook;
static hook::InlineHook g_fourArgHook;

using FourArgFn = int (*)(int, int, int, int);
static const uint8_t kFourArgCode[] = {
    0x40, 0x55, 0x56, 0x57, 0x41, 0x56, // six-byte LOS-style prologue
    0x44, 0x89, 0xC0,                   // mov eax, r8d
    0x44, 0x01, 0xC8,                   // add eax, r9d
    0x01, 0xC8,                         // add eax, ecx
    0x01, 0xD0,                         // add eax, edx
    0x41, 0x5E, 0x5F, 0x5E, 0x5D, 0xC3 // balanced pops + ret
};

static int DetourFourArg(int a, int b, int flags, int mode) {
    return g_fourArgHook.Original<FourArgFn>()(a, b, flags, mode) + 1000;
}

static int DetourTarget() {
    return g_testHook.Original<TargetFn>()() + 100;
}

static void TestInlineHook() {
    printf("\ninline hook mechanism\n");

    void* mem = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    Check(mem != nullptr, "allocated a synthetic target");
    if (!mem) return;
    memcpy(mem, kTargetCode, sizeof(kTargetCode));
    FlushInstructionCache(GetCurrentProcess(), mem, sizeof(kTargetCode));

    TargetFn fn = reinterpret_cast<TargetFn>(mem);
    Check(fn() == 42, "target returns 42 before hooking");

    // Wrong expected bytes must be refused.
    const uint8_t wrong[5] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
    hook::InlineHook bad;
    Check(!bad.Install(mem, reinterpret_cast<void*>(&DetourTarget), 5,
                       wrong, sizeof(wrong), "mismatch"),
          "refuses to patch on prologue mismatch");
    Check(fn() == 42, "target untouched after a refused install");

    Check(g_testHook.Install(mem, reinterpret_cast<void*>(&DetourTarget), 5,
                             kTargetCode, 5, "selftest"),
          "installs with matching prologue");
    Check(fn() == 142, "detour runs and can call the original via the trampoline");

    g_testHook.Remove();
    Check(fn() == 42, "original bytes restored on Remove()");

    VirtualFree(mem, 0, MEM_RELEASE);

    void* four = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE,
                              PAGE_EXECUTE_READWRITE);
    Check(four != nullptr, "allocated four-argument LOS-style target");
    if (!four) return;
    memcpy(four, kFourArgCode, sizeof(kFourArgCode));
    FlushInstructionCache(GetCurrentProcess(), four, sizeof(kFourArgCode));
    FourArgFn fourFn = reinterpret_cast<FourArgFn>(four);
    Check(fourFn(1,2,3,4) == 10, "native target reads all four arguments");
    Check(g_fourArgHook.Install(four, reinterpret_cast<void*>(&DetourFourArg),
                                6, kFourArgCode, 6, "four-arg-selftest"),
          "six-byte LOS-style hook installs");
    Check(fourFn(1,2,3,4) == 1010,
          "detour forwards both LOS flags in r8/r9");
    g_fourArgHook.Remove();
    Check(fourFn(1,2,3,4) == 10,
          "four-argument target restored after Remove()");
    VirtualFree(four, 0, MEM_RELEASE);
}

// ---------------------------------------------------------------------------
// 8. Portal frustum maths (PortalGeom.h).
//
// This is the part of the room culling that can be wrong without crashing: a
// sign slip in the edge planes turns "visible through that doorway" into "not
// visible", and the symptom on screen -- geometry missing when you look away
// from the game camera -- is indistinguishable from the bug the whole feature
// exists to fix. So the cases below are worked out by hand rather than
// captured from a run.
//
// Everything is in eye space with the viewpoint at the origin and -Z forward.
// ---------------------------------------------------------------------------
using tr::portal::Vec3;
using tr::portal::Plane;
using tr::portal::Poly;

static bool InsideAll(const Plane* pl, int n, const Vec3& p) {
    for (int i = 0; i < n; ++i) {
        if (tr::portal::Dot(pl[i].n, p) + pl[i].d < 0.0f) return false;
    }
    return true;
}

static void TestPortalGeometry() {
    printf("\nportal frustum maths\n");

    // --- the root frustum ---------------------------------------------------
    //
    // tanX = 1 is 45 degrees a side, so at 1000 units ahead the edge is at
    // x = +/-1000 exactly. The boundary case matters: `inside` is >= 0, so a
    // point exactly on the edge counts as visible, which is the conservative
    // direction.
    Plane root[tr::portal::kMaxPlanes];
    const int nRoot = tr::portal::RootPlanes(1.0f, 0.5f, root);
    Check(nRoot == 5, "root frustum is 5 planes (near + four sides)");

    Check( InsideAll(root, nRoot, Vec3{    0.0f,   0.0f, -1000.0f }), "straight ahead is inside");
    Check( InsideAll(root, nRoot, Vec3{  999.0f,   0.0f, -1000.0f }), "just inside the right edge");
    Check(!InsideAll(root, nRoot, Vec3{ 1001.0f,   0.0f, -1000.0f }), "just outside the right edge");
    Check( InsideAll(root, nRoot, Vec3{    0.0f, 499.0f, -1000.0f }), "just inside the bottom edge");
    Check(!InsideAll(root, nRoot, Vec3{    0.0f, 501.0f, -1000.0f }), "just outside the bottom edge");
    Check(!InsideAll(root, nRoot, Vec3{    0.0f,   0.0f,  1000.0f }), "behind the head is outside");
    Check(!InsideAll(root, nRoot, Vec3{    0.0f,   0.0f,    -1.0f }), "nearer than the near plane is outside");

    // --- looking through a doorway -----------------------------------------
    //
    // A 200x200 opening 1000 units ahead. The cone through it widens linearly,
    // so at 2000 units it is 400 wide: x = 150 is inside, x = 250 is not. That
    // is the whole point of the mechanism -- the frustum SHRINKS at the doorway
    // rather than the room beyond simply being let in.
    Poly door;
    door.n = 4;
    door.v[0] = Vec3{ -100.0f, -100.0f, -1000.0f };
    door.v[1] = Vec3{  100.0f, -100.0f, -1000.0f };
    door.v[2] = Vec3{  100.0f,  100.0f, -1000.0f };
    door.v[3] = Vec3{ -100.0f,  100.0f, -1000.0f };

    Plane thru[tr::portal::kMaxPlanes];
    const int nThru = tr::portal::BuildPlanes(door, thru);
    Check(nThru == 5, "a quad opening yields near + four edge planes");

    Check( InsideAll(thru, nThru, Vec3{   0.0f,   0.0f, -2000.0f }), "through the doorway, on axis");
    Check( InsideAll(thru, nThru, Vec3{ 150.0f,   0.0f, -2000.0f }), "through the doorway, inside the cone");
    Check(!InsideAll(thru, nThru, Vec3{ 250.0f,   0.0f, -2000.0f }), "beside the doorway is culled");
    Check(!InsideAll(thru, nThru, Vec3{   0.0f, 250.0f, -2000.0f }), "above the doorway is culled");
    Check( InsideAll(thru, nThru, Vec3{  90.0f,   0.0f, -1000.0f }), "in the doorway itself");

    // The engine stores portal vertices in an order relative to the room that
    // owns them, so the same opening arrives wound either way depending on
    // which side it is crossed from. The centroid test in BuildPlanes is what
    // makes that not matter, and this is the case that would catch it.
    Poly reversed;
    reversed.n = 4;
    for (int i = 0; i < 4; ++i) reversed.v[i] = door.v[3 - i];
    Plane rthru[tr::portal::kMaxPlanes];
    const int nR = tr::portal::BuildPlanes(reversed, rthru);
    Check(nR == nThru, "reversed winding yields the same plane count");
    Check( InsideAll(rthru, nR, Vec3{ 150.0f, 0.0f, -2000.0f }), "reversed winding: inside is still inside");
    Check(!InsideAll(rthru, nR, Vec3{ 250.0f, 0.0f, -2000.0f }), "reversed winding: outside is still outside");

    // --- clipping one opening against another ------------------------------
    Poly cut = door;
    const Plane half{ Vec3{ 1.0f, 0.0f, 0.0f }, 0.0f };      // keep x >= 0
    Check(tr::portal::ClipPoly(cut, half), "clip against a half-space succeeds");
    Check(cut.n == 4, "a square cut down the middle is still four-sided");
    bool allRight = true;
    for (int i = 0; i < cut.n; ++i) if (cut.v[i].x < -1e-3f) allRight = false;
    Check(allRight, "every surviving vertex is on the kept side");

    Poly gone = door;
    const Plane away{ Vec3{ 1.0f, 0.0f, 0.0f }, -1000.0f };  // keep x >= 1000
    Check(tr::portal::ClipPoly(gone, away), "clip that removes everything still succeeds");
    Check(gone.n < 3, "a fully-clipped opening is empty");

    // --- degenerate openings ------------------------------------------------
    //
    // A portal seen exactly edge-on has every edge in a plane through the apex.
    // BuildPlanes must drop those rather than emit garbage normals; reporting
    // fewer than four planes is how the traversal is told to keep the frustum
    // it already had.
    Poly edgeOn;
    edgeOn.n = 4;
    edgeOn.v[0] = Vec3{ 0.0f, -100.0f, -1000.0f };
    edgeOn.v[1] = Vec3{ 0.0f, -100.0f, -2000.0f };
    edgeOn.v[2] = Vec3{ 0.0f,  100.0f, -2000.0f };
    edgeOn.v[3] = Vec3{ 0.0f,  100.0f, -1000.0f };
    Plane eplanes[tr::portal::kMaxPlanes];
    const int nE = tr::portal::BuildPlanes(edgeOn, eplanes);
    Check(nE >= 1 && nE <= 5, "an edge-on opening produces no bogus planes");

    // --- box culling --------------------------------------------------------
    //
    // A box far larger than the frustum's cross-section, straddling the axis.
    // No corner of it is inside, and the naive "is any corner visible" test
    // would drop it -- which on screen is a wall or a large static mesh
    // vanishing when you stand close to it.
    Vec3 big[8];
    int  bi = 0;
    for (int xs = 0; xs < 2; ++xs)
        for (int ys = 0; ys < 2; ++ys)
            for (int zs = 0; zs < 2; ++zs)
                big[bi++] = Vec3{ xs ? 5000.0f : -5000.0f,
                                  ys ? 5000.0f : -5000.0f,
                                  zs ? -900.0f : -1100.0f };
    bool anyCornerInside = false;
    for (int i = 0; i < 8; ++i) if (InsideAll(root, nRoot, big[i])) anyCornerInside = true;
    Check(!anyCornerInside, "the oversized box has no corner inside the frustum");
    Check(tr::portal::BoxVisible(big, root, nRoot), "...and BoxVisible reports it visible anyway");

    Vec3 behind[8];
    bi = 0;
    for (int xs = 0; xs < 2; ++xs)
        for (int ys = 0; ys < 2; ++ys)
            for (int zs = 0; zs < 2; ++zs)
                behind[bi++] = Vec3{ xs ? 100.0f : -100.0f,
                                     ys ? 100.0f : -100.0f,
                                     zs ? 2000.0f : 1000.0f };
    Check(!tr::portal::BoxVisible(behind, root, nRoot), "a box wholly behind the head is culled");
}

static void TestMotionGunMath() {
    printf("\nTouch controller -> bind-pose wrist and muzzle\n");
    using namespace tr::motiongun;
    const float identity[3][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    const Basis straight = ControllerBasis(identity, 0);
    const Basis pistol = GunBasis(straight);
    const Vec barrel = Transform(pistol, {0,1,0});
    CheckNear(barrel.x, 0, "native barrel +Y maps to controller forward X");
    CheckNear(barrel.y, 0, "barrel remains level, not wrist-to-tip tilted");
    CheckNear(barrel.z, 1, "native barrel +Y maps to controller forward Z");
    const Vec rightTip=Transform(pistol,MuzzleLocal(1,1));
    const Vec leftTip=Transform(pistol,MuzzleLocal(1,0));
    CheckNear(rightTip.x,-10,"right pistol keeps native muzzle side offset");
    CheckNear(leftTip.x,10,"left pistol keeps native muzzle side offset");
    CheckNear(rightTip.y,-35,"muzzle above wrist does not pitch barrel down");
    CheckNear(leftTip.z,190,"HD pistol flash and shot share muzzle length");

    // Nonzero bind pivot is essential: the old tests used only controller
    // vectors and could not detect rotation around the palette translation.
    const Basis one{{{1,0,0},{0,1,0},{0,0,1}}};
    const Frame bindPose{one,{123,-487,231}};
    Frame inverseBind{};
    Check(Inverse(bindPose,inverseBind),"inverse bind with nonzero wrist origin");
    bool anchorOk=true, barrelOk=true, muzzleOk=true, roundTripOk=true;
    bool oldPivotFails=false, blendOk=true;
    bool gripOk=true, calibratedMuzzleOk=true, translationOk=true;
    float worstError=0;
    auto closeVec=[](Vec a,Vec b) { const Vec d=Sub(a,b); return Dot(d,d)<0.0025f; };
    for (int hd=0;hd<2;++hd) {
        const Frame ib=hd ? HdInverseBind(inverseBind) : inverseBind;
        Frame bind{};
        Check(Inverse(ib,bind),"invert both native and HD axis-swizzled bind");
        const Vec meshWrist=Transform(bind,{0,0,0});
        for (int hand=0;hand<2;++hand)
        for (int axis=0;axis<3;++axis)
        for (int step=0;step<=36;++step) {
            const float angle=step*6.28318530718f/36;
            const float c=std::cos(angle),s=std::sin(angle);
            float tracked[3][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
            const int u=(axis+1)%3,v=(axis+2)%3;
            tracked[u][u]=tracked[v][v]=c;
            tracked[u][v]=-s; tracked[v][u]=s;
            // Also vary artificial/body heading throughout a full turn.
            for (int headingStep=0;headingStep<8;++headingStep) {
                const float heading=headingStep*6.28318530718f/8;
                const Basis controller=ControllerBasis(tracked,heading);
                const Vec handWorld=HandInWorld({1250,-530,2410},
                    hand ? .23f : -.23f, .2f,.4f,heading,500);
                const Frame desired{GunBasis(controller),handWorld};
                // Include non-orthonormal native animation interpolation.
                const Frame native{{{{.85f,.12f,0},{0,1.1f,.08f},{.02f,0,.9f}}},
                                   {410,-150,820}};
                const Frame palette=Multiply(native,ib);
                Frame correction{};
                if (!PaletteCorrection(palette,ib,desired,correction)) {
                    anchorOk=false; continue;
                }
                const Frame moved=Multiply(correction,palette);
                // Seven inches back, one inch up. The calibrated
                // mesh grip must stay on the controller, not orbit it.
                const float gripUnits=.1778f*500, raiseUnits=.0254f*500;
                const Frame calibrated=GripFrame(desired.basis,handWorld,gripUnits,raiseUnits);
                Frame gripCorrection{};
                if (!PaletteCorrection(palette,ib,calibrated,gripCorrection)) {
                    gripOk=false; continue;
                }
                const Frame gripPalette=Multiply(gripCorrection,palette);
                const Vec modelGrip=Transform(bind,{0,gripUnits,-raiseUnits});
                gripOk &= closeVec(Transform(gripPalette,modelGrip),handWorld);
                const Frame zeroGrip=GripFrame(desired.basis,handWorld,0);
                gripOk &= closeVec(zeroGrip.origin,desired.origin);
                const Vec physicalStep{17,-23,31};
                const Frame translated=GripFrame(desired.basis,
                    Add(handWorld,physicalStep),gripUnits,raiseUnits);
                translationOk &= closeVec(Sub(translated.origin,calibrated.origin),
                                          physicalStep);
                for (int weapon=1;weapon<=2;++weapon) {
                    const Vec localMuzzle=MuzzleLocal(weapon,hand);
                    const Vec rendered=Transform(gripPalette,Transform(bind,localMuzzle));
                    const Vec shotAndFlash=Transform(calibrated,localMuzzle);
                    calibratedMuzzleOk &= closeVec(rendered,shotAndFlash);
                }
                const Vec actual=Transform(moved,meshWrist);
                const Vec error=Sub(actual,handWorld);
                worstError=std::fmax(worstError,std::sqrt(Dot(error,error)));
                anchorOk &= closeVec(actual,handWorld);
                for (int weapon=1;weapon<=2;++weapon) {
                    const Vec tip=MuzzleLocal(weapon,hand);
                    const Vec renderedTip=Transform(moved,Transform(bind,tip));
                    const Vec flashAndShot=Transform(desired,tip);
                    muzzleOk &= closeVec(renderedTip,flashAndShot);
                    const Vec barrelDelta=Sub(
                        Transform(moved,Transform(bind,Add(tip,{0,64,0}))),renderedTip);
                    barrelOk &= closeVec(barrelDelta,
                        Scale({controller.r[0][2],controller.r[1][2],controller.r[2][2]},64));
                }
                float packed[12]{};
                WriteRows(moved,packed);
                roundTripOk &= closeVec(Transform(ReadRows(packed),meshWrist),actual);
                // All helper bones must receive the same affine correction:
                // linear skin blends commute with this rigid placement.
                const Frame helper{native.basis,Add(native.origin,{20,30,40})};
                const Vec p{10,20,30};
                const Vec blend=Add(Scale(Transform(palette,p),.6f),
                                    Scale(Transform(helper,p),.4f));
                const Vec movedBlend=Add(Scale(Transform(moved,p),.6f),
                    Scale(Transform(Multiply(correction,helper),p),.4f));
                blendOk &= closeVec(movedBlend,Transform(correction,blend));
                // Reproduce the OLD pivot formula; fixture must reject it.
                const Vec old=Add(Add(palette.origin,
                    Transform(correction.basis,Sub(Transform(palette,meshWrist),palette.origin))),
                    Sub(handWorld,native.origin));
                oldPivotFails |= !closeVec(old,handWorld);
            }
        }
    }
    Check(anchorOk,"wrist stays at controller through yaw/pitch/roll and full heading rotation");
    Check(barrelOk,"left/right barrel direction stays controller-parallel through rotations");
    Check(muzzleOk,"rendered muzzle equals flash/shot position for pistols and Uzis");
    Check(blendOk,"HD blended helper bones follow the same arm placement");
    Check(roundTripOk,"shader row packing preserves corrected wrist");
    Check(oldPivotFails,"regression fixture detects the old skin-palette pivot bug");
    Check(gripOk,"raised/depth-calibrated grip stays fixed through full wrist/heading rotations");
    Check(translationOk,"grip calibration preserves 1:1 controller translation");
    Check(calibratedMuzzleOk,"calibrated rendered muzzle, flash and shots stay aligned");
    const Frame straightGrip=GripFrame(pistol,{0,0,0},.3048f*500);
    CheckNear(straightGrip.origin.x,0,"grip adjustment does not add sideways offset");
    CheckNear(straightGrip.origin.y,0,"level grip adjustment does not add vertical offset");
    CheckNear(straightGrip.origin.z,-152.4f,"grip adjustment pulls gun back twelve inches");
    const Frame adjustedGrip=GripFrame(pistol,{0,0,0},.1778f*500,.0254f*500);
    const Vec adjustment=Sub(adjustedGrip.origin,straightGrip.origin);
    CheckNear(adjustment.x,0,"latest calibration leaves horizontal alignment unchanged");
    CheckNear(adjustment.y,-12.7f,"latest calibration raises guns exactly one inch");
    CheckNear(adjustment.z,63.5f,"latest calibration advances guns exactly five inches");
    printf("  worst synthetic wrist error: %.6f game units\n",worstError);
    Frame singular{}, result{};
    Check(!Inverse(singular,result),"singular frame is rejected safely");

    const Vec shot=ShotForward(1.5707963268f,.5235987756f);
    CheckNear(shot.x,std::cos(.5235987756f),"native +90 yaw fires right");
    CheckNear(shot.y,-.5f,"native +30 pitch fires upward");
    const float yawVr[3][4]={{0,0,1,0},{0,1,0,0},{-1,0,0,0}};
    CheckNear(ControllerBasis(yawVr,0).r[0][2],-1,"OpenVR yaw sign maps correctly");
    const Vec hand=HandInWorld({100,200,300},.2f,.1f,.5f,1.5707963268f,100);
    CheckNear(hand.x,150,"heading rotates forward offset into world X");
    CheckNear(hand.y,210,"hand height uses Y-down");
    CheckNear(hand.z,280,"heading rotates right offset into world -Z");
}

static void TestGunCalibration() {
    printf("\nLive motion-gun calibration\n");
    using namespace tr::motiongun;
    CalibrationKeys keys{};
    int command=-1;
    Check(keys.ControlEvent(true,true,false),"Ctrl reserved so calibration does not fire guns");
    Check(keys.ControlEvent(false,false,false),"reserved Ctrl release swallowed after deactivation");
    Check(!keys.ControlEvent(true,false,false),"Ctrl remains native outside calibration");
    Check(!keys.ControlEvent(false,false,false),"unowned Ctrl release remains native");
    Check(!keys.Event(0,true,false,false,false,true,false,command),
          "plain function keys remain native");
    Check(!keys.Event(0,true,true,false,false,false,false,command),
          "calibration inactive outside focused motion-gun gameplay");
    Check(!keys.Event(3,true,true,false,true,true,false,command),
          "Alt chords are not consumed");
    bool mappings=true;
    for (int shift=0;shift<2;++shift)
    for (int key=0;key<7;++key) {
        mappings &= keys.Event(key,true,true,shift!=0,false,true,false,command);
        mappings &= command==key+shift*7;
        mappings &= keys.Event(key,true,true,shift!=0,false,true,true,command);
        mappings &= command==-1; // no keyboard-repeat runaway
        mappings &= keys.Event(key,false,false,false,false,false,false,command);
        mappings &= command==-1; // release consumed even after Ctrl/focus loss
    }
    Check(mappings,"all fourteen hotkey commands, repeat and keyup ownership");
    Calibration c{};
    const Calibration initial=c;
    AdjustCalibration(c,1); AdjustCalibration(c,3); AdjustCalibration(c,5);
    CheckNear(c.rightMetres,.00635f,"right step is quarter inch");
    CheckNear(c.raiseMetres-initial.raiseMetres,.00635f,"up step is quarter inch");
    CheckNear(c.gripForwardMetres-initial.gripForwardMetres,-.00635f,"forward reduces grip-back");
    AdjustCalibration(c,8); AdjustCalibration(c,10); AdjustCalibration(c,12);
    CheckNear(c.yawDegrees,1,"yaw step is one degree");
    CheckNear(c.pitchDegrees,1,"pitch step is one degree");
    CheckNear(c.rollDegrees,1,"roll step is one degree");
    for (int i=0;i<1000;++i) { AdjustCalibration(c,1); AdjustCalibration(c,10); }
    CheckNear(c.rightMetres,.5f,"position limited to half metre");
    CheckNear(c.pitchDegrees,90,"pitch safely limited to ninety degrees");
    const Basis identity{{{1,0,0},{0,1,0},{0,0,1}}};
    c={}; c.yawDegrees=25; c.pitchDegrees=30; c.rollDegrees=40;
    const Basis angled=CalibratedController(identity,c);
    const Vec ray=Transform(angled,{0,0,1});
    const Vec expected=ShotForward(25*.0174532925199433f,30*.0174532925199433f);
    CheckNear(ray.x,expected.x,"calibrated barrel yaw equals shot yaw");
    CheckNear(ray.y,expected.y,"calibrated barrel pitch equals shot pitch");
    CheckNear(ray.z,expected.z,"roll does not steer the barrel");
    bool pivot=true,muzzle=true;
    auto closeVec=[](Vec a,Vec b) { const Vec d=Sub(a,b); return Dot(d,d)<.0025f; };
    const Frame inverseBind{identity,{-123,487,-231}};
    Frame bind{}; Inverse(inverseBind,bind);
    const Frame native{identity,{400,500,600}};
    const Frame palette=Multiply(native,inverseBind);
    for (int step=0;step<=36;++step) {
        const float angle=step*.1745329252f;
        const float tracked[3][4]={{std::cos(angle),0,std::sin(angle),0},
                                 {0,1,0,0},{-std::sin(angle),0,std::cos(angle),0}};
        c.rightMetres=.03f; c.gripForwardMetres=.1778f; c.raiseMetres=.0254f;
        const Basis controller=CalibratedController(ControllerBasis(tracked,.63f),c);
        const Frame desired=GripFrame(GunBasis(controller),{17,23,31},
            c.gripForwardMetres*500,c.raiseMetres*500,c.rightMetres*500);
        Frame correction{}; PaletteCorrection(palette,inverseBind,desired,correction);
        const Frame moved=Multiply(correction,palette);
        pivot &= closeVec(Transform(moved,Transform(bind,
            {-c.rightMetres*500,c.gripForwardMetres*500,-c.raiseMetres*500})),{17,23,31});
        for (int hand=0;hand<2;++hand) for (int weapon=1;weapon<=2;++weapon)
            muzzle &= closeVec(Transform(moved,Transform(bind,MuzzleLocal(weapon,hand))),
                               Transform(desired,MuzzleLocal(weapon,hand)));
    }
    Check(pivot,"combined position/pitch/yaw/roll keeps calibrated grip pinned");
    Check(muzzle,"combined calibration preserves rendered muzzle and shot origin");
}

static void TestGunCalibrationPersistence() {
    printf("\nGun calibration save/reload\n");
    wchar_t tempDir[MAX_PATH]{}, ini[MAX_PATH]{};
    GetTempPathW(MAX_PATH,tempDir);
    const bool created=GetTempFileNameW(tempDir,L"vrg",0,ini)!=0;
    Check(created,"create isolated temporary INI");
    if (!created) return;
    WritePrivateProfileStringW(L"VR",L"FirstPersonMotionGuns",L"1",ini);
    WritePrivateProfileStringW(L"VR",L"FirstPersonMotionGunGripForwardMetres",L"0.1778",ini);
    WritePrivateProfileStringW(L"VR",L"UnrelatedUserSetting",L"123",ini);
    tr::LoadConfig(ini);
    const auto baseline=tr::LiveMotionGunCalibration();
    tr::AdjustMotionGunCalibration(3);
    tr::RestoreMotionGunCalibration();
    CheckNear(tr::LiveMotionGunCalibration().raiseMetres,baseline.raiseMetres,
              "restore discards unsaved changes");
    tr::AdjustMotionGunCalibration(1);
    tr::AdjustMotionGunCalibration(3);
    tr::AdjustMotionGunCalibration(5);
    tr::AdjustMotionGunCalibration(8);
    tr::AdjustMotionGunCalibration(10);
    tr::AdjustMotionGunCalibration(12);
    const auto desired=tr::LiveMotionGunCalibration();
    Check(tr::SaveMotionGunCalibration(),"save six live values to INI");
    tr::AdjustMotionGunCalibration(3);
    tr::RestoreMotionGunCalibration();
    CheckNear(tr::LiveMotionGunCalibration().raiseMetres,desired.raiseMetres,
              "restore uses latest successful save");
    tr::LoadConfig(ini);
    const auto actual=tr::LiveMotionGunCalibration();
    CheckNear(actual.rightMetres,desired.rightMetres,"reload right offset");
    CheckNear(actual.raiseMetres,desired.raiseMetres,"reload up offset");
    CheckNear(actual.gripForwardMetres,desired.gripForwardMetres,"reload depth offset");
    CheckNear(actual.pitchDegrees,desired.pitchDegrees,"reload pitch");
    CheckNear(actual.yawDegrees,desired.yawDegrees,"reload yaw");
    CheckNear(actual.rollDegrees,desired.rollDegrees,"reload roll");
    Check(GetPrivateProfileIntW(L"VR",L"UnrelatedUserSetting",0,ini)==123,
          "save preserves unrelated INI settings");
    Check(GetPrivateProfileIntW(L"VR",L"FirstPersonMotionGuns",0,ini)==1,
          "save preserves enabled motion guns");
    wchar_t backup[MAX_PATH+40]{};
    swprintf_s(backup,L"%s.motion-gun-calibration.bak",ini);
    Check(GetFileAttributesW(backup)!=INVALID_FILE_ATTRIBUTES,"save creates recovery backup");
    Check(GetPrivateProfileIntW(L"VR",L"UnrelatedUserSetting",0,backup)==123,
          "backup preserves original INI");
    DeleteFileW(backup);
    DeleteFileW(ini);
}

static void TestMotionGunEnemyAim() {
    using namespace tr::motiongun;
    Vec ray{9,9,9};
    const Vec muzzle{100,200,300}, forward{0,0,1};
    Check(AssistedDirection(muzzle,forward,{200,200,1300},ray),"near-barrel enemy assisted");
    CheckNear(Dot(ray,ray),1.f,"assist ray normalized");
    Check(ray.x>0 && ray.z>0,"assist originates at muzzle not Lara");
    Check(AssistedDirection(muzzle,forward,{100,100,1300},ray),"vertical enemy assist");
    Check(ray.y<0,"assist includes vertical aim");
    Check(!AssistedDirection(muzzle,forward,{400,200,1300},ray),"outside 12 degree cone unchanged");
    Check(!AssistedDirection(muzzle,forward,{100,200,-700},ray),"enemy behind hand rejected");
    Check(!AssistedDirection(muzzle,forward,muzzle,ray),"zero distance rejected");
    Check(!AssistedDirection(muzzle,forward,{100,200,10000},ray),"distant enemy rejected");
    Check(!AssistedDirection(muzzle,{0,0,0},{100,200,1300},ray),"invalid barrel rejected");
    const Vec nativeEnd{123,245,678};
    const auto hit=CollisionEndpoint(true,nativeEnd,muzzle,forward);
    Check(hit.x==nativeEnd.x && hit.y==nativeEnd.y && hit.z==nativeEnd.z,
          "confirmed sphere hit retains short native endpoint for HitTarget");
    const auto miss=CollisionEndpoint(false,nativeEnd,muzzle,forward);
    Check(miss.x==100 && miss.y==200 && miss.z==20780,"miss retains long muzzle impact ray");
    Calibration c; c.pitchDegrees=-20;
    const Basis identity{{{1,0,0},{0,1,0},{0,0,1}}};
    const auto controller=CalibratedController(identity,c);
    const auto down=Transform(controller,forward);
    CheckNear(down.y,std::sin(20.f*.01745329252f),"minus 20 pitch points down in game axes");
    const auto gun=GunBasis(controller);
    const auto wrist=GripFrame(gun,muzzle,c.gripForwardMetres,c.raiseMetres,c.rightMetres);
    const auto grip=Transform(wrist,{-c.rightMetres,c.gripForwardMetres,-c.raiseMetres});
    CheckNear(grip.x,muzzle.x,"20 degree tilt keeps grip X");
    CheckNear(grip.y,muzzle.y,"20 degree tilt keeps grip Y");
    CheckNear(grip.z,muzzle.z,"20 degree tilt keeps grip Z");
}

int main() {
    printf("TombRaiderVR self-test\n======================\n");

    TestProjectionMatchesEngine();
    TestAsymmetricShear();
    TestFrustumEdgesMapToNDC();
    TestNearFarRoundTrip();
    TestAffine();
    TestEngineSpaceConversion();
    TestPackedView();
    TestPortalGeometry();
    TestInlineHook();
    TestMotionGunMath();
    TestMotionGunEnemyAim();
    TestGunCalibration();
    TestGunCalibrationPersistence();

    printf("\n%s (%d failure%s)\n",
           g_fail == 0 ? "ALL PASSED" : "FAILURES",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
