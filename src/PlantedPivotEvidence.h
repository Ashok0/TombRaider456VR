#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Evidence for a planted downward inspection, independent of walking/turn
// continuation. A fitted circle alone is insufficient: straight travel plus
// slight hand rotation also has a circle centre, usually beside the player.
struct PlantedPivotEvidence {
    struct Sample { Vec eye{},left{},right{};double time=0; };
    Sample samples[32]{};int next=0,count=0;
    double until=0;
    bool straightTravel=false;
    bool Update(Vec eye,Vec left,Vec right,Vec forward,double now,float inspectionPitch=-.65f) {
        bool orbit=false,straight=false;
        int nearbyTurns=0,arcTurns=0;
        // Current shared translation releases an old angular observation.
        // This fast path is independent of gaze and the previous hold timer.
        for(int i=0;i<count;++i) {
            const auto& s=samples[i];const double age=now-s.time;
            if(age<.08 || age>.12) continue;
            const Vec h=Flat(Sub(eye,s.eye)),l=Flat(Sub(left,s.left)),r=Flat(Sub(right,s.right));
            const float distance=Length(h);
            if(distance>.006f && Length(Sub(h,l))<distance*.12f && Length(Sub(h,r))<distance*.12f) straight=true;
        }
        straightTravel=straight;
        const bool inspecting=forward.y<inspectionPitch && left.y>eye.y-.5f && right.y>eye.y-.5f;
        if(inspecting) for(int i=0;i<count;++i) {
            const auto& s=samples[i];const double age=now-s.time;
            if(age<.10 || age>.24) continue;
            const Vec a=Flat(Sub(s.right,s.left)),b=Flat(Sub(right,left));
            const float span=Length(b);
            if(Length(a)<.12f || span<.12f) continue;
            const float turn=BodyAngle(std::atan2(b.x,b.z)-std::atan2(a.x,a.z));
            if(std::fabs(turn)<.04f || std::fabs(turn)>.7f) continue;
            const Vec oldMid=Scale(Add(s.left,s.right),.5f),mid=Scale(Add(left,right),.5f);
            const Vec predicted=Add(mid,YawVector(Sub(s.eye,oldMid),turn));
            if(Length(Flat(Sub(predicted,eye)))>.06f) continue;
            const Vec t=Sub(mid,YawVector(oldMid,turn));
            const float c=1-std::cos(turn),sn=std::sin(turn),den=c*c+sn*sn;
            const Vec pivot{(c*t.x+sn*t.z)/den,eye.y,(-sn*t.x+c*t.z)/den};
            const Vec direction{-b.z/span,0,b.x/span},toPivot=Flat(Sub(pivot,eye));
            const float behind=-Dot(toPivot,direction),side=Dot(toPivot,{direction.z,0,-direction.x});
            // A leaned torso swivels about a point behind the neck. Require
            // that anatomical direction as well as distance and rigid-pose
            // agreement; a point beside/in front of the eye cannot block a step.
            if(behind>.28f && behind<.85f && std::fabs(side)<behind*.5f) {orbit=true;++arcTurns;}
            if(Length(toPivot)<.24f && std::fabs(turn)>.08f) ++nearbyTurns;
        }
        // Fresh evidence of a short neck/body orbit retires an older lean arc.
        if(nearbyTurns>=3 && nearbyTurns>arcTurns) until=0;
        else if(orbit) until=now+.12;
        // Bound history at high headset refresh rates without frame-count timing.
        if(count==0 || now-samples[(next+31)%32].time>=.01) {
            samples[next]={eye,left,right,now};next=(next+1)%32;count=std::min(32,count+1);
        }
        return inspecting && now<until && !straight;
    }
};
}
