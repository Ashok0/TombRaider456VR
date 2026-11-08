#include "BodyFollower.h"
#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){
 for(int scenario=0;scenario<4;++scenario){
 ControllerBodyAnchor a;BodyFollower b;float worst=0, minStep=10,maxStep=0;int paused=0;
 for(int i=0;i<=720;++i){
 float angle=i*.008f;float span=scenario==1?.075f: scenario==2 && i>100?.075f:.4f;
 float reach=.3f+(scenario==3 && i>100?.16f:0);
 Vec h{0,1.7f,0},l=YawVector({-span*.5f,1,reach},angle),r=YawVector({span*.5f,1,reach},angle);
 a.Update(i+1,i/90.,h,l,r,0,b.yaw,true,false,angle);
 float old=b.yaw;
 b.Update(i+1,i/90.,0,a.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
 if(i>200){float e=std::fabs(BodyAngle(b.yaw-angle));worst=std::max(worst,e);float step=std::fabs(BodyAngle(b.yaw-old));minStep=std::min(minStep,step);maxStep=std::max(maxStep,step);if(step<.0001f)++paused;}
 }
 printf("scenario=%d worstError=%.2fdeg minStep=%.3fdeg maxStep=%.3fdeg paused=%d/520\n",scenario,worst*57.2958f,minStep*57.2958f,maxStep*57.2958f,paused);
 }
for(float sign:{-1.f,1.f}) {
ControllerBodyAnchor a;BodyFollower b;Vec head{0,1.7f,0},left{-.2f,1,.3f},right{.2f,1,.3f};
a.Update(1,0,head,left,right,0,0,true,false);a.Update(2,.2,head,left,right,0,0,true,false);b.Update(2,.2,0,0,0,{},{},false,BodyFollowSettings());
for(int i=1;i<=400;++i){float angle=sign*std::min(i,360)*.01f,flex=i<=360?1.f:std::max(0.f,1-(i-360)/15.f);
auto l=Add(head,YawVector(Add(Sub(left,head),{0,0,flex*.07f*std::sin(i*.31f)}),angle));
auto r=Add(head,YawVector(Add(Sub(right,head),{0,0,flex*.07f*std::cos(i*.27f)}),angle));
a.Update(i+2,.2+i/90.,head,l,r,0,b.yaw,true,false,angle);
b.Update(i+2,.2+i/90.,0,angle,0,{},{},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
if(i%20==0)printf("flex sign %.0f frame %d yaw %.2f target %.2f ref %.2f turning %d\n",sign,i,b.yaw,a.heading.target,a.heading.referenceTime,a.heading.turning);
}
}
}
