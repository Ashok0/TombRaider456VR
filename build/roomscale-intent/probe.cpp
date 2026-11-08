#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(float speed:{0.f,.002f,.008f})for(float swing:{0.f,.01f,.025f})for(int dir=0;dir<4;++dir){
 ControllerBodyAnchor a;BodyFollower b;int travel=0;float maxError=0,prior=0,maxJerk=0;
 for(int i=0;i<480;++i){float angle=i*speed;Vec shift=YawVector({0,0,i*.002f},dir*1.5707963f),neck{0,1.7f,-.15f};
 Vec eye=Add(Add(neck,YawVector({0,0,.15f},angle)),shift);
 Vec l=Add(Add(neck,YawVector({-.2f,-.7f,.45f+swing*std::sin(i*.13f)},angle)),shift);
 Vec r=Add(Add(neck,YawVector({.2f,-.7f,.45f-swing*std::sin(i*.13f)},angle)),shift);
 a.Update(i+1,i/90.,eye,l,r,0,b.yaw,true,false,angle);
 float before=b.yaw;
 b.Update(i+1,i/90.,0,a.yaw,0,{},{},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
 if(i>120){if(a.translating)++travel;maxError=std::max(maxError,Length(Flat(Sub(a.head,eye))));float step=BodyAngle(b.yaw-before);maxJerk=std::max(maxJerk,std::fabs(step-prior));prior=step;}
 }
 printf("speed %.3f swing %.3f dir %d permitted %d/359 error %.1fmm jerk %.3fdeg\n",speed,swing,dir,travel,maxError*1000,maxJerk*57.2958f);
}}
