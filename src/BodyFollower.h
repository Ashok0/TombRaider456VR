#pragma once
#include "FirstPersonBodyIK.h"
#include <cstdint>

namespace tr::firstperson {
inline float BodyAngle(float angle) { return std::remainder(angle,6.28318530718f); }
inline Vec Flat(Vec v) { return {v.x,0,v.z}; }
inline Vec YawVector(Vec v,float yaw) {
    return {v.x*std::cos(yaw)+v.z*std::sin(yaw),v.y,-v.x*std::sin(yaw)+v.z*std::cos(yaw)};
}
struct BodyFollowSettings {
    float yawSlack=.785398163f, yawRelease=.523598776f, yawSpeed=2.0943951f;
    float leanRadius=.15f, leanRelease=.09f, followSpeed=.9f;
};
// Capture once in an ordinary standing pose. Size is not a per-frame IK
// degree of freedom: ducking, walking and leaning must never shrink the body.
struct BodySizeCalibration {
    bool valid=false;
    float factor=1,units=0;
    void Reset() { *this={}; }
    bool Capture(const Frame (&native)[15],Vec anchor,float standingEyeY,float worldUnits) {
        if(valid) return true;
        if(!std::isfinite(standingEyeY+worldUnits) || worldUnits<=1) return false;
        for(const auto& joint:native) if(!Finite(joint)) return false;
        const float floor=std::max(native[3].origin.y,native[6].origin.y);
        const float available=floor-standingEyeY;
        float relaxedHipHeight=0;
        for(int first:{1,4}) {
            const float a=Length(Sub(native[first+1].origin,native[first].origin));
            const float b=Length(Sub(native[first+2].origin,native[first+1].origin));
            if(a<.001f || b<.001f) return false;
            // Near full extension defines standing stature. Even 1% of leg
            // length left unused produces a visible ~16-degree knee bend.
            relaxedHipHeight+=(a+b)*.99995f+native[first].origin.y-native[0].origin.y;
        }
        const float stature=relaxedHipHeight*.5f+native[0].origin.y-anchor.y;
        if(!std::isfinite(stature+available) || stature<worldUnits*.5f || available<worldUnits*.5f) return false;
        factor=std::clamp(available/stature,.5f,1.5f);units=worldUnits;valid=true;return true;
    }
};
// Evaluate hands in the BODY frame, never the current gaze frame. Looking
// aside cannot turn two stationary forward controllers into a new body hint.
inline bool HandsSupportFacing(Vec left,Vec right,Vec leftAim,Vec rightAim,float yaw) {
    const Vec forward{std::sin(yaw),0,std::cos(yaw)};
    Vec a{},b{};
    return Unit(Flat(leftAim),a) && Unit(Flat(rightAim),b) &&
        Dot(Flat(left),forward)>.15f && Dot(Flat(right),forward)>.15f &&
        Dot(a,forward)>.8f && Dot(b,forward)>.8f;
}
inline float BodyChestTwist(float headTwist,bool handsForward) {
    if(handsForward) return 0;
    const float excess=std::max(0.f,std::fabs(headTwist)-.523598776f); // Neck owns first 30 degrees.
    return std::copysign(std::min(excess*.35f,.34906585f),headTwist);
}
// World positions are metres. Update once per tracked pose, independent of
// skin/hand/holster passes and stereo eyes. Artificial turns are applied whole.
struct BodyFollower {
    bool valid=false,turning=false,stepping=false,supportedHands=false;
    uint64_t serial=0;
    double time=0;
    float yaw=0,base=0,phase=0,stepWeight=0;
    Vec root{},nativeRoot{};
    void Reset() { *this={}; }
    void Update(uint64_t frame,double now,float artificialYaw,float headYaw,float travelYaw,
                Vec goal,Vec native,bool moving,const BodyFollowSettings& s,Vec turnPivot={},bool handsForward=false) {
        if(!std::isfinite(now+artificialYaw+headYaw+travelYaw) ||
           !Finite({IdentityBasis(),goal}) || !Finite({IdentityBasis(),native})) { Reset();return; }
        if(!valid || now<time || now-time>.25 || Length(Sub(native,nativeRoot))>2) {
            valid=true;turning=stepping=false;serial=frame;time=now;
            yaw=headYaw;base=artificialYaw;root=Flat(goal);nativeRoot=native;stepWeight=0;
            supportedHands=handsForward;return;
        }
        const float turn=BodyAngle(artificialYaw-base);
        if(turn!=0) {
            yaw=BodyAngle(yaw+turn);
            root=Add(Flat(turnPivot),YawVector(Sub(root,Flat(turnPivot)),turn));
        }
        base=artificialYaw;
        if(serial==frame) return;
        supportedHands=handsForward;
        const float dt=float(std::clamp(now-time,0.0,.05));time=now;serial=frame;
        // Artificial travel carries the body; physical root consumption must
        // not count the same HMD displacement a second time.
        if(moving) root=Add(root,Flat(Sub(native,nativeRoot)));
        nativeRoot=native;
        const float delta=BodyAngle(headYaw-yaw);
        const bool supported=handsForward && !moving && std::fabs(delta)<1.3962634f;
        if(supported) turning=false;
        if(!supported && !turning && std::fabs(delta)>s.yawSlack) turning=true;
        if(turning && std::fabs(delta)<=s.yawRelease+.001f) turning=false;
        const float target=moving ? travelYaw : turning ? travelYaw-std::copysign(s.yawRelease,delta) : yaw;
        const float change=std::clamp(BodyAngle(target-yaw)*(1-std::exp(-10*dt)),-s.yawSpeed*dt,s.yawSpeed*dt);
        yaw=BodyAngle(yaw+change);
        const Vec offset=Flat(Sub(goal,root));const float distance=Length(offset);
        if(!stepping && distance>s.leanRadius) stepping=true;
        if(stepping && distance<=s.leanRelease+.001f) stepping=false;
        Vec move{};
        if(distance>0 && (moving || stepping)) {
            const float excess=std::max(0.f,distance-(moving ? 0.f : s.leanRelease));
            move=Scale(offset,std::min(excess,s.followSpeed*dt)/distance);root=Add(root,move);
        }
        const float stride=Length(move)+std::fabs(change)*.12f;
        phase=std::fmod(phase+stride*15.f,6.28318530718f);
        const float weight=!moving && stride>.0001f ? 1.f : 0.f;
        stepWeight+=(weight-stepWeight)*(1-std::exp(-12*dt));
    }
};
struct RoomscaleFollower {
    bool following=false;
    Vec Request(Vec offset,float dt,const BodyFollowSettings& s) {
        offset=Flat(offset);const float distance=Length(offset);
        if(!std::isfinite(distance) || distance>2 || !std::isfinite(dt)) { following=false;return {}; }
        if(!following && distance>s.leanRadius) following=true;
        if(following && distance<=s.leanRelease+.001f) following=false;
        if(!following || distance==0) return {};
        return Scale(offset,std::min(std::max(0.f,distance-s.leanRelease),s.followSpeed*std::clamp(dt,0.f,.05f))/distance);
    }
};
struct HeightCrouch {
    bool active=false;
    bool Update(bool enabled,float drop) {
        if(!enabled || !std::isfinite(drop)) active=false;
        else if(!active && drop>=.35f) active=true;
        else if(active && drop<=.23f) active=false;
        return active;
    }
};
} // namespace tr::firstperson
