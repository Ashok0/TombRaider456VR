#pragma once
#include "SwitchPosePath.h"
#include "MotionGunMath.h"
#include <algorithm>
#include <cmath>

namespace tr::firstperson {
// Airlock valves finish by entering their authored post-use clip, not by an
// animation DEACTIVATE command. Native SwitchControl fires the linked items
// at last-1 in the turning clip; the later clip may move/drop the valve.
inline bool BuildValvePath(SwitchPosePath& path,const SwitchAnim* animations,int base,SwitchPose start) {
    path.count=0;
    if(!animations || base<0 || start.animation!=base || start.state!=0) return false;
    const auto& idle=animations[base];const auto& turn=animations[base+1];const auto& end=animations[base+2];
    if(!idle.frames || !turn.frames || !end.frames || idle.state!=0 || idle.first!=idle.last || start.frame!=idle.first ||
       idle.jump!=base+1 || idle.jumpFrame!=turn.first ||
       turn.state!=1 || turn.last-turn.first!=165 || (turn.interpolation&255)!=1 ||
       turn.jump!=base+2 || turn.jumpFrame!=end.first || end.state!=1 ||
       turn.velocity || turn.acceleration || turn.xVelocity || turn.xAcceleration || end.first<0) return false;
    path.poses[path.count++]=start;
    for(int f=turn.first;f<=turn.last;++f) path.poses[path.count++]={int16_t(base+1),int16_t(f),1};
    path.poses[path.count++]={int16_t(base+2),end.first,1};
    return true;
}

inline motiongun::Vec ValveRimContact(motiongun::Vec hand,bool markets) {
    // Measured centre lines inside both installed AIRLOCK_SWITCH rim meshes.
    const float cx=markets ? 3.f : 0.f,cy=6.4f,z=markets ? -83.f : -82.f;
    const float x=hand.x-cx,y=hand.y-cy,r=std::hypot(x,y);
    if(!std::isfinite(r) || r<1.f) return {cx+112.f,cy,z};
    return {cx+112.f*x/r,cy+112.f*y/r,z};
}

struct ValveTurn {
    float angles[256]{},progress=0,travel=0,previous=0,direction=1;
    int count=0;
    bool Build(const float* samples,int size) {
        *this={};
        if(!samples || size<3 || size>256 || !std::isfinite(samples[0])) return false;
        float accumulated=0;
        for(int i=1;i<size;++i) {
            const float delta=std::remainder(samples[i]-samples[i-1],2*3.14159265358979323846f);
            if(!std::isfinite(delta) || std::fabs(delta)>.75f) return false;
            accumulated+=delta;angles[i]=accumulated;
        }
        if(std::fabs(accumulated)<1.f || std::fabs(accumulated)>12.6f) return false;
        direction=accumulated<0 ? -1.f : 1.f;
        for(int i=1;i<size;++i) {
            const float a=angles[i]*direction;
            // Remove small authored hand-settling reversals, not real turns.
            if(a<angles[i-1]-.05f) return false;
            angles[i]=std::max(angles[i-1],a);
        }
        count=size;return true;
    }
    void Grab(float angle) { previous=angle;travel=progress; }
    bool Track(float angle) {
        const float delta=std::remainder(angle-previous,2*3.14159265358979323846f);
        if(!count || !std::isfinite(delta) || std::fabs(delta)>.75f) return false;
        previous=angle;travel+=direction*delta;
        // Keep excess motion at the start stop as debt, so moving the wrong
        // way and returning cannot manufacture a turn or shift the grip.
        progress=std::clamp(travel,0.f,angles[count-1]);return true;
    }
    int Pose() const {
        if(!count || progress<.005f) return 0;
        int best=0;float distance=1.e30f;
        for(int i=0;i<count;++i) {
            const float d=std::fabs(angles[i]-progress);
            if(d<distance-1.e-5f) { distance=d;best=i; }
        }
        return best;
    }
    bool Complete() const { return count && progress>=angles[count-1]-.015f; }
};
}
