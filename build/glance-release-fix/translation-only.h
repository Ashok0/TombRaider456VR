#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Controller POSITIONS, not their aim. Require coordinated head/hand travel.
// A fixed reference and separate entry/continuation thresholds reject noise without
// mistaking a slow cumulative physical step for stationary hands.
// A physical step needs repeated directional agreement, not one frame where
// headset sway happens to coincide with controller noise. This state is separate
// from facing and keeps its own observation references across detector resets.
struct PhysicalStepIntent {
    bool valid=false,moving=false;int samples=0;
    Vec neck{},hands[2]{},direction{};
    double began=0,last=0;
    void Capture(Vec eye,Vec left,Vec right,double now) {
        valid=true;moving=false;samples=0;neck=eye;hands[0]=left;hands[1]=right;
        direction={};began=last=now;
    }
    bool Observe(Vec eye,Vec left,Vec right,double now,bool coherent) {
        if(!valid || now<last || now-last>.12) Capture(eye,left,right,now);
        const Vec travel=Flat(Sub(eye,neck));const float distance=Length(travel);
        if(coherent && distance>=.006f &&
           Length(Flat(Sub(left,hands[0])))>=.001f && Length(Flat(Sub(right,hands[1])))>=.001f) {
            const Vec next=Scale(travel,1/distance);
            if(samples==0 || Dot(next,direction)<.85f) {samples=1;began=now;moving=false;}
            else ++samples;
            direction=next;neck=eye;hands[0]=left;hands[1]=right;last=now;
            if(samples>=2 && now-began>=.04) moving=true;
        }
        return moving;
    }
};
struct ControllerBodyAnchor {
    RoomscaleHeadingRecovery heading;
    PhysicalStepIntent stepIntent;
    float turnOutput=0; // Facing is supplied exclusively as an absolute target.
    bool valid=false,held=false,rotating=false,translating=false;
    uint64_t serial=0;
    double time=0,moved=0,referenceTime=0,rotated=0,translated=0;
    float base=0,yaw=0,height=0,motionYaw=0;
    int reason=0;float handTravel[2]{};
    Vec hands[2]{},sampleHands[2]{},sampleEye{},offset{},head{},motionEye{},consumedTravel{};
    float sampleHeadYaw=0;double headOnlySince=-1;
    Vec placementHands[2]{},placementEye{},placementNeck{},placementAnchor{};
    float placementFacing=0,placementHeadYaw=0;
    Vec motionSampleHands[2]{};double handMotionTime=0;bool placementQuiet=false;
    void CapturePlacementEvidence(Vec eye,Vec left,Vec right,float headYaw) {
        placementHands[0]=left;placementHands[1]=right;placementEye=eye;placementHeadYaw=headYaw;
    }
    void CapturePlacement(Vec eye,Vec neck,Vec left,Vec right,float headYaw) {
        CapturePlacementEvidence(eye,left,right,headYaw);
        placementNeck=neck;placementAnchor=head;
        const Vec pair=Sub(right,left);placementFacing=std::atan2(pair.x,pair.z);
    }
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
            *this={};valid=true;held=true;serial=frame;time=moved=referenceTime=rotated=now;base=artificial;
            motionSampleHands[0]=sampleHands[0]=hands[0]=left;
            motionSampleHands[1]=sampleHands[1]=hands[1]=right;handMotionTime=now;
            head=eye;motionEye=walkEye;motionYaw=headYaw;yaw=bodyYaw;height=eye.y;reason=turn ? 4 : 0;
            CapturePlacement(eye,walkEye,left,right,headYaw);
            stepIntent.Capture(walkEye,left,right,now);
            sampleEye=eye;sampleHeadYaw=headYaw;
            heading.Update(eye,left,right,bodyYaw,headYaw,now,head);return;
        }
        // Evidence may span several frames, but a new head pose cannot spend
        // old controller travel after both controllers have stopped.
        const bool freshHands=Length(Flat(Sub(left,sampleHands[0])))>1.e-6f ||
            Length(Flat(Sub(right,sampleHands[1])))>1.e-6f;
        const Vec previousPair=Flat(Sub(sampleHands[1],sampleHands[0])),currentPair=Flat(Sub(right,left));
        const float handMotion[2]={Length(Flat(Sub(left,sampleHands[0]))),Length(Flat(Sub(right,sampleHands[1])))};
        const float currentHandTravel=std::max(handMotion[0],handMotion[1]);
        const float currentHeadTravel=Length(Flat(Sub(eye,sampleEye)));
        const float signedHeadTurn=BodyAngle(headYaw-sampleHeadYaw);
        const float currentHeadTurn=std::fabs(signedHeadTurn);
        const float signedHandTurn=Length(previousPair)>.05f && Length(currentPair)>.05f ?
            BodyAngle(std::atan2(currentPair.x,currentPair.z)-std::atan2(previousPair.x,previousPair.z)) : 0;
        const float currentHandTurn=std::fabs(signedHandTurn);
        // A pending turn checkpoint is not fresh evidence for this head pose.
        // If the headset moves independently now, retire that positional
        // checkpoint instead of applying the new glance to the old turn.
        const bool headOnlyTravel=currentHeadTravel>std::max(.0015f,currentHandTravel*2.f);
        const bool headOnlyMotion=headOnlyTravel ||
            (currentHandTravel<.003f && currentHeadTurn>.002f && currentHeadTurn>currentHandTurn*2.2f);
        if(headOnlyMotion) {if(headOnlySince<0) headOnlySince=now;}
        else headOnlySince=-1;
        const bool retirePlacement=headOnlyTravel && (now-headOnlySince>=.05 ||
            currentHeadTravel>std::max(.003f,currentHandTravel*2.f));
        const bool currentTurn=signedHeadTurn*signedHandTurn>0 &&
            std::fabs(signedHeadTurn-signedHandTurn)<std::max(.008f,currentHeadTurn*.5f);
        const Vec previousNeck=Sub(sampleEye,YawVector({0,0,std::clamp(neckMetres,0.f,.4f)},sampleHeadYaw));
        const Vec neckStep=Flat(Sub(walkEye,previousNeck));const float neckDistance=Length(neckStep);
        const Vec handStep[2]={Flat(Sub(left,sampleHands[0])),Flat(Sub(right,sampleHands[1]))};
        const bool currentStep=neckDistance>.0001f &&
            Dot(handStep[0],neckStep)>.95f*Length(handStep[0])*neckDistance &&
            Dot(handStep[1],neckStep)>.95f*Length(handStep[1])*neckDistance &&
            Length(Sub(handStep[0],neckStep))<std::max(.0005f,neckDistance*.35f) &&
            Length(Sub(handStep[1],neckStep))<std::max(.0005f,neckDistance*.35f);
        sampleEye=eye;sampleHeadYaw=headYaw;
        sampleHands[0]=left;sampleHands[1]=right;
        // Position confidence also expires when only millimetre tracking noise
        // remains. Retire the pending geometry, without changing the held body
        // or consuming any unapplied turn. Cumulative real motion renews it.
        if(Length(Flat(Sub(left,motionSampleHands[0])))>.006f ||
           Length(Flat(Sub(right,motionSampleHands[1])))>.006f) {
            motionSampleHands[0]=left;motionSampleHands[1]=right;handMotionTime=now;
        }
        const bool quietHands=now-handMotionTime>=.10;
        if(quietHands && !placementQuiet) CapturePlacementEvidence(eye,left,right,headYaw);
        placementQuiet=quietHands;
        const float dt=float(std::clamp(now-time,0.,.05));serial=frame;time=now;turnOutput=0;
        if(now-rotated>=.12 || walking) rotating=false;
        if(walking) heading={};
        heading.Update(eye,left,right,bodyYaw,headYaw,now,head);
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
        candidate(0);
        // A gaze/body mismatch persists after the short movement window is
        // refreshed. A sideways glance must not become an ordinary step just
        // because its next few degrees are small.
        const float controllerFacing=heading.valid && afterSpan>.05f ?
            BodyAngle(heading.bodyOrigin+std::atan2(afterPair.x,afterPair.z)-heading.facingOrigin) : bodyYaw;
        bool headLeading=std::fabs(BodyAngle(headYaw-controllerFacing))>.35f;
        if(beforeSpan>.05f && afterSpan>.05f) {
            const float pairTurn=BodyAngle(std::atan2(afterPair.x,afterPair.z)-std::atan2(beforePair.x,beforePair.z));
            const bool headLedIncrement=std::fabs(headTurn)>.06f && std::fabs(pairTurn)<std::fabs(headTurn)*.5f;
            headLeading=headLeading || headLedIncrement;
            if(!headLedIncrement && std::fabs(BodyAngle(pairTurn-headTurn))<.15f) candidate(pairTurn);
            if(!headLedIncrement && std::fabs(BodyAngle(pairTurn-headTurn))<.08f) candidate(headTurn);
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
        const bool stepConfirmed=stepIntent.Observe(walkEye,left,right,now,
            freshHands && !quietHands && translationCoherent);
        if(!walking && !stepConfirmed && !held) {held=true;height=eye.y;yaw=bodyYaw;}
        const bool translation=(!(headOnlyTravel || (currentHeadTurn>.025f && currentHeadTurn>currentHandTurn*2.2f)) || currentStep) && stepConfirmed && freshHands && !quietHands && translationCoherent && c>=threshold && h>=threshold &&
            rawTravel[0]>=threshold*.35f && rawTravel[1]>=threshold*.35f &&
            Length(Flat(Scale(Add(Sub(left,hands[0]),Sub(right,hands[1])),.5f)))>=std::max(threshold*.8f,h*.5f);
        if(translation) {translating=true;translated=now;}
        else if(now-translated>=.12) translating=false;
        // There is only one authority for physical facing: the planted-stance
        // classifier. Fine head/hand correlation cannot bypass its confidence.
        const bool rotation=freshHands && !quietHands && !walking && heading.sampleAccepted;
        if(rotation) rotated=now;
        // Position has one reference, independent of the long-lived facing
        // origin. Moving arms in/out must not be replayed as body translation
        // when angular tracking resumes after a pause.
        const Vec oldPair=Flat(Sub(placementHands[1],placementHands[0]));
        const Vec newPair=Flat(Sub(right,left));
        const bool pairUsable=Length(oldPair)>.05f && Length(newPair)>.05f;
        const float placementTurn=pairUsable ? BodyAngle(std::atan2(newPair.x,newPair.z)-std::atan2(oldPair.x,oldPair.z)) : 0.f;
        const Vec oldMid=Scale(Add(placementHands[0],placementHands[1]),.5f),newMid=Scale(Add(left,right),.5f);
        const Vec predictedEye=Add(newMid,YawVector(Sub(placementEye,oldMid),placementTurn));
        const float placementError=Length(Flat(Sub(predictedEye,eye)));
        const bool placementCoherent=placementError<.05f;
        const float placementTravel[2]={Length(Flat(Sub(left,placementHands[0]))),Length(Flat(Sub(right,placementHands[1])))};
        // During a confirmed curved step one hand can pass close to the turn
        // pivot. Require stronger rigid-pose agreement for that asymmetric
        // travel, rather than waiting for both hands to cover the same distance.
        const bool rigidTravel=rotation && placementError<.01f &&
            std::fabs(BodyAngle(headYaw-placementHeadYaw-placementTurn))<.04f &&
            std::min(placementTravel[0],placementTravel[1])>.003f;
        const bool freshPlacement=std::max(placementTravel[0],placementTravel[1])>=.025f &&
            (std::min(placementTravel[0],placementTravel[1])>=.025f || rigidTravel);
        bool placed=false;
        if((rotation && (currentTurn || (currentHeadTravel<1.e-6f && currentHeadTurn<1.e-6f) || (std::min(handMotion[0],handMotion[1])>.003f && currentHandTravel>.008f))) && !headOnlyMotion && pairUsable && (freshPlacement || translation) && placementCoherent) {
            // Fresh bilateral movement and the headset agree on this frame.
            // Carry the existing intentional lean, rather than integrating
            // small arm-pose fitting errors into a permanent body displacement.
            const float y=head.y;
            const float positionTurn=BodyAngle(std::atan2(newPair.x,newPair.z)-placementFacing);
            head=Add(walkEye,YawVector(Flat(Sub(placementAnchor,placementNeck)),positionTurn));head.y=y;
            consumedTravel=headDelta;placed=true;
        }
        if(translation && !walking && !placed) {
            // Track the already consumed displacement in the SAME neck-space
            // coordinates as the walking detector, never as a hand-fit offset.
            const Vec travel=Sub(headDelta,consumedTravel);
            head=Add(head,travel);
            placementAnchor=Add(placementAnchor,travel);placementNeck=Add(placementNeck,travel);
            // A step alone cannot consume the pending rotation. Keep its
            // positional frame and advance only the translation component.
            CapturePlacementEvidence(eye,left,right,headYaw);
        }
        if(placed || retirePlacement) CapturePlacement(eye,walkEye,left,right,headYaw);
        else if(freshPlacement && !placementCoherent) {
            // Learn changed arm geometry without discarding unconsumed body
            // rotation. The positional frame advances only when the body does.
            CapturePlacementEvidence(eye,left,right,headYaw);
        }
        if(walking || translation || rotation) {
            // Move the body by corroborated travel, never erase an existing
            // head-only lean merely because a hand-motion gate released.
            // Accepted placement above owns the position change. Neither
            // this bookkeeping nor facing can assign a second body position.
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
            if(!rotation && std::fabs(placementTurn)<.02f) CapturePlacement(eye,walkEye,left,right,headYaw);
        }
        if(!walking) offset=Flat(Sub(eye,head));
        else {
            // Explicit stick travel recentres smoothly; physical hand movement
            // must never release the entire accumulated head-only displacement.
            const float length=Length(offset);
            if(length>0) offset=Scale(offset,std::max(0.f,1-dt/length));
            head=Sub(eye,offset);height=eye.y;yaw=bodyYaw;
            CapturePlacement(eye,walkEye,left,right,headYaw);
        }
    }
};
}
