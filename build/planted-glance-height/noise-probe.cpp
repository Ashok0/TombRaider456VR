#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){float worst=0;int failures=0;
for(float speed:{.002f,.008f,.02f})for(float step:{0.f,.002f})for(int stop=30;stop<500;stop+=7){
ControllerBodyAnchor a;BodyFollower b;Vec eye{},l{},r{};float angle=0;
auto tick=[&](int frame,Vec e,Vec left,Vec right,float yaw){a.Update(frame,frame/90.,e,left,right,0,b.yaw,true,false,yaw);b.Update(frame,frame/90.,0,a.yaw,0,{},{},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);};
for(int i=1;i<=stop;++i){angle=i*speed;Vec neck{step*i,1.7f,-.15f};eye=Add(neck,YawVector({0,0,.15f},angle));l=Add(neck,YawVector({-.2f,-.7f,.5f},angle));r=Add(neck,YawVector({.2f,-.7f,.5f},angle));tick(i,eye,l,r,angle);}
Vec held=a.head;
for(int i=1;i<=90;++i){Vec e=Add(eye,{.03f*std::sin(i*.12f),0,.03f*std::sin(i*.05f)});tick(stop+i,e,Add(l,{.002f*std::sin(i*.8f),0,.002f*std::cos(i*.7f)}),Add(r,{.002f*std::cos(i*.6f),0,.002f*std::sin(i*.9f)}),angle+.3f*std::sin(i*.1f));float drift=Length(Flat(Sub(a.head,held)));if(drift>worst){worst=drift;printf("worst %.2fmm speed%f walk%f stop%d glance%d reason%d held%d\n",drift*1000,speed,step,stop,i,a.reason,a.held);}if(drift>.0001f)++failures;}
}
printf("fixed hands: worst %.2fmm failures%d\n",worst*1000,failures);
}
