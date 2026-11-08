#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(int mode=0;mode<4;++mode)for(float reach:{-.22f,.30f}) {
 ControllerBodyAnchor a;BodyFollower body;float maxJump=0,maxError=0;Vec prevEye{},prevBody{};
 for(int i=0;i<600;++i){
 float angle=0,extension=0,walk=0;
 if(mode==0){extension=reach*std::clamp((i-90)/60.f,0.f,1.f);angle=std::max(0,i-150)*.008f;}
 if(mode==1){angle=std::min(std::max(0,i-40),130)*.008f+std::max(0,i-260)*.008f;extension=reach*std::clamp((i-180)/60.f,0.f,1.f);}
 if(mode==2){angle=std::max(0,i-40)*.008f;extension=reach*std::clamp((i-150)/80.f,0.f,1.f);walk=std::clamp((i-250)/100.f,0.f,1.f)*.35f;}
 if(mode==3){walk=std::clamp((i-40)/100.f,0.f,1.f)*.35f;extension=reach*std::clamp((i-150)/60.f,0.f,1.f);angle=std::max(0,i-220)*.008f;}
 Vec pivot{0,1.7f,-.12f},shift{0,0,walk};
 auto rotate=[&](Vec v){return Add(Add(pivot,YawVector(Sub(v,pivot),angle)),shift);};
 Vec eye=rotate({0,1.7f,0}),l=rotate({-.2f,1,.35f+extension}),r=rotate({.2f,1,.35f+extension});
 a.Update(i+1,i/90.,eye,l,r,0,body.yaw,true,false,angle);
 body.Update(i+1,i/90.,0,a.yaw,0,{},{},false,BodyFollowSettings(),{},false,true,0,a.held,true,a.heading.active?a.heading.target:NAN,a.rotating);
 if(i>0){maxJump=std::max(maxJump,Length(Flat(Sub(a.head,prevBody)))-Length(Flat(Sub(eye,prevEye))));maxError=std::max(maxError,Length(Flat(Sub(a.head,eye))));}
 prevEye=eye;prevBody=a.head;
 }
 printf("mode=%d reach=%.2fm excessJump=%.1fmm worstOffset=%.1fmm finalOffset=%.1fmm\n",mode,reach,maxJump*1000,maxError*1000,Length(Flat(Sub(a.head,prevEye)))*1000);
}}
