#pragma once
#include "MotionGunMath.h"
#include <algorithm>
#include <cmath>

namespace tr::weaponcollision {
using namespace motiongun;
inline float Length(Vec v) { return std::sqrt(Dot(v,v)); }
inline bool Finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline float Clamp(float value) { return std::clamp(value,0.f,1.f); }
// Closest points on two finite barrel segments, including parallel/zero-length cases.
inline void Closest(Vec a,Vec b,Vec c,Vec d,Vec& left,Vec& right) {
    const Vec u=Sub(b,a),v=Sub(d,c),w=Sub(a,c);
    const float uu=Dot(u,u),vv=Dot(v,v),uv=Dot(u,v),uw=Dot(u,w),vw=Dot(v,w);
    float s=0,t=0;
    if(uu<1.e-6f) t=vv>1.e-6f ? Clamp(vw/vv) : 0;
    else if(vv<1.e-6f) s=Clamp(-uw/uu);
    else {
        const float denominator=uu*vv-uv*uv;
        if(denominator>1.e-6f*uu*vv) s=Clamp((uv*vw-uw*vv)/denominator);
        t=(uv*s+vw)/vv;
        if(t<0) {t=0;s=Clamp(-uw/uu);}
        else if(t>1) {t=1;s=Clamp((uv-uw)/uu);}
    }
    left=Add(a,Scale(u,s));right=Add(c,Scale(v,t));
}
// Symmetric capsule separation computed exclusively from the two raw tracked
// wrists. Both render orders and both eyes therefore get the same correction.
// No room queries, controller mutation, aim rotation or frame-history feedback.
inline Vec LeftOffset(const Frame& left,const Frame& right,Vec leftTip,Vec rightTip,float units) {
    if(!std::isfinite(units) || units<=1 || !Finite(left.origin) || !Finite(right.origin)) return {};
    const Vec a=left.origin,b=Transform(left,leftTip),c=right.origin,d=Transform(right,rightTip);
    if(!Finite(b) || !Finite(d)) return {};
    Vec p{},q{};Closest(a,b,c,d,p,q);
    Vec normal=Sub(p,q);const float distance=Length(normal),clearance=units*.06f;
    if(!std::isfinite(distance) || distance>=clearance) return {};
    if(distance>units*.00001f) normal=Scale(normal,1/distance);
    else {
        // Intersecting barrels need a deterministic normal perpendicular to both.
        const Vec u=Sub(b,a),v=Sub(d,c);
        normal={u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        float length=Length(normal);
        if(length<1.e-4f) {
            normal=Transform(left.basis,{-1,0,0});
            const float square=Dot(u,u);
            if(square>1.e-6f) normal=Sub(normal,Scale(u,Dot(normal,u)/square));
            length=Length(normal);
            if(length<1.e-4f) { normal={0,0,1};length=1; }
        }
        normal=Scale(normal,1/length);
    }
    return Scale(normal,(clearance-distance)*.5f);
}
}
