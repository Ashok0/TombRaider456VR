#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){int errors=0;for(float pitch:{0.f,.7f,1.4f})for(float sign:{-1.f,1.f}){
 ControllerBodyAnchor a;BodyFollower b;Vec eye{};
 for(int i=0;i<800;++i){const float yaw=sign*std::min(i,720)*6.28318530718f/720;
  const Vec forward=YawVector({0,-std::sin(pitch),std::cos(pitch)},yaw),pivot{0,1.7f,0};
  eye=Add(pivot,Scale(forward,.15f));Vec l=Add(pivot,YawVector({-.2f,-.4f,.5f},yaw)),r=Add(pivot,YawVector({.2f,-.4f,.5f},yaw));
  double now=i/90.;a.Update(i+1,now,eye,l,r,0,b.yaw,true,false,yaw,.15f,forward);
  b.Update(i+1,now,0,a.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,a.turnOutput,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
 }
 const float distance=Length(Flat(Sub(a.head,eye)));const float angle=std::fabs(BodyAngle(b.yaw));
 printf("pitch %.2f direction %.0f final position %.2fmm final yaw error %.2fdeg\n",pitch,sign,distance*1000,angle*57.29578f);
 if(distance>.035f || angle>.15f)++errors;
}return errors?1:0;}
