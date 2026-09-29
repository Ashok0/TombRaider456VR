#pragma once
#include "LocomotionMath.h"

namespace tr::stabilization {
// Input polls latch a velocity; only rendered views integrate it. No yaw EMA,
// catch-up motion or release tail. Times are monotonic seconds supplied by the
// caller so mismatched input/render cadence can be tested deterministically.
struct RenderTurn {
    double sampleTime=0, frameTime=0;
    float rate=0;
    bool valid=false;
    void Reset() { *this={}; }
    void Sample(float next,double now) {
        if (!std::isfinite(next) || !std::isfinite(now)) { Reset(); return; }
        if (!valid || rate==0 || rate*next<0 || now<sampleTime || now-sampleTime>0.1)
            frameTime=now; // Never charge time before a press/reversal/resume.
        rate=next; sampleTime=now; valid=true;
    }
    float Step(double now) {
        if (!valid) return 0;
        if (!std::isfinite(now) || now<frameTime) { Reset(); return 0; }
        const double dt=std::clamp(now-frameTime,0.0,0.05);
        frameTime=now;
        if (now-sampleTime>0.1) return 0; // Lost polling must not leave a latched turn.
        return rate*float(dt);
    }
};

// Simulation-step filter, never a render/headset pose filter. New gaits and
// direction reversals start at native speed; zero motion stops immediately.
struct RootMotion {
    bool valid=false;
    int gait=-1;
    uint64_t action=0;
    float speed=0;
    locomotion::Vec direction{}, remainder{};
    void Reset() { *this={}; }
    bool Step(locomotion::Vec native, locomotion::Vec requested, int state,
              uint64_t intent, locomotion::Vec& result) {
        using namespace locomotion;
        const float sample=Length(native), n=Length(requested);
        if (!std::isfinite(sample) || !std::isfinite(n) || n<0.001f || sample>256) {
            Reset(); return false; // Animation commands/teleports remain native.
        }
        if (sample<0.001f) { Reset(); result={}; return true; }
        const Vec next=requested*(1/n);
        if (!valid || gait!=state || action!=intent ||
            next.x*direction.x+next.z*direction.z<0.5f) {
            speed=sample; remainder={};
        } else speed+=0.25f*(sample-speed);
        valid=true; gait=state; action=intent; direction=next;
        const Vec precise=next*speed+remainder;
        result={std::round(precise.x),std::round(precise.z)};
        remainder=precise-result;
        return true;
    }
};

struct Point { float x=0,y=0,z=0; };
// Hold the eye offset in the artificial-yaw frame across grounded gaits.
// Physical body yaw must not orbit this origin a second time. Lara's
// interpolated world root and physical headset translation are NOT filtered.
// Keep this reference while native non-ground animation drives the eye; only
// a new camera session/item/relocation should discard the standing reference.
struct GroundEye {
    bool valid=false;
    Point local{};
    void Reset() { *this={}; }
    Point Apply(Point root,float yaw,Point animated) {
        using namespace locomotion;
        if (!valid) {
            const auto flat=Rotate({animated.x-root.x,animated.z-root.z},-yaw);
            local={flat.x,animated.y-root.y,flat.z};
            valid=true;
        }
        const auto flat=Rotate({local.x,local.z},yaw);
        return {root.x+flat.x,root.y+local.y,root.z+flat.z};
    }
};
} // namespace tr::stabilization
