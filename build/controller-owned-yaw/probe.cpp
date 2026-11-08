#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
struct Rig {
    ControllerBodyAnchor anchor; BodyFollower body; PhysicalBodyTurn fine;
    void Step(int frame,Vec eye,Vec l,Vec r,float yaw) {
        const double now=frame/90.;
        anchor.Update(frame,now,eye,l,r,0,body.yaw,true,false,yaw);
        const float turn=fine.Observe(frame,now,yaw,Sub(l,eye),Sub(r,eye),true);
        body.Update(frame,now,0,anchor.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,
            anchor.turnOutput,anchor.held,true,anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
    }
};
int main() {
    const Vec head{0,1.7f,0},left{-.2f,1,.3f},right{.2f,1,.3f};
    float worst=0;int bad=0;
    for(int stop=20;stop<=400;stop+=2) for(float noise:{0.f,.002f,.0065f}) {
        Rig a;
        Vec l=left,r=right;
        for(int frame=1;frame<=stop;++frame) {
            const float yaw=std::max(0,frame-20)*.01f;
            l=Add(head,YawVector(Add(Sub(left,head),{0,0,.07f*std::sin(frame*.31f)}),yaw));
            r=Add(head,YawVector(Add(Sub(right,head),{0,0,.07f*std::cos(frame*.27f)}),yaw));
            a.Step(frame,head,l,r,yaw);
        }
        Rig b=a;const float start=(stop-20)*.01f;
        for(int i=1;i<=360;++i) {
            const float glance=.2f*std::sin(i*.035f);
            const Vec eye=Add(head,{.03f*std::sin(i*.025f),0,.03f*std::sin(i*.019f)});
            const auto nl=Add(l,{noise*std::sin(i*1.7f),0,noise*std::cos(i*1.3f)});
            const auto nr=Add(r,{noise*std::cos(i*1.1f),0,noise*std::sin(i*1.9f)});
            a.Step(stop+i,eye,nl,nr,start+glance);
            b.Step(stop+i,head,nl,nr,start);
            const float drift=std::fabs(BodyAngle(a.body.yaw-b.body.yaw));
            if(drift>worst) { worst=drift;bad=stop; if(drift>.13f) std::printf("stop=%d noise=%f i=%d a=%f b=%f active=%d/%d rotating=%d/%d pending=%f/%f target=%f/%f\n",stop,noise,i,a.body.yaw,b.body.yaw,a.anchor.heading.active,b.anchor.heading.active,a.anchor.rotating,b.anchor.rotating,a.body.pendingPhysicalTurn,b.body.pendingPhysicalTurn,a.anchor.heading.target,b.anchor.heading.target); }
        }
    }
    std::printf("Head-only rotation difference: %.3f degrees, turn stop=%d\n",worst*57.2957795f,bad);
    return worst>.001f;
}



