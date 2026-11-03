#pragma once
#include "MotionGunMath.h"
#include <algorithm>
#include <cmath>

namespace tr::firstperson {
// The native cog advances one peg (1/16 revolution) per 54-frame pull.
// Ignore the authored anticipation frames when matching physical rotation.
constexpr float CogPegAngle=3.14159265358979323846f/8;
struct CogDrive {
    float angle=0,previous=0;
    bool tracked=false;
    void Grab(float handAngle) { previous=handAngle;tracked=true; }
    void Release() { tracked=false; }
    bool Track(float handAngle,float& advance) {
        advance=0;
        if(!tracked || !std::isfinite(handAngle)) return false;
        float delta=std::remainder(handAngle-previous,2*3.14159265358979323846f);
        // Tracking jumps must release the hand, never spin the wheel or door.
        if(!std::isfinite(delta) || std::fabs(delta)>.7f) { Release();return false; }
        previous=handAngle;
        // Ratchet: reversing a hand must not create additional credited travel
        // when it returns to the same place. Keep that reverse travel as debt.
        angle+=delta;
        advance=std::max(0.f,angle);
        angle-=advance;
        return true;
    }
};

// Centre lines of the actual radial pegs in the shared cog skeleton. The
// three-joint model is the half wheel; the four-joint model has all 16 pegs.
inline motiongun::Vec CogPegContact(motiongun::Vec hand,bool fullWheel) {
    using namespace motiongun;
    Vec best{};float nearest=1.e30f;
    for(int i=0;i<(fullWheel ? 16 : 9);++i) {
        const float a=-float(i)*CogPegAngle;
        const Vec axis{std::cos(a),0,std::sin(a)};
        const Vec point=Scale(axis,std::clamp(Dot(hand,axis),550.f,765.f));
        const Vec delta=Sub(hand,point);const float distance=Dot(delta,delta);
        if(distance<nearest) { nearest=distance;best=point; }
    }
    return best;
}

// Locate the native pose at a physical angle. Flat anticipation poses are
// skipped only once the hand actually turns; standing still earns no pulse.
inline int CogPoseAtAngle(const float* angles,int count,float angle) {
    // Every cycle has a small physical dead band, including the first frame
    // after wrapping. Floating-point residue must not credit another pull.
    if(!angles || count<2 || !std::isfinite(angle) || angle<=.01f) return 0;
    int selected=0;
    for(int i=1;i<count && angles[i]<=angle;++i) selected=i;
    return selected;
}
}
