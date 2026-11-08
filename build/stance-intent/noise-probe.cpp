#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(float n:{.002f,.0065f,.012f})for(float h:{.008f,.025f,.05f}){float worst=0;int moves=0;
for(int pre=0;pre<3;++pre)for(int stop:{100,139,200,259}){
ControllerBodyAnchor a;BodyFollower b;Vec eye{},l{},r{};float angle=0;
auto tick=[&](int i,Vec e,Vec x,Vec y,float yaw){a.Update(i,i/90.,e,x,y,0,b.yaw,true,false,yaw);b.Update(i,i/90.,0,a.yaw,0,{},{},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);};
for(int i=1;i<=stop;++i){angle=pre==2?i*.008f:0;Vec neck{pre==1?i*.002f:0,1.7f,-.15f};eye=Add(neck,YawVector({0,0,.15f},angle));l=Add(neck,YawVector({-.2f,-.7f,.5f},angle));r=Add(neck,YawVector({.2f,-.7f,.5f},angle));tick(i,eye,l,r,angle);}
Vec initial=a.head;
for(int i=1;i<=900;++i){float v=std::sin(i*.04f);Vec jitter{n*v,0,n*std::sin(i*.03f)},g{h*v,0,h*std::sin(i*.03f)};tick(stop+i,Add(eye,g),Add(l,jitter),Add(r,jitter),angle+.5f*std::sin(i*.02f));if(i==30)initial=a.head;if(i>30){float drift=Length(Flat(Sub(a.head,initial)));worst=std::max(worst,drift);moves+=drift>.001f;}}
}printf("hand%.1fmm head%.1fmm drift%.1fmm frames%d\n",n*1000,h*1000,worst*1000,moves);
}}
