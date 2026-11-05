#pragma once
#include "MotionGunMath.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace tr::weaponcollision {
using namespace motiongun;
inline float Length(Vec v) { return std::sqrt(Dot(v,v)); }
inline bool Finite(Vec v) { return std::isfinite(v.x+v.y+v.z); }
// Conservative barrel/hand samples queried against room geometry. The callback
// returns a clipped point on the segment, without firing or damaging anything.
// Correct translation only: arm IK and all weapon/muzzle users share this wrist.
template<class Trace>
Vec Retract(Vec eye,const Frame& wrist,Vec muzzle,float units,Trace trace) {
    if(!Finite(eye) || !Finite(wrist.origin) || !Finite(muzzle) || !std::isfinite(units) || units<=1) return {};
    const float radius=units*.05f,margin=std::max(2.f,units*.01f);
    const Vec samples[]={ {},Scale(muzzle,.5f),muzzle,Add(muzzle,{radius,0,0}),
        Add(muzzle,{-radius,0,0}),Add(muzzle,{0,0,radius}),Add(muzzle,{0,0,-radius}),
        {radius,0,0},{-radius,0,0} };
    Vec offset{};
    for(int pass=0;pass<6;++pass) {
        Vec correction{};float largest=0;
        for(const auto local:samples) {
            const Vec end=Add(Transform(wrist,local),offset),ray=Sub(end,eye);
            const float distance=Length(ray);Vec hit{};
            if(distance<margin || distance>units*4 || !trace(eye,end,hit) || !Finite(hit)) continue;
            const Vec along=Sub(hit,eye);
            // Reject corrupt/out-of-segment native results rather than moving
            // a tracked weapon to an unrelated room or render coordinate frame.
            if(Dot(along,ray)<-1 || Dot(along,ray)>distance*distance+distance*2 ||
               Length(Sub(along,Scale(ray,Dot(along,ray)/(distance*distance))))>4) continue;
            const Vec delta=Sub(Sub(hit,end),Scale(ray,margin/distance));
            const float amount=Length(delta);
            if(amount>largest) { largest=amount;correction=delta; }
        }
        if(largest<.01f) break;
        const Vec next=Add(offset,correction);
        if(!Finite(next) || Length(next)>units*2) break;
        offset=next;
    }
    return offset;
}
struct Cache {
    bool valid=false;
    uint64_t serial=0,base=0;
    const void* owner=nullptr;
    int level=-1,weapon=0;float units=0;
    Frame raw{};Vec eye{},muzzle{},offset{};
    template<class Trace>
    Vec Update(uint64_t frame,uint64_t address,const void* item,int roomLevel,int gun,
               float scale,Vec view,const Frame& wrist,Vec tip,Trace trace) {
        if(!valid || serial!=frame || base!=address || owner!=item || level!=roomLevel || weapon!=gun || units!=scale ||
           std::memcmp(&raw,&wrist,sizeof(raw)) || std::memcmp(&eye,&view,sizeof(eye)) || std::memcmp(&muzzle,&tip,sizeof(tip))) {
            offset=Retract(view,wrist,tip,scale,trace);
            valid=true;serial=frame;base=address;owner=item;level=roomLevel;weapon=gun;units=scale;raw=wrist;eye=view;muzzle=tip;
        }
        return offset;
    }
};
}
