// Headset/controller-only body inference. Input/output are joint frames, not
// skinning matrices. Native animated feet remain the exact leg IK targets.
#pragma once
#include "FirstPersonIK.h"

namespace tr::firstperson {
inline float Length(Vec v) { return std::sqrt(Dot(v,v)); }
inline Vec LimitLength(Vec v,float limit) {
    const float n=Length(v);return n>limit ? Scale(v,limit/n) : v;
}
inline bool Finite(const Frame& f) {
    if (!std::isfinite(f.origin.x) || !std::isfinite(f.origin.y) || !std::isfinite(f.origin.z)) return false;
    for (const auto& row:f.basis.r) for(float value:row) if(!std::isfinite(value)) return false;
    return true;
}
inline Basis LimitRotation(Basis rotation,float radians) {
    auto q=RotationOf(rotation);
    if(q.w<0) { q.w=-q.w;q.v=Scale(q.v,-1); }
    const float angle=2*std::atan2(Length(q.v),q.w);
    Vec axis{};
    return angle>radians && Unit(q.v,axis) ? RotationMatrix(AxisRotation(axis,radians)) : rotation;
}

inline bool SolveBody(const Frame (&native)[15],Vec headDelta,
                      const Basis& headRotation,Vec forward,float scale,
                      Frame (&posed)[15]) {
    if (!std::isfinite(scale) || scale<=1 || !Finite({headRotation,headDelta})) return false;
    for (const auto& joint:native) { Frame inverse{};if(!Finite(joint) || !Inverse(joint,inverse)) return false; }
    float upper[2]{},lower[2]{};
    for(int leg=0;leg<2;++leg) {
        const int first=1+leg*3;
        upper[leg]=Length(Sub(native[first+1].origin,native[first].origin));
        lower[leg]=Length(Sub(native[first+2].origin,native[first+1].origin));
        if(upper[leg]<1.e-3f || lower[leg]<1.e-3f) return false;
    }
    const float legLength=std::min(upper[0]+lower[0],upper[1]+lower[1]);
    headDelta=LimitLength(headDelta,scale*.85f);
    Vec shift=LimitLength({headDelta.x*.35f,0,headDelta.z*.35f},scale*.20f);
    shift.y=std::clamp(headDelta.y*.9f,-scale*.12f,legLength*.65f);
    // Start at the feasible animated pelvis and move only as far as BOTH legs
    // can reach. This keeps feet planted without stretching either leg, even
    // on steps, in a split running stride, or with unequal bone lengths.
    auto reachable=[&](float t) {
        for(int leg=0;leg<2;++leg) {
            const int first=1+leg*3;
            const float d=Length(Sub(native[first+2].origin,Add(native[first].origin,Scale(shift,t))));
            if(d>upper[leg]+lower[leg]+.001f || d<std::fabs(upper[leg]-lower[leg])-.001f) return false;
        }
        return true;
    };
    // The first intersection along this ray is the physical limit. Sampling
    // also catches a path through the inner (fully folded) unreachable sphere.
    float valid=0;
    for(int step=1;step<=32;++step) {
        const float next=step/32.f;
        if(reachable(next)) { valid=next;continue; }
        float hi=next;
        for(int i=0;i<16;++i) { const float mid=(valid+hi)*.5f;if(reachable(mid)) valid=mid;else hi=mid; }
        break;
    }
    shift=Scale(shift,valid);
    for(int i=0;i<15;++i) posed[i]=native[i];
    posed[0].origin=Add(native[0].origin,shift);
    const Vec spine=Sub(native[14].origin,native[7].origin);
    if(Length(spine)<1.e-3f) return false;
    constexpr float radians=3.14159265358979323846f/180.f;
    const Basis lean=LimitRotation(Align(spine,Add(spine,Sub(headDelta,shift))),35*radians);
    // Share a bounded fraction of head yaw with the chest. Pelvis orientation
    // remains with locomotion so looking sideways does not rotate planted feet.
    const Vec facing=Transform(headRotation,forward);
    const float yaw=std::clamp(std::atan2(Cross(forward,facing).y,Dot(forward,facing))*.35f,-20*radians,20*radians);
    const Basis torso=Multiply(RotationMatrix(AxisRotation({0,1,0},yaw)),lean);
    const Vec pivot=native[7].origin;
    const Frame chest{torso,Sub(Add(pivot,shift),Transform(torso,pivot))};
    for(int i=7;i<15;++i) posed[i]=Multiply(chest,native[i]);
    posed[14].basis=Multiply(headRotation,native[14].basis);
    for(int leg=0;leg<2;++leg) {
        const int first=1+leg*3;
        Frame chain[3]={native[first],native[first+1],native[first+2]},correction[3]{};
        for(auto& joint:chain) joint.origin=Add(joint.origin,shift);
        // Keep the authored knee side when it has a clear bend; use forward
        // for near-straight legs, where the native pole is underdetermined.
        Vec axis{};Unit(Sub(native[first+2].origin,native[first].origin),axis);
        const Vec knee=Sub(native[first+1].origin,native[first].origin);
        Vec pole=Sub(knee,Scale(axis,Dot(knee,axis)));
        if(Length(pole)<legLength*.05f) pole=forward;
        if(!SolveArm(chain,native[first+2],pole,correction,nullptr,nullptr,false)) return false;
        for(int j=0;j<3;++j) posed[first+j]=Multiply(correction[j],chain[j]);
        // Never accumulate numerical changes to the floor contact/orientation.
        posed[first+2]=native[first+2];
    }
    for(const auto& joint:posed) if(!Finite(joint)) return false;
    return true;
}
} // namespace tr::firstperson
