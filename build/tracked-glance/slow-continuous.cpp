#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
struct Rig {
ControllerBodyAnchor a;BodyFollower b;
void Step(int f,Vec e,Vec l,Vec r,float yaw){double now=f/90.;a.Update(f,now,e,l,r,0,b.yaw,true,false,yaw);b.Update(f,now,0,a.yaw,0,{}, {},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);}
};
int main(){for(float noise:{.002f,.0065f})for(float glance:{.03f,.15f,.3f}){
float worst=0;int worstStop=0;float worstSpeed=0;float worstYaw=0;
for(float speed:{.002f,.008f,.02f})for(int stop=30;stop<500;stop+=7){Rig rig;Vec eye{},l{},r{};float angle=0;
for(int i=1;i<=stop;i++){angle=i*speed;Vec neck{0,1.7f,-.15f};eye=Add(neck,YawVector({0,0,.15f},angle));l=Add(neck,YawVector({-.2f,-.7f,.5f},angle));r=Add(neck,YawVector({.2f,-.7f,.5f},angle));rig.Step(i,eye,l,r,angle);}
auto fixed=rig.a.head;float fixedYaw=rig.b.yaw;
for(int i=1;i<=180;i++){rig.Step(stop+i,Add(eye,{glance*std::sin(i*.008f),0,glance*.3f*std::sin(i*.004f)}),Add(l,{noise*std::sin(i*.1f),0,noise*std::sin(i*.07f)}),Add(r,{noise*std::sin(i*.06f),0,noise*std::sin(i*.09f)}),angle+.7f*std::sin(i*.01f));float d=Length(Flat(Sub(rig.a.head,fixed)));if(d>worst){worst=d;worstStop=stop;worstSpeed=speed;worstYaw=BodyAngle(rig.b.yaw-fixedYaw);}}
}printf("noise %.1fmm glance %.0fmm worst %.2fmm stop %d speed %.3f yaw %.2fdeg\n",noise*1000,glance*1000,worst*1000,worstStop,worstSpeed,worstYaw*57.3f);
}}
