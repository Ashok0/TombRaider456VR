#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// A short rigid-motion fit for slow movement. Admission depends on agreement
// among three tracked positions and head yaw, not on a minimum walking speed.
// This supplies intent only. The existing anchor remains the single owner of
// displacement, so overlapping observation windows cannot integrate it twice.
struct CoherentBodyMotion {
    struct Sample { Vec eye{},left{},right{};float head=0;double time=0; };
    Sample samples[96]{};int next=0,count=0;
    bool turn=false,step=false;double headLeadUntil=0,turnEligibleSince=0;
    void Reset() {*this={};}
    bool Update(Vec eye,Vec left,Vec right,float head,double now,bool allowTurn,bool aligned=true) {
        turn=step=false;
        if(count && (now<samples[(next+95)%96].time || now-samples[(next+95)%96].time>.15)) Reset();
        if(!allowTurn) turnEligibleSince=now;
        const Sample current{eye,left,right,head,now};
        const auto pair=Flat(Sub(right,left));const float span=Length(pair);
        const auto mid=Scale(Add(left,right),.5f);
        // A matching tail of a head-led glance is not a new physical turn.
        // Retain that onset evidence across overlapping short windows.
        for(int k=count-1;k>=0;--k) {
            const auto& s=samples[(next-count+k+96)%96];const double age=now-s.time;
            if(age<.15) continue;if(age>.35) break;
            const Vec p=Flat(Sub(s.right,s.left));if(Length(p)<.12f || span<.12f) continue;
            const float hand=BodyAngle(std::atan2(pair.x,pair.z)-std::atan2(p.x,p.z));
            const float gaze=BodyAngle(head-s.head);
            if(std::fabs(gaze)>.06f && (hand*gaze<0 || std::fabs(hand)<std::fabs(gaze)*.5f))
                headLeadUntil=now+.6;
        }
        allowTurn=allowTurn && now>=headLeadUntil;
        bool accepted=false;
        for(int k=count-1;k>=0 && !accepted;--k) {
            const auto& s=samples[(next-count+k+96)%96];const double age=now-s.time;
            if(age<.30 || age>.75 || span<.12f) continue;
            const Vec p=Flat(Sub(s.right,s.left));const float oldSpan=Length(p);
            if(oldSpan<.12f) continue;
            const float angle=BodyAngle(std::atan2(pair.x,pair.z)-std::atan2(p.x,p.z));
            const float gaze=BodyAngle(head-s.head);
            const Vec oldMid=Scale(Add(s.left,s.right),.5f);
            const Vec h=Flat(Sub(eye,s.eye)),l=Flat(Sub(left,s.left)),r=Flat(Sub(right,s.right));
            const float distance=Length(h),motion=std::min(Length(l),Length(r));
            const bool broad=!aligned && std::fabs(angle)>=.35f;
            const bool straight=distance>=.004f && motion>=.003f && std::fabs(gaze)<.018f && std::fabs(angle)<.018f &&
                Length(Sub(h,l))<.0005f+distance*.06f && Length(Sub(h,r))<.0005f+distance*.06f;
            bool orbit=false;
            if(allowTurn && s.time>=turnEligibleSince && (aligned || broad) && motion>=.003f && std::fabs(angle)>=.018f && std::fabs(angle)<.65f && gaze*angle>0 &&
               std::fabs(BodyAngle(gaze-angle))<.002f+std::fabs(angle)*.10f &&
               std::fabs(span-oldSpan)<.001f+motion*.12f) {
                const Vec predicted=Add(mid,YawVector(Sub(s.eye,oldMid),angle));
                const Vec translation=Sub(mid,YawVector(oldMid,angle));
                const float c=1-std::cos(angle),sn=std::sin(angle),den=c*c+sn*sn;
                const Vec pivot{(c*translation.x+sn*translation.z)/den,eye.y,(-sn*translation.x+c*translation.z)/den};
                // A torso swivelling over planted feet describes a larger
                // lean arc. Only a compact near-head pivot admits this turn.
                orbit=Length(Flat(Sub(predicted,eye)))<(broad ? .003f : .0005f+motion*.04f) && Length(Flat(Sub(pivot,eye)))<(broad ? .18f : .24f);
            }
            if(!straight && !orbit) continue;
            // Check the intervening trajectory too; two coincidental endpoints
            // cannot promote a glance or an arm gesture to whole-body motion.
            bool coherent=true;int moving=0;float progress=0;
            for(int j=k+1;j<=count && coherent;++j) {
                const auto& q=j==count ? current : samples[(next-count+j+96)%96];
                const Vec qp=Flat(Sub(q.right,q.left)),qm=Scale(Add(q.left,q.right),.5f);
                const float qa=BodyAngle(std::atan2(qp.x,qp.z)-std::atan2(p.x,p.z));
                const float qh=BodyAngle(q.head-s.head);
                if(straight) {
                    const Vec travel=Flat(Sub(q.eye,s.eye));const float d=Length(travel);
                    coherent=std::fabs(qh)<.022f && std::fabs(qa)<.022f &&
                        Length(Sub(Flat(Sub(q.left,s.left)),travel))<.0008f+distance*.06f &&
                        Length(Sub(Flat(Sub(q.right,s.right)),travel))<.0008f+distance*.06f &&
                        (d<.001f || Dot(travel,h)>-.00001f);
                    if(d>.001f) ++moving;
                } else {
                    const Vec predicted=Add(qm,YawVector(Sub(s.eye,oldMid),qa));
                    coherent=Length(qp)>.12f && std::fabs(BodyAngle(qh-qa))<.003f+std::fabs(angle)*.10f &&
                        Length(Flat(Sub(predicted,q.eye)))<(broad ? .004f : .0008f+motion*.04f) && qa*angle>=-.00005f;
                    if(qa*angle>progress*angle+.00001f) ++moving;
                    progress=qa;
                }
            }
            if(!coherent || moving<3) continue;
            turn=orbit && !straight;step=straight;
            accepted=true;
        }
        if(!count || now-samples[(next+95)%96].time>=.008) {
            samples[next]=current;
            next=(next+1)%96;count=std::min(96,count+1);
        }
        return accepted;
    }
};
}
