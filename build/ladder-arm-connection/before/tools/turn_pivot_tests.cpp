#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* message) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main() {
 for(int hz:{60,90,120}) for(float direction:{-1.f,1.f}) for(float lean:{-.14f,0.f,.14f}) {
  ControllerBodyAnchor a;BodyFollower follower;BodyFollowSettings settings;
  Vec eye{.28f,1.7f,.18f},left{.08f,1.1f,.5f},right{.48f,1.1f,.5f};
  uint64_t serial=1;double now=1;
  a.Update(serial,now,eye,left,right,0,0,true,false,0,.15f,{0,0,1});
  // First walk with all devices, then look sideways over planted feet.
  for(int i=1;i<=hz*2;++i) {
   const Vec d{.08f/hz,0,0};eye=Add(eye,d);left=Add(left,d);right=Add(right,d);
   a.Update(++serial,now+=1./hz,eye,left,right,0,0,true,false,0,.15f,{0,0,1});
  }
  for(int i=1;i<=hz;++i) {
   eye.x+=lean/hz;
   a.Update(++serial,now+=1./hz,eye,left,right,0,0,true,false,.7f*i/hz,.15f,YawVector({0,0,1},.7f*i/hz));
  }
  Vec view{.31f,0,-.24f}; // Nonzero unconsumed tracking, as after physical travel.
  const Vec pivot=Sub(view,a.visualOffset),foot=Add(pivot,{.035f,0,-.12f});
  follower.Update(serial,now,0,0,0,foot,{},false,settings,pivot,false,true,0,true,true);
  float base=0;
  for(int i=1;i<=hz*6;++i) {
   const float delta=direction*6.28318530718f/(hz*6),next=BodyAngle(base+delta);
   view=YawVector(view,-delta); // Production PivotHeadFloorOffset behavior.
   a.Update(++serial,now+=1./hz,eye,left,right,next,0,true,false,.7f,.15f,YawVector({0,0,1},.7f));
   const Vec worldPivot=YawVector(Sub(view,a.visualOffset),next);
   Check(Length(Flat(Sub(worldPivot,pivot)))<.0001f,"stick yaw preserves the established world-space body pivot after walking/glancing");
   follower.Update(serial,now,next,next,0,Add(pivot,YawVector(Sub(foot,pivot),base)),{},false,settings,worldPivot,false,true,0,true,true);
   const Vec expected=Add(pivot,YawVector(Sub(foot,pivot),next));
   Check(Length(Flat(Sub(follower.root,expected)))<.0002f,"analog turn does not accumulate an off-center footprint orbit");
   const auto held=a.visualHead;
   a.Update(serial,now,eye,left,right,next,0,true,false,.7f,.15f,YawVector({0,0,1},.7f));
   Check(Length(Sub(held,a.visualHead))==0,"duplicate stereo/accessory pass cannot apply the turn twice");
   base=next;
  }
  const auto stop=a.visualHead;
  for(int i=0;i<hz;++i) {
   a.Update(++serial,now+=1./hz,eye,left,right,base,0,true,false,.7f,.15f,YawVector({0,0,1},.7f));
   Check(Length(Flat(Sub(stop,a.visualHead)))<.0001f,"releasing the stick cannot erase the held visual pivot");
  }
 }
 // An admitted turn remains a turn while the player lowers their gaze.
 for(int hz:{60,90,120}) {
  ControllerBodyAnchor a;BodyFollower b;BodyFollowSettings settings;
  Vec eye{0,1.6f,.12f},left{-.18f,1.2f,.3f},right{.18f,1.2f,.3f};
  for(int i=0;i<hz*9;++i) {
   const double now=1.+double(i)/hz;const float angle=.35f*std::max(0.f,float(i)/hz-1);
   const float down=i>hz*4 ? -.88f : -.6f;
   // Modest constant head/torso difference is an intentional gaze offset.
   const float gaze=angle+.2f;
   a.Update(i+1,now,YawVector(eye,angle),YawVector(left,angle),YawVector(right,angle),0,b.yaw,true,false,gaze,.15f,YawVector({0,down,std::sqrt(1-down*down)},gaze),.25f);
   b.Update(i+1,now,0,0,0,{}, {},false,settings,a.head,false,true,0,true,true,a.heading.active?a.heading.target:NAN,a.rotating);
   if(i>hz*3) {
    Check(a.rotating && !a.loweredInspectionHold,"looking down cannot interrupt fresh accepted whole-body rotation");
    Check(std::fabs(BodyAngle(b.yaw-angle))<.12f,"accepted physical turn retains progress through a downward gaze transition");
   }
  }
 }
 std::printf("PASS: %d analog pivot continuity checks\n",checks);
}
