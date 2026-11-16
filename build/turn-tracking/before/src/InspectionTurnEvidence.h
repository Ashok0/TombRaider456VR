#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// A downward inspection is a persistent stance, not a permanent ban on turns.
// Release it only after a broad, one-direction turn of the tracked triangle
// around a compact body pivot. A single matching sample cannot release it.
struct InspectionTurnEvidence {
    struct Sample { Vec eye{},left{},right{},forward{};float yaw=0;double time=0; };
    Sample samples[100]{};int next=0,count=0;
    double permittedUntil=0;
    bool Update(Vec eye,Vec left,Vec right,Vec forward,float yaw,double now) {
        if(count && (now<samples[(next+99)%100].time || now-samples[(next+99)%100].time>.15)) *this={};
        if(count && now-samples[(next+99)%100].time<.025) return now<permittedUntil;
        const Sample current{eye,left,right,forward,yaw,now};
        const Vec pair=Flat(Sub(right,left)),mid=Scale(Add(left,right),.5f);
        const float facing=std::atan2(pair.x,pair.z);
        float freshTurn=0;
        for(int k=count-1;k>=0;--k) {
            const auto& recent=samples[(next-count+k+100)%100];
            if(now-recent.time<.10) continue;
            if(now-recent.time>.16) break;
            const Vec p=Flat(Sub(recent.right,recent.left));
            const float a=BodyAngle(facing-std::atan2(p.x,p.z));
            const float h=BodyAngle(yaw-recent.yaw);
            if(std::fabs(a)>.012f && std::fabs(h)>.008f && a*h>0) freshTurn=a;
            break;
        }
        bool accepted=false;
        for(int k=0;freshTurn!=0 && k<count && !accepted;++k) {
            const auto& start=samples[(next-count+k+100)%100];
            const double age=now-start.time;
            if(age<.35 || age>2.) continue;
            const Vec initialPair=Flat(Sub(start.right,start.left));
            if(Length(initialPair)<.12f || Length(pair)<.12f) continue;
            const float initialFacing=std::atan2(initialPair.x,initialPair.z);
            const float angle=BodyAngle(facing-initialFacing),head=BodyAngle(yaw-start.yaw);
            // A wider sweep tolerates foot-to-foot pivot changes. The shorter
            // escape requires a single stable orbit, keeping side inspections
            // that merely carry the hands with the head planted.
            const bool wide=std::fabs(angle)>=1.9198622f;
            const float positionError=wide ? .06f : .04f;
            if(std::fabs(angle)<1.3962634f || std::fabs(angle)>2.8f || angle*head<=0 ||
               angle*freshTurn<=0 || std::fabs(BodyAngle(angle-head))>(wide ? .25f : .15f)) continue;
            const Vec initialMid=Scale(Add(start.left,start.right),.5f);
            const Vec predictedEye=Add(mid,YawVector(Sub(start.eye,initialMid),angle));
            if(Length(Flat(Sub(predictedEye,eye)))>positionError) continue;
            const Vec translation=Sub(mid,YawVector(initialMid,angle));
            const float c=1-std::cos(angle),s=std::sin(angle),den=c*c+s*s;
            const Vec pivot{(c*translation.x+s*translation.z)/den,eye.y,
                            (-s*translation.x+c*translation.z)/den};
            if(Length(Flat(Sub(pivot,eye)))>.18f) continue;
            float low=eye.y,high=eye.y,lowPitch=forward.y,highPitch=forward.y;
            float last=0,reverse=0;bool coherent=true;
            for(int j=k;j<=count && coherent;++j) {
                const auto& sample=j==count ? current : samples[(next-count+j+100)%100];
                const Vec p=Flat(Sub(sample.right,sample.left));
                if(Length(p)<.12f) {coherent=false;break;}
                const float a=BodyAngle(std::atan2(p.x,p.z)-initialFacing);
                const float delta=BodyAngle(a-last);last=a;
                if(delta*angle<0) reverse+=std::fabs(delta);
                low=std::min(low,sample.eye.y);high=std::max(high,sample.eye.y);
                lowPitch=std::min(lowPitch,sample.forward.y);highPitch=std::max(highPitch,sample.forward.y);
                const Vec m=Scale(Add(sample.left,sample.right),.5f);
                const Vec predicted=Add(m,YawVector(Sub(start.eye,initialMid),a));
                const Vec orbit=Add(pivot,YawVector(Sub(initialMid,pivot),a));
                coherent=high-low<=(wide ? .06f : .04f) && highPitch-lowPitch<=(wide ? .16f : .12f) && reverse<=.10f &&
                    std::fabs(BodyAngle(sample.yaw-start.yaw-a))<=(wide ? .25f : .18f) &&
                    Length(Flat(Sub(predicted,sample.eye)))<=positionError && (wide || Length(Flat(Sub(orbit,m)))<=.035f);
            }
            accepted=coherent;
        }
        samples[next]=current;next=(next+1)%100;count=std::min(100,count+1);
        // Only fresh observed motion renews this bridge into the existing turn
        // classifier. Holding still cannot release inspection on a timer.
        if(accepted) permittedUntil=now+.35;
        return now<permittedUntil;
    }
};
}
