#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
struct Rig {
    ControllerBodyAnchor a; BodyFollower f;
    void Step(int n,Vec eye,Vec l,Vec r,float yaw) {
        const double t=n/90.;
        a.Update(n,t,eye,l,r,0,f.yaw,true,false,yaw);
        f.Update(n,t,0,a.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,a.turnOutput,a.held,true,
            a.heading.active ? a.heading.target : NAN,a.rotating);
    }
};
int main() {
    const Vec eye{0,1.7f,0},l{-.2f,1,.3f},r{.2f,1,.3f},pivot{0,0,-.12f};
    auto rotate=[&](Vec v,float y) {return Add(pivot,YawVector(Sub(v,pivot),y));};
    Rig rig;int n=0;float centerError=0;
    for(int i=0;i<=720;++i) {
        const float yaw=i*6.28318530718f/720;
        const auto e=rotate(eye,yaw);
        rig.Step(++n,e,rotate(l,yaw),rotate(r,yaw),yaw);
        if(i>180) centerError=std::max(centerError,Length(Flat(Sub(rig.a.head,e))));
    }
    for(int i=0;i<90;++i) rig.Step(++n,eye,l,r,0);
    const float facing=rig.f.yaw;const Vec anchor=rig.a.head;
    float unwanted=0,slide=0;
    for(int i=1;i<=720;++i) {
        const float glance=.7f*std::sin(i*.025f),adjust=.12f*std::sin(i*.025f);
        rig.Step(++n,rotate(eye,glance),rotate(l,adjust),rotate(r,adjust),glance);
        unwanted=std::max(unwanted,std::fabs(BodyAngle(rig.f.yaw-facing)));
        slide=std::max(slide,Length(Flat(Sub(rig.a.head,anchor))));
    }
    std::printf("Head glance with small hand adjustments: %.3f deg rotation, %.3f cm slide; whole-turn centering error %.3f cm\n",
        unwanted*57.2957795f,slide*100,centerError*100);
    return unwanted>.001f || slide>.001f || centerError>.03f;
}
