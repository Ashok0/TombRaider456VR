#include "ControllerBodyAnchor.h"
#include <cstdio>
void Check(bool,const char*) {}
#include "../../tools/close_hands_motion_regression.inl"
int main(){using namespace tr::firstperson;ControllerBodyAnchor a;BodyFollower b;bool hold=false;int turns=0;
for(const auto& s:CloseHandsCapture()){float body=b.valid?BodyAngle(b.yaw-s.base):s.body;
const auto pair=Sub(s.right,s.left);float facing=a.heading.valid?BodyAngle(a.heading.bodyOrigin+std::atan2(pair.x,pair.z)-a.heading.facingOrigin):body;
a.Update(s.frame,s.time,s.eye,s.left,s.right,s.base,body,true,false,s.yaw,.15f,s.forward);
if(hold!=a.inspectionHold)printf("hold=%d t=%.3f gazeY=%.3f head=%.1f body=%.1f translating=%d established=%d controller=%.1f\n",a.inspectionHold,s.time-CloseHandsCapture()[0].time,s.forward.y,s.yaw*57.3,body*57.3,a.translating,a.heading.established,facing*57.3);
hold=a.inspectionHold;b.Update(s.frame,s.time,s.base,s.base+a.yaw,s.base+s.yaw,{},{},false,BodyFollowSettings{}, {},false,true,0,a.held,true,a.heading.active?s.base+a.heading.target:NAN,a.rotating);
if(s.time-CloseHandsCapture()[0].time>=20.5 &&s.time-CloseHandsCapture()[0].time<29.5&&a.rotating)++turns;}
printf("turns=%d\n",turns);}
