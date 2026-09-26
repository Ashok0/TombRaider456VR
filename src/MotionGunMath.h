// Pure Quest/OpenVR -> TR4/5 first-person hand maths. Kept separate from
// runtime hooks so the axis/sign conventions can be regression-tested.
#pragma once
#include <cmath>

namespace tr::motiongun {

struct Vec { float x, y, z; };
struct Basis { float r[3][3]; };
struct Frame { Basis basis; Vec origin; };

inline Vec Add(Vec a, Vec b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec Sub(Vec a, Vec b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec Scale(Vec a, float s) { return {a.x*s, a.y*s, a.z*s}; }
inline float Dot(Vec a, Vec b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec Cross(Vec a, Vec b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline Vec Transform(const Basis& m, Vec p) {
    return {m.r[0][0]*p.x + m.r[0][1]*p.y + m.r[0][2]*p.z,
            m.r[1][0]*p.x + m.r[1][1]*p.y + m.r[1][2]*p.z,
            m.r[2][0]*p.x + m.r[2][1]*p.y + m.r[2][2]*p.z};
}
inline Basis Multiply(const Basis& a, const Basis& b) {
    Basis result{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            for (int k = 0; k < 3; ++k)
                result.r[row][col] += a.r[row][k] * b.r[k][col];
    return result;
}

inline Vec Transform(const Frame& m, Vec p) {
    return Add(Transform(m.basis, p), m.origin);
}
inline Frame Multiply(const Frame& a, const Frame& b) {
    return {Multiply(a.basis, b.basis), Transform(a, b.origin)};
}
inline Frame ReadRows(const float* p) {
    return {{{{p[0],p[1],p[2]}, {p[4],p[5],p[6]}, {p[8],p[9],p[10]}}},
            {p[3],p[7],p[11]}};
}
inline void WriteRows(const Frame& m, float* p) {
    for (int row=0; row<3; ++row)
        for (int col=0; col<3; ++col) p[row*4+col]=m.basis.r[row][col];
    p[3]=m.origin.x; p[7]=m.origin.y; p[11]=m.origin.z;
}
inline bool Inverse(const Frame& m, Frame& out) {
    const auto& a=m.basis.r;
    const Vec rows[3]={{a[0][0],a[0][1],a[0][2]},
                       {a[1][0],a[1][1],a[1][2]},
                       {a[2][0],a[2][1],a[2][2]}};
    const Vec cof[3]={Cross(rows[1],rows[2]), Cross(rows[2],rows[0]),
                      Cross(rows[0],rows[1])};
    const float det=Dot(rows[0],cof[0]);
    if (!std::isfinite(det) || std::fabs(det)<1e-8f) return false;
    for (int col=0; col<3; ++col) {
        out.basis.r[0][col]=cof[col].x/det;
        out.basis.r[1][col]=cof[col].y/det;
        out.basis.r[2][col]=cof[col].z/det;
    }
    out.origin=Scale(Transform(out.basis,m.origin),-1);
    return std::isfinite(out.origin.x) && std::isfinite(out.origin.y) &&
           std::isfinite(out.origin.z);
}

// GetJoints returns skinning palettes, NOT joint frames: P = J * inverseBind.
// Recover J before computing the rigid correction for the whole masked draw.
inline bool PaletteCorrection(const Frame& palette, const Frame& inverseBind,
                              const Frame& desired, Frame& correction) {
    Frame bind{}, inverseJoint{};
    if (!Inverse(inverseBind,bind) ||
        !Inverse(Multiply(palette,bind),inverseJoint)) return false;
    correction=Multiply(desired,inverseJoint);
    return true;
}

// The HD mesh input axes are converted by GetJoints: col0, -col2, col1.
inline Frame HdInverseBind(Frame bind) {
    for (int row=0; row<3; ++row) {
        const float y=bind.basis.r[row][1];
        bind.basis.r[row][1]=-bind.basis.r[row][2];
        bind.basis.r[row][2]=y;
    }
    return bind;
}

// Native barrel axis is +Y, not the wrist-to-muzzle displacement. The latter
// includes the barrel's height/side offset and must not steer the gun.
inline Basis GunBasis(const Basis& controller) {
    const Basis barrelToController{{{1,0,0},{0,0,-1},{0,1,0}}};
    return Multiply(controller,barrelToController);
}
inline Vec MuzzleLocal(int weapon, int hand) {
    // HD SetGunFlash offsets, shared by TR4/5 (hand 0=left, 1=right).
    return weapon==2 ? Vec{-11,195,60} : Vec{hand ? -10.0f : 10.0f,190,35};
}

inline Vec HandInWorld(Vec camera, float right, float down, float forward,
                       float heading, float unitsPerMetre) {
    const float c = std::cos(heading), s = std::sin(heading);
    return {camera.x + unitsPerMetre*(c*right + s*forward),
            camera.y + unitsPerMetre*down,
            camera.z + unitsPerMetre*(-s*right + c*forward)};
}

// Third row of TR4/5 phd_GenerateW2V: this is the LOS direction used by
// FireWeapon, with game-positive pitch pointing upward (negative Y).
inline Vec ShotForward(float yaw, float pitch) {
    const float flat = std::cos(pitch);
    return {std::sin(yaw)*flat, -std::sin(pitch), std::cos(yaw)*flat};
}

// OpenVR's -Z forward/Y up becomes TR's +Z forward/Y down. The physical
// controller's +Z basis is consequently the ray Lara should fire along.
inline Basis ControllerBasis(const float (&m)[3][4], float heading) {
    Basis controller{};
    const float c = std::cos(heading), s = std::sin(heading);
    for (int j = 0; j < 3; ++j) {
        const float sign = j == 0 ? 1.0f : -1.0f;
        const float x = m[0][j]*sign, y = -m[1][j]*sign,
                    z = -m[2][j]*sign;
        controller.r[0][j] = c*x + s*z;
        controller.r[1][j] = y;
        controller.r[2][j] = -s*x + c*z;
    }
    return controller;
}

} // namespace tr::motiongun
