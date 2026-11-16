#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* msg) {++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
int main() {
 for(int hz:{60,90,120}) {
  RoomscaleHeadingRecovery h;float body=0;Vec eye{0,1.7f,.12f},left{-.18f,1.4f,.4f},right{.18f,1.4f,.4f};
  for(int i=0;i<hz*2;++i) {
   const float yaw=.5f*i/hz;
   h.Update(YawVector(eye,yaw),YawVector(left,yaw),YawVector(right,yaw),body,yaw,1.+double(i)/hz);
   if(h.active)body=h.target;
  }
  Check(h.turning,"true roomscale turn starts");
  const float stopped=body;
  // Head stops while BOTH arms keep sweeping: old support never rechecks head.
  for(int i=0;i<hz*3;++i) {
   const float hands=1.f+.4f*std::sin(i*2.f/hz);
   h.Update(YawVector(eye,1.f),YawVector(left,hands),YawVector(right,hands),body,1.f,3.+double(i)/hz);
   if(h.active)body=h.target;
   if(i>hz/2)Check(!h.turning && !h.sampleAccepted,"hands alone cannot indefinitely renew turn intent");
  }
  Check(std::fabs(BodyAngle(body-stopped))<.15f,"stopped head cannot be dragged around by later arm motion");
 }
 std::printf("PASS: %d turn-continuation checks\n",checks);
}
