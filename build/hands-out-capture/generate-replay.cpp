#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){ControllerBodyAnchor a;
 for(int i=0;i<900;++i){
  if(i==450){a.Reset();std::puts("body-input-reset");}
  float t=i/90.f,yaw=.8f*std::sin(t*2),pitch=.7f+.3f*std::sin(t);
  Vec forward=YawVector({0,-std::sin(pitch),std::cos(pitch)},yaw),travel{.2f*std::max(0.f,t-5),0,0};
  Vec eye=Add(travel,Add({0,1.7f,0},Scale(forward,.15f))),left=Add(travel,{-.2f,1.3f,.5f}),right=Add(travel,{.2f,1.3f,.5f});
  double time=i/90.;a.Update(i+1,time,eye,left,right,0,0,true,false,yaw,.15f,forward);
  std::printf("body-input: %llu %.17g %.9g %.9g %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g -> %.9g %.9g %d %d %d\n",(unsigned long long)(i+1),time,0.,0.,1,0,yaw,.15f,
   eye.x,eye.y,eye.z,left.x,left.y,left.z,right.x,right.y,right.z,forward.x,forward.y,forward.z,a.head.x,a.head.z,int(a.held),int(a.rotating),int(a.translating));
 }
}
