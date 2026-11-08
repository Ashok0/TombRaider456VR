#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <algorithm>
#include <vector>
using namespace tr::firstperson;
struct Sample {int frame;float yaw,body;Vec eye,l,r;};
int main(int argc,char**argv){
 FILE* f=nullptr;fopen_s(&f,argv[1],"r");if(!f)return 1;
 std::vector<Sample> samples;Sample s{};
 while(fscanf_s(f,"%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",&s.frame,&s.yaw,&s.body,&s.eye.x,&s.eye.y,&s.eye.z,&s.l.x,&s.l.y,&s.l.z,&s.r.x,&s.r.y,&s.r.z)==12)samples.push_back(s);
 ControllerBodyAnchor a;Vec previous{};float max=0;int frameMax=0;
 for(size_t i=0;i+1<samples.size();++i) {
  const auto& p=samples[i];const auto& q=samples[i+1];
  for(int frame=p.frame;frame<q.frame;++frame){
   const float t=float(frame-p.frame)/(q.frame-p.frame);
   auto mix=[&](Vec x,Vec y){return Add(x,Scale(Sub(y,x),t));};
   const Vec eye=mix(p.eye,q.eye);const float yaw=p.yaw+BodyAngle(q.yaw-p.yaw)*t;
   a.Update(frame,frame/60.,eye,mix(p.l,q.l),mix(p.r,q.r),0,p.body+BodyAngle(q.body-p.body)*t,true,false,yaw);
   const float distance=Length(Flat(Sub(a.head,previous)));if(i && distance>max){max=distance;frameMax=frame;}
   if(i && distance>.04f)printf("jump frame=%d mm=%.1f move=%d turn=%d held=%d\n",frame,distance*1000,a.translating,a.rotating,a.held);
   previous=a.head;
  }
 }
 printf("samples=%zu max=%.1fmm frame=%d\n",samples.size(),max*1000,frameMax);
}