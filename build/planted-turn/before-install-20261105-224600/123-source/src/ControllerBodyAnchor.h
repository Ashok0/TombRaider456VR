#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Controller POSITIONS, not their aim. Require coordinated head/hand travel.
// A fixed reference and separate entry/continuation thresholds reject noise without
// mistaking a slow cumulative physical step for stationary hands.
struct ControllerBodyAnchor {
    PhysicalBodyTurn physicalTurn;
    RoomscaleHeadingRecovery heading;
    float turnOutput=0,turnEvidence=0;double lastTurnTime=0;Vec turnHands[2]{};
    bool valid=false,held=false,rotating=false;
    uint64_t serial=0;
    double time=0,moved=0,referenceTime=0,rotated=0;
    float base=0,yaw=0,height=0,motionYaw=0;
    int reason=0;float handTravel[2]{};
    Vec hands[2]{},offset{},head{},motionEye{};
    void Reset() { *this={}; }
    void Update(uint64_t frame,double now,Vec eye,Vec left,Vec right,float artificial,float bodyYaw,bool tracked,bool walking,float headYaw=0) {
        if(!tracked || !Finite({IdentityBasis(),eye}) || !Finite({IdentityBasis(),left}) ||
           !Finite({IdentityBasis(),right}) || !std::isfinite(now+artificial+bodyYaw+headYaw)) {Reset();return;}
        if(valid && serial==frame && artificial==base) return;
        const bool turn=valid && std::fabs(BodyAngle(artificial-base))>1.e-6f;
        if(!valid || now<time || now-time>.5 || turn) {
            *this={};valid=true;serial=frame;time=moved=referenceTime=rotated=now;base=artificial;
            hands[0]=left;hands[1]=right;head=motionEye=eye;motionYaw=headYaw;yaw=bodyYaw;height=eye.y;reason=turn ? 4 : 0;
            physicalTurn.Observe(frame,now,headYaw,Sub(left,eye),Sub(right,eye),true);
            turnHands[0]=left;turnHands[1]=right;lastTurnTime=now;
            heading.Update(eye,left,right,bodyYaw,headYaw,now);return;
        }
        const float dt=float(std::clamp(now-time,0.,.05));serial=frame;time=now;turnOutput=0;
        if(now-rotated>=.12 || walking) rotating=false;
        if(walking) heading={};
        heading.Update(eye,left,right,bodyYaw,headYaw,now);
        if(heading.established || rotating) yaw=bodyYaw;
        const Vec delta[2]={Flat(Sub(left,hands[0])),Flat(Sub(right,hands[1]))};
        const Vec headDelta=Flat(Sub(eye,motionEye)),common=Scale(Add(delta[0],delta[1]),.5f);
        const float h=Length(headDelta),c=Length(common),a=Length(delta[0]),b=Length(delta[1]);
        handTravel[0]=a;handTravel[1]=b;
        // Releasing an established hold needs substantial, coordinated motion.
        // Small independent hand drift is not evidence that the torso moved.
        // Once moving, a lower continuation threshold preserves slow roomscale.
        const float threshold=held ? .025f : .003f;
        const float turnDelta=BodyAngle(headYaw-motionYaw);
        // Two hands must describe the same rigid motion. Averaging unrelated
        // drift (e.g. 40 cm on one hand and 3 cm on the other) invents a step.
        const bool translationCoherent=a>0 && b>0 && h>0 && c>0 &&
            Dot(delta[0],delta[1])>.95f*a*b && Dot(common,headDelta)>.95f*c*h &&
            Length(Sub(delta[0],delta[1]))<std::max(.008f,c*.25f) &&
            Length(Sub(common,headDelta))<std::max(.012f,h*.25f) && std::fabs(turnDelta)<.06f;
        const bool translation=translationCoherent && a>=threshold && b>=threshold && c>=threshold && h>=threshold;
        // Use the same confirmed angular evidence as the rendered follower.
        // A slow turn may take longer than the translation detector's 250 ms
        // reference window to move both hands 2.5 cm. Do not repeatedly erase it.
        const float confirmedTurn=physicalTurn.Observe(frame,now,headYaw,Sub(left,eye),Sub(right,eye),true);
        if(now-lastTurnTime>.25 || walking) {turnEvidence=0;turnHands[0]=left;turnHands[1]=right;}
        if(confirmedTurn!=0) {turnEvidence=BodyAngle(turnEvidence+confirmedTurn);lastTurnTime=now;}
        // Every angular update needs fresh positional travel from both hands,
        // including continuation. A 120 ms rotating flag is not permission to
        // apply headset-only samples or previously queued head angles.
        const float turnThreshold=.025f;
        const bool rotation=!walking && confirmedTurn!=0 && (rotating || std::fabs(turnEvidence)>.06f) &&
            Length(Flat(Sub(left,turnHands[0])))>=turnThreshold && Length(Flat(Sub(right,turnHands[1])))>=turnThreshold;
        if(rotation) {
            const Vec before=Sub(turnHands[1],turnHands[0]),after=Sub(right,left);
            // Absolute recovery and incremental turns must never both spend
            // the same controller motion. The emitted angle is controller-owned.
            if(!heading.established) turnOutput=BodyAngle(std::atan2(after.x,after.z)-std::atan2(before.x,before.z));
            turnEvidence=0;turnHands[0]=left;turnHands[1]=right;rotating=true;rotated=now;
        }
        if(walking || translation || rotation) {
            // Move the body by corroborated travel, never erase an existing
            // head-only lean merely because a hand-motion gate released.
            // Angular evidence never consumes HMD translation or unlocks the
            // positional/posture hold. Only an actual shared step moves it.
            if(translation && !walking) head=Add(head,common);
            hands[0]=left;hands[1]=right;motionEye=eye;motionYaw=headYaw;referenceTime=now;
            if(walking || translation) {moved=now;held=false;}
            reason=walking ? 3 : rotation ? 2 : 1;
        }
        if(!walking && !translation && now-moved>=.12) {
            if(!held) {held=true;height=eye.y;yaw=bodyYaw;}
        }
        // Refresh only the movement detector after unrelated head/hand motion.
        // Keep the held body anchor untouched. Old gestures cannot accumulate
        // for seconds and become a body step when the head happens to pass by.
        if(!walking && !translationCoherent && confirmedTurn==0 && now-referenceTime>=.25) {
            hands[0]=left;hands[1]=right;motionEye=eye;motionYaw=headYaw;referenceTime=now;
        }
        if(!walking) offset=Flat(Sub(eye,head));
        else {
            // Explicit stick travel recentres smoothly; physical hand movement
            // must never release the entire accumulated head-only displacement.
            const float length=Length(offset);
            if(length>0) offset=Scale(offset,std::max(0.f,1-dt/length));
            head=Sub(eye,offset);height=eye.y;yaw=bodyYaw;
        }
    }
};
}
