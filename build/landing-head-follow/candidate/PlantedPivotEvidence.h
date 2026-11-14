#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// Evidence for a planted downward inspection, independent of walking/turn
// continuation. A fitted circle alone is insufficient: straight travel plus
// slight hand rotation also has a circle centre, usually beside the player.
struct PlantedPivotEvidence {
    struct Sample { Vec eye{},left{},right{};double time=0; };
    Sample samples[32]{};int next=0,count=0;
    double travelUntil=0; double until=0; bool dbgStraight=false,dbgClose=false;int dbgArc=0;
    // Separate steady shared travel from a changing aiming arc. Fit head and
    // hand midpoint independently to rotation about a fixed pivot plus velocity.
    bool SharedTravel(Vec eye,Vec left,Vec right,double now) const {
        if(count<12) return false;
        const auto& first=samples[count==32 ? next : 0];
        if(now-first.time<.32) return false;
        const Vec pair=Flat(Sub(first.right,first.left));
        if(Length(pair)<.12f) return false;
        const float angle=std::atan2(pair.x,pair.z);
        const Vec p[2]={first.eye,Scale(Add(first.left,first.right),.5f)};
        double m[4][8]{};int used=0;
        auto add=[&](const Sample& v) {
            const Vec q=Flat(Sub(v.right,v.left));if(Length(q)<.12f) return;
            const double turn=BodyAngle(std::atan2(q.x,q.z)-angle);
            const double c=std::cos(turn),sn=std::sin(turn),t=v.time-first.time;
            const double a[2][4]={{1-c,-sn,t,0},{sn,1-c,0,t}};
            const Vec point[2]={v.eye,Scale(Add(v.left,v.right),.5f)};
            for(int row=0;row<2;++row) for(int i=0;i<4;++i) {
                for(int j=0;j<4;++j) m[i][j]+=a[row][i]*a[row][j];
                for(int k=0;k<2;++k) {
                    const double b=row==0 ? point[k].x-c*p[k].x-sn*p[k].z : point[k].z+sn*p[k].x-c*p[k].z;
                    m[i][4+k]+=a[row][i]*b;
                }
            }
            ++used;
        };
        for(int i=0;i<count;++i) add(samples[i]);add({eye,left,right,now});
        if(used<12) return false;
        for(int k=0;k<4;++k) {
            int pivot=k;for(int i=k+1;i<4;++i) if(std::fabs(m[i][k])>std::fabs(m[pivot][k])) pivot=i;
            if(std::fabs(m[pivot][k])<1.e-7) return false;
            if(pivot!=k) for(int j=0;j<6;++j) std::swap(m[k][j],m[pivot][j]);
            const double divisor=m[k][k];for(int j=k;j<6;++j) m[k][j]/=divisor;
            for(int i=0;i<4;++i) if(i!=k) {const double f=m[i][k];for(int j=k;j<6;++j) m[i][j]-=f*m[k][j];}
        }
        const Vec h{float(m[2][4]),0,float(m[3][4])},v{float(m[2][5]),0,float(m[3][5])};
        return Length(h)>.12f && Length(v)>.12f && Length(Sub(h,v))<.08f && Dot(h,v)>.95f*Length(h)*Length(v);
    }
    bool Update(Vec eye,Vec left,Vec right,Vec forward,double now) {
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
        const bool closeInspection=forward.y<-.5f && left.y>eye.y-.12f && right.y>eye.y-.12f;
        if(closeInspection && SharedTravel(eye,left,right,now)) travelUntil=now+.35;
        if(now<travelUntil) straight=true;
        const bool inspecting=(forward.y<-.65f && left.y>eye.y-.5f && right.y>eye.y-.5f) || closeInspection;
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
            if(behind>(closeInspection ? .06f : .28f) && behind<.85f && std::fabs(side)<behind*.5f+(closeInspection ? .15f : 0.f)) {orbit=true;++arcTurns;}
            if(Length(toPivot)<.24f && std::fabs(turn)>.08f) ++nearbyTurns;
        }
        // Fresh evidence of a short neck/body orbit retires an older lean arc.
        if(!closeInspection && nearbyTurns>=3 && nearbyTurns>arcTurns) until=0;
        else if(orbit) until=now+(closeInspection ? .5 : .12);
        // Bound history at high headset refresh rates without frame-count timing.
        if(count==0 || now-samples[(next+31)%32].time>=.01) {
            samples[next]={eye,left,right,now};next=(next+1)%32;count=std::min(32,count+1);
        }
        dbgStraight=straight;dbgClose=closeInspection;dbgArc=arcTurns;return inspecting && now<until && !straight;
    }
};
}


