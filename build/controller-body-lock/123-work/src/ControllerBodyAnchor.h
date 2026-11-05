#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Controller POSITIONS, not their aim or their position relative to the head.
// A fixed reference plus a settling period rejects tracking noise without
// mistaking a slow cumulative physical step for stationary hands.
struct ControllerBodyAnchor {
    bool valid=false,held=false;
    uint64_t serial=0;
    double time=0,moved=0;
    float base=0,yaw=0,height=0;
    Vec hands[2]{},offset{},head{};
    void Reset() { *this={}; }
    void Update(uint64_t frame,double now,Vec eye,Vec left,Vec right,float artificial,float bodyYaw,bool tracked,bool walking) {
        if(!tracked || !Finite({IdentityBasis(),eye}) || !Finite({IdentityBasis(),left}) ||
           !Finite({IdentityBasis(),right}) || !std::isfinite(now+artificial+bodyYaw)) {Reset();return;}
        if(valid && serial==frame && artificial==base) return;
        const bool turn=valid && std::fabs(BodyAngle(artificial-base))>1.e-6f;
        if(!valid || now<time || now-time>.5 || turn) {
            *this={};valid=true;serial=frame;time=moved=now;base=artificial;
            hands[0]=left;hands[1]=right;head=eye;yaw=bodyYaw;height=eye.y;return;
        }
        const float dt=float(std::clamp(now-time,0.,.05));serial=frame;time=now;
        if(walking || Length(Sub(left,hands[0]))>.006f || Length(Sub(right,hands[1]))>.006f) {
            hands[0]=left;hands[1]=right;moved=now;held=false;
        } else if(now-moved>=.12) {
            if(!held) {held=true;head=Sub(eye,offset);height=eye.y;yaw=bodyYaw;}
        }
        if(held) offset=Flat(Sub(eye,head));
        else {
            // Release any head-only displacement smoothly when hands move.
            const float length=Length(offset);
            if(length>0) offset=Scale(offset,std::max(0.f,1-dt/length));
            head=Sub(eye,offset);height=eye.y;yaw=bodyYaw;
        }
    }
};
}
