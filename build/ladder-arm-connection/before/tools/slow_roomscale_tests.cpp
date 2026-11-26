#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* message) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main() {
 float worstTurn=0,worstStep=0;
 for(int hz:{60,90,120}) for(float pitch:{0.f,1.1f}) for(float reach:{.1f,.35f,.6f}) {
  for(float speed:{.08f,.16f,.35f}) for(float sign:{-1.f,1.f}) {
   ControllerBodyAnchor a;BodyFollower b;BodyFollowSettings settings;
   uint64_t frame=0;double now=1;float yaw=0;
   const Vec eye{0,1.65f,.12f},left{-.2f,1.2f,reach},right{.2f,1.2f,reach};
   auto tick=[&](float head,float hands) {
    a.Update(++frame,now,YawVector(eye,head),YawVector(left,hands),YawVector(right,hands),0,b.yaw,true,false,head,.15f,YawVector({0,-std::sin(pitch),std::cos(pitch)},head),pitch>.5f?.2f:0);
    const float before=b.yaw;
    b.Update(frame,now,0,head,0,{}, {},false,settings,a.head,false,true,0,true,true,a.heading.active?a.heading.target:NAN,a.rotating);
    Check(std::fabs(BodyAngle(b.yaw-before))<3.142f/hz+.001f,"slow-turn output cannot pop or exceed the normal yaw rate");now+=1./hz;
   };
   for(int i=0;i<hz;++i)tick(0,0);
   for(int i=0;i<hz*6;++i) {
    yaw=sign*speed*(i+1)/hz;tick(yaw,yaw);
    if(i>hz/2) {const float e=std::fabs(BodyAngle(b.yaw-yaw));worstTurn=std::max(worstTurn,e);
     if(e>.06f)std::printf("turn hz=%d pitch=%.1f reach=%.2f speed=%.2f t=%.2f error=%.1f\n",hz,pitch,reach,speed,i/float(hz),e*57.3f);
     Check(e<.06f,"slow physical turns follow within 3.5 degrees while still turning");}
    Check(Length(Flat(Sub(a.visualHead,YawVector(eye,yaw))))<.012f,"slow turn keeps the body centered around the physical pivot");
   }
   for(int i=0;i<hz;++i)tick(yaw,yaw);
   const auto held=a.visualHead;const float facing=b.yaw;
   for(int i=0;i<hz*2;++i){tick(yaw+.6f*std::sin(i*3.f/hz),yaw);
    Check(Length(Flat(Sub(a.visualHead,held)))<.002f && std::fabs(BodyAngle(b.yaw-facing))<.002f,"head-only glances after a slow turn leave the mesh planted");}
  }
  for(float speed:{.02f,.05f,.15f}) for(Vec direction:{Vec{-1,0,0},Vec{1,0,0},Vec{0,0,-1},Vec{0,0,1}}) {
   ControllerBodyAnchor a;const Vec eye{0,1.65f,.12f},left{-.2f,1.2f,reach},right{.2f,1.2f,reach};
   uint64_t frame=0;double now=1;
   auto tick=[&](Vec travel) {a.Update(++frame,now,Add(eye,travel),Add(left,travel),Add(right,travel),0,0,true,false,0,.15f,{0,-std::sin(pitch),std::cos(pitch)},pitch>.5f?.2f:0);now+=1./hz;};
   for(int i=0;i<hz;++i)tick({});
   Vec travel{};
   for(int i=0;i<hz*6;++i){travel=Scale(direction,speed*(i+1)/hz);tick(travel);
    const float error=Length(Flat(Sub(a.visualHead,Add(eye,travel))));worstStep=std::max(worstStep,error);
    if(error>.012f)std::printf("step hz=%d pitch=%.1f speed=%.2f t=%.2f error=%.1fmm\n",hz,pitch,speed,i/float(hz),error*1000);
    Check(error<.012f,"slow four-direction roomscale travel cannot leave the torso behind");}
   for(int i=0;i<hz;++i)tick(travel);
   Check(Length(Flat(Sub(a.visualHead,Add(eye,travel))))<.012f,"stopping a slow step retains its consumed displacement");
  }
 }
 std::printf("Worst slow-turn lag %.2f degrees; slow-step error %.2f mm\n",worstTurn*57.29578f,worstStep*1000);
 std::printf("PASS: %d slow roomscale checks\n",checks);
}
