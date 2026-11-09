#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// A downward inspection can swivel the leaned upper torso about the hips.
// Its long head orbit is not the short eye/neck orbit of an upright body turn.
// Use this only as evidence for retaining an existing stance. It never changes
// the tracked view, body position, or the displacement consumed by locomotion.
struct PlantedSwivel {
    struct Sample { Vec eye{},left{},right{};double time=0; };
    Sample samples[96]{};int next=0,count=0;
    double until=0;
    double velocitySince=-1,travelUntil=0,lastFit=0;Vec velocityDirection{};
    bool Update(Vec eye,Vec left,Vec right,Vec forward,double now) {
        bool orbit=false,straight=false;
        if(forward.y<-.65f) {
            for(int i=0;i<count;++i) {
                const auto& s=samples[i];const double age=now-s.time;
                if(age<.10 || age>.24) continue;
                const Vec a=Flat(Sub(s.right,s.left)),b=Flat(Sub(right,left));
                if(Length(a)<.12f || Length(b)<.12f) continue;
                const float turn=BodyAngle(std::atan2(b.x,b.z)-std::atan2(a.x,a.z));
                if(std::fabs(turn)<.04f || std::fabs(turn)>.7f) continue;
                const Vec oldMid=Scale(Add(s.left,s.right),.5f),mid=Scale(Add(left,right),.5f);
                const Vec predicted=Add(mid,YawVector(Sub(s.eye,oldMid),turn));
                if(Length(Flat(Sub(predicted,eye)))>.06f) continue;
                const Vec t=Sub(mid,YawVector(oldMid,turn));
                const float c=1-std::cos(turn),sn=std::sin(turn),den=c*c+sn*sn;
                const Vec pivot{(c*t.x+sn*t.z)/den,eye.y,(-sn*t.x+c*t.z)/den};
                const float radius=Length(Flat(Sub(eye,pivot)));
                if(radius>.24f && radius<.85f) {orbit=true;break;}
            }
        }
        if(orbit) until=now+.12;
        // Bound both storage and work independently of headset refresh rate.
        if(count==0 || now-samples[(next+95)%96].time>=.01) {
            samples[next]={eye,left,right,now};next=(next+1)%96;count=std::min(96,count+1);
        }
        return forward.y<-.65f && now<until;
    }
};
}
