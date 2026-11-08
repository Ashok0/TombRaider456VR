#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Controller POSITIONS, not their aim. Require coordinated head/hand travel.
// A fixed reference and separate entry/continuation thresholds reject noise without
// mistaking a slow cumulative physical step for stationary hands.
struct ControllerBodyAnchor {
    RoomscaleHeadingRecovery heading;
    float turnOutput=0; // Facing is supplied exclusively as an absolute target.
    bool valid=false,held=false,rotating=false,translating=false;
    uint64_t serial=0;
    double time=0,moved=0,referenceTime=0,rotated=0,translated=0;
    float base=0,yaw=0,height=0,motionYaw=0;
    int reason=0;float handTravel[2]{};
    Vec hands[2]{},offset{},head{},motionEye{},consumedTravel{};
    void Reset() { *this={}; }
    void Update(uint64_t frame,double now,Vec eye,Vec left,Vec right,float artificial,float bodyYaw,bool tracked,bool walking,float headYaw=0,float neckMetres=.15f) {
        if(!tracked || !Finite({IdentityBasis(),eye}) || !Finite({IdentityBasis(),left}) ||
           !Finite({IdentityBasis(),right}) || !std::isfinite(now+artificial+bodyYaw+headYaw+neckMetres)) {Reset();return;}
        // Remove the ordinary eye-around-neck arc from the step evidence.
        // Looking aside while walking must not veto a real shared translation.
        const Vec walkEye=Sub(eye,YawVector({0,0,std::clamp(neckMetres,0.f,.4f)},headYaw));
        if(valid && serial==frame && artificial==base) return;
        const bool turn=valid && std::fabs(BodyAngle(artificial-base))>1.e-6f;
        if(!valid || now<time || now-time>.5 || turn) {
            *this={};valid=true;serial=frame;time=moved=referenceTime=rotated=now;base=artificial;
            hands[0]=left;hands[1]=right;head=eye;motionEye=walkEye;motionYaw=headYaw;yaw=bodyYaw;height=eye.y;reason=turn ? 4 : 0;
            heading.Update(eye,left,right,bodyYaw,headYaw,now,head);return;
        }
        const float dt=float(std::clamp(now-time,0.,.05));serial=frame;time=now;turnOutput=0;
        if(now-rotated>=.12 || walking) rotating=false;
        if(walking) heading={};
        heading.Update(eye,left,right,bodyYaw,headYaw,now,head);
        if(heading.sampleAccepted) head=heading.anchorTarget;
        rotating=heading.turning;
        if(heading.established || rotating) yaw=bodyYaw;
        // Walking owns an independent observation window. Remove the rigid
        // turn around the neck before comparing travel; curved walking does
        // not produce parallel raw controller displacements.
        const Vec beforePair=Flat(Sub(hands[1],hands[0])),afterPair=Flat(Sub(right,left));
        const float beforeSpan=Length(beforePair),afterSpan=Length(afterPair);
        const Vec headDelta=Flat(Sub(walkEye,motionEye));
        const float h=Length(headDelta),headTurn=BodyAngle(headYaw-motionYaw);
        Vec delta[2]{},common{};float best=INFINITY;
        auto candidate=[&](float angle) {
            if(std::fabs(angle)>.7f) return;
            const Vec d[2]={Flat(Sub(left,Add(motionEye,YawVector(Sub(hands[0],motionEye),angle)))),
                            Flat(Sub(right,Add(motionEye,YawVector(Sub(hands[1],motionEye),angle))))};
            const Vec shared=Scale(Add(d[0],d[1]),.5f);
            const float error=Length(Sub(shared,headDelta));
            if(error<best) {best=error;common=shared;delta[0]=d[0];delta[1]=d[1];}
        };
        // Fit the shared displacement with no rotation, controller rotation,
        // or corroborated head rotation. Opposite arm swing must not corrupt
        // the turn estimate used to recognize a real shared step.
        candidate(0);bool headLeading=false;
        if(beforeSpan>.05f && afterSpan>.05f) {
            const float pairTurn=BodyAngle(std::atan2(afterPair.x,afterPair.z)-std::atan2(beforePair.x,beforePair.z));
            headLeading=std::fabs(headTurn)>.06f && std::fabs(pairTurn)<std::fabs(headTurn)*.5f;
            if(!headLeading && std::fabs(BodyAngle(pairTurn-headTurn))<.15f) candidate(pairTurn);
            if(!headLeading && std::fabs(BodyAngle(pairTurn-headTurn))<.08f) candidate(headTurn);
        }
        const float c=Length(common);
        const float rawTravel[2]={Length(Flat(Sub(left,hands[0]))),Length(Flat(Sub(right,hands[1])))};
        handTravel[0]=rawTravel[0];handTravel[1]=rawTravel[1];
        const float threshold=held ? .025f : .003f;
        // Both real controllers must travel; removing a head rotation from
        // fixed hands is never sufficient evidence. Allow modest arm swing
        // around the common step, but reject large asymmetric gestures.
        const bool translationCoherent=h>0 && c>0 &&
            (!headLeading || (Dot(delta[0],delta[1])>.95f*Length(delta[0])*Length(delta[1]) &&
                Length(Sub(delta[0],delta[1]))<std::max(.008f,c*.25f))) &&
            Dot(common,headDelta)>.95f*c*h &&
            Length(Sub(common,headDelta))<std::max(.012f,h*.25f) &&
            Length(Sub(delta[0],headDelta))<std::max(headLeading ? .012f : .035f,h*(headLeading ? .25f : .5f)) &&
            Length(Sub(delta[1],headDelta))<std::max(headLeading ? .012f : .035f,h*(headLeading ? .25f : .5f));
        const bool translation=translationCoherent && c>=threshold && h>=threshold &&
            rawTravel[0]>=threshold*.35f && rawTravel[1]>=threshold*.35f &&
            Length(Flat(Scale(Add(Sub(left,hands[0]),Sub(right,hands[1])),.5f)))>=std::max(threshold*.8f,h*.5f);
        if(translation) {translating=true;translated=now;}
        else if(now-translated>=.12) translating=false;
        // There is only one authority for physical facing: the planted-stance
        // classifier. Fine head/hand correlation cannot bypass its confidence.
        const bool rotation=!walking && heading.sampleAccepted;
        if(rotation) {rotated=now;consumedTravel=common;}
        if(walking || translation || rotation) {
            // Move the body by corroborated travel, never erase an existing
            // head-only lean merely because a hand-motion gate released.
            // A confirmed turn transports the anchor above as a rigid pose;
            // it does not release the positional/posture hold to headset sway.
            if(translation && !walking && !rotation) head=Add(head,Sub(common,consumedTravel));
            if(walking || translation) {
                hands[0]=left;hands[1]=right;motionEye=walkEye;motionYaw=headYaw;referenceTime=now;consumedTravel={};
            }
            if(walking || translation) {moved=now;held=false;}
            reason=walking ? 3 : rotation ? 2 : 1;
        }
        if(!walking && !translation && now-moved>=.12) {
            if(!held) {held=true;height=eye.y;yaw=bodyYaw;}
        }
        // Refresh only the movement detector after unrelated head/hand motion.
        // Keep the held body anchor untouched. Old gestures cannot accumulate
        // for seconds and become a body step when the head happens to pass by.
        if(!walking && !translationCoherent && now-referenceTime>=.25) {
            hands[0]=left;hands[1]=right;motionEye=walkEye;motionYaw=headYaw;referenceTime=now;consumedTravel={};
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
