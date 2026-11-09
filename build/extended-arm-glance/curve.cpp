#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(float speed:{0.f,.002f,.008f})for(float swing:{0.f,.01f,.025f})for(int direction=0;direction<4;++direction){
 ControllerBodyAnchor a;BodyFollower body;int permitted=0;float peak=0;
 for(int i=0;i<480;++i){float angle=i*speed;Vec shift=YawVector(Vec{0,0,i*.002f},direction*1.5707963f),neck{0,1.7f,-.15f};
 Vec eye=Add(Add(neck,YawVector(Vec{0,0,.15f},angle)),shift),l=Add(Add(neck,YawVector(Vec{-.2f,-.7f,.45f+swing*std::sin(i*.13f)},angle)),shift),r=Add(Add(neck,YawVector(Vec{.2f,-.7f,.45f-swing*std::sin(i*.13f)},angle)),shift);
 double now=(i+1)/90.;a.Update(i+1,now,eye,l,r,0,body.yaw,true,false,angle);body.Update(i+1,now,0,a.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,a.turnOutput,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
 if(i>120&&a.translating)++permitted;if(i>220)peak=std::max(peak,Length(Flat(Sub(a.head,eye))));
 }if(peak>.055f||permitted<=340)printf("speed %.3f swing %.3f dir %d peak %.1fmm permitted %d\n",speed,swing,direction,peak*1000,permitted);
}}
