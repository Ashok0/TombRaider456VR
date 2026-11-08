#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main() {
    const Vec head{0,1.7f,0},left{-.2f,1,.3f},right{.2f,1,.3f},pivot{0,0,-.12f},local{.035f,0,.14f};
    ControllerBodyAnchor a;BodyFollower f;float error=0;
    for(int i=0;i<=1080;++i) {
        const float yaw=i*6.2831853f/720;const double now=i/90.;
        auto rotate=[&](Vec v) {return Add(pivot,YawVector(Sub(v,pivot),yaw));};
        a.Update(i+1,now,rotate(head),rotate(left),rotate(right),0,f.yaw,true,false,yaw);
        const auto goal=Sub(a.head,YawVector(local,f.yaw));
        f.Update(i+1,now,0,a.yaw,0,goal,{},false,BodyFollowSettings(),a.head,false,true,0,a.held,true,
            a.heading.active ? a.heading.target : NAN,a.rotating);
        const auto expected=Sub(Flat(a.head),YawVector(local,f.yaw));
        if(a.held) error=std::max(error,Length(Sub(f.root,expected)));
    }
    float missed=0;
    for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor walk;walk.Update(1,0,head,left,right,0,0,true,false);
        walk.Update(2,.2,head,left,right,0,0,true,false);
        for(int i=1;i<=180;++i) {
            const float glance=.45f*std::sin(i*.04f);
            const auto step=YawVector({0,0,i*.002f},direction*1.5707963f);
            const auto eye=Add(Add(head,step),Sub(YawVector({0,0,.15f},glance),{0,0,.15f}));
            walk.Update(i+2,.2+i/90.,eye,Add(left,step),Add(right,step),0,0,true,false,glance);
        }
        missed=std::max(missed,Length(Sub(Flat(walk.head),YawVector({0,0,.36f},direction*1.5707963f))));
    }
    std::printf("Footprint drift during roomscale turn %.2f cm; lost directional travel while glancing %.2f cm\n",100*error,100*missed);
    return error>.001f || missed>.025f;
}
