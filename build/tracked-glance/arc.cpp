#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
struct Rig {
ControllerBodyAnchor a;BodyFollower b;
void Step(int f,Vec e,Vec l,Vec r,float yaw){double now=f/90.;a.Update(f,now,e,l,r,0,b.yaw,true,false,yaw);b.Update(f,now,0,a.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);}
};
int main(){Rig rig;Vec eye{};for(int i=0;i<=400;i++){float radius=.06f,angle=std::min(i,360)*.01f,fade=i<=360?1.f:std::max(0.f,1-(i-360)/15.f);eye={radius*std::sin(angle),1.7f,radius*(std::cos(angle)-1)};Vec l=Add(eye,YawVector({-.2f,-.3f,.35f+fade*.07f*std::sin(i*.31f)},angle)),r=Add(eye,YawVector({.2f,-.3f,.35f+fade*.07f*std::sin(i*.27f)},angle));rig.Step(i+1,eye,l,r,angle);printf("%d %.6f %.6f %.6f %.6f %d %d %d %.6f %.6f\n",i,rig.a.head.x,rig.a.head.z,eye.x,eye.z,rig.a.held,rig.a.rotating,rig.a.translating,rig.a.placementNeck.x,rig.a.placementNeck.z);}}