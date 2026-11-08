#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(float radius:{0.f,.06f,.12f,.15f})for(float sign:{-1.f,1.f}){
 ControllerBodyAnchor anchor;BodyFollower body;Vec eye{};float peak=0;
 for(int i=0;i<=400;++i){const float angle=sign*std::min(i,360)*.01f;const float fade=i<=360?1.f:std::max(0.f,1-(i-360)/15.f);
 eye={radius*std::sin(angle),1.7f,radius*(std::cos(angle)-1)};
 const Vec l=Add(eye,YawVector(Vec{-.2f,-.3f,.35f+fade*.07f*std::sin(i*.31f)},angle));
 const Vec r=Add(eye,YawVector(Vec{ .2f,-.3f,.35f+fade*.07f*std::sin(i*.27f)},angle));
 double now=(i+1)/90.;anchor.Update(i+1,now,eye,l,r,0,body.yaw,true,false,angle);
 body.Update(i+1,now,0,anchor.yaw,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,anchor.turnOutput,anchor.held,true,anchor.heading.active?anchor.heading.target:NAN,anchor.rotating);
 peak=std::max(peak,Length(Flat(Sub(anchor.head,eye))));
 }
 printf("r%.2f s%.0f final %.1fmm peak %.1fmm\n",radius,sign,Length(Flat(Sub(anchor.head,eye)))*1000,peak*1000);
}}