#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Releasing a planted inspection needs an actual shared displacement, not a
// single 6 mm coincidence while head and hands sweep past the same direction.
struct InspectionStepEvidence {
    struct Sample { Vec eye{},left{},right{};float yaw=0;double time=0; };
    Sample samples[64]{};int next=0,count=0;
    bool Update(Vec eye,Vec left,Vec right,float yaw,double now) {
        bool step=false;
        for(int i=0;i<count;++i) {
            const auto& s=samples[i];const double age=now-s.time;
            if(age<.2 || age>.6) continue;
            const Vec h=Flat(Sub(eye,s.eye));const float distance=Length(h);
            const float headTurn=std::fabs(BodyAngle(yaw-s.yaw));
            if(distance<.03f || headTurn>.10f) continue;
            const Vec a=Flat(Sub(s.right,s.left)),b=Flat(Sub(right,left));
            if(Length(a)<.05f || Length(b)<.05f) continue;
            const float turn=std::fabs(BodyAngle(std::atan2(b.x,b.z)-std::atan2(a.x,a.z)));
            if(turn>.08f) continue;
            const float error=std::max(Length(Sub(Flat(Sub(left,s.left)),h)),
                Length(Sub(Flat(Sub(right,s.right)),h)));
            // Very clean straight movement can start sooner. The longer
            // displacement is needed only when allowing natural arm sway.
            if((distance>=.06f && error<distance*.2f) ||
               (headTurn<.02f && turn<.02f && error<.001f)) step=true;
        }
        if(count==0 || now-samples[(next+63)%64].time>=.01) {
            samples[next]={eye,left,right,yaw,now};next=(next+1)%64;count=std::min(64,count+1);
        }
        return step;
    }
};
}
