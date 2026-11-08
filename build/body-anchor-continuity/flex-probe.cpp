#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(float radius:{0.f,.06f,.12f,.15f})for(float sign:{-1.f,1.f}){
ControllerBodyAnchor a;BodyFollower body;float worst=0;int steps=0;float oldPhase=0;
for(int i=0;i<=400;++i){
float angle=sign*std::min(i,360)*.01f,fade=i<=360?1.f:std::max(0.f,1-(i-360)/15.f);
Vec eye{radius*std::sin(angle),1.7f,radius*(std::cos(angle)-1)};
Vec l=Add(eye,YawVector({-.2f,-.3f,.35f+fade*.07f*std::sin(i*.31f)},angle));
Vec r=Add(eye,YawVector({ .2f,-.3f,.35f+fade*.07f*std::sin(i*.27f)},angle));
a.Update(i+1,i/90.,eye,l,r,0,body.yaw,true,false,angle);
body.Update(i+1,i/90.,0,a.yaw,0,{},{},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
Vec rel=YawVector(Sub(a.placementAnchor,a.placementNeck),-a.placementFacing);float phase=std::atan2(rel.x,rel.z);
if(false && radius==0 && std::fabs(BodyAngle(phase-oldPhase))>.005f)printf("PHASE %d delta%f reason%d rotation%d translation%d\n",i,BodyAngle(phase-oldPhase),a.reason,a.rotating,a.translating);oldPhase=phase;
float err=Length(Flat(Sub(a.head,eye)));worst=std::max(worst,err);steps+=a.translating;
if(false && radius==0 && i%20==0)printf("%d err%.1fmm pos(%f,%f) offNeck%f reason%d r%d t%d\n",i,err*1000,a.head.x,a.head.z,Length(Flat(Sub(a.placementAnchor,a.placementNeck))),a.reason,a.rotating,a.translating);
}
printf("sign%.1f radius%.2f worst%.1fmm final%.1fmm translations%d\n",sign,radius,worst*1000,Length(a.offset)*1000,steps);
}}
