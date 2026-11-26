#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tr::firstperson;using namespace tr::motiongun;
struct Window {double begin,end;int poses=0,accepted=0;float sum=0,worst=0;};
int main(int argc,char**argv) {
 if(argc!=2)return 2;FILE* f=nullptr;fopen_s(&f,argv[1],"r");if(!f)return 2;
 Window windows[]={{18,28},{62,72},{116,122},{136,148}};
 ControllerBodyAnchor a;BodyFollower b;BodyFollowSettings settings;
 char line[4096];double start=-1;int count=0;Vec planted{};
 while(std::fgets(line,sizeof(line),f)) {
  if(!std::strncmp(line,"body-input-reset",16)){a.Reset();b.Reset();continue;}
  if(std::strncmp(line,"body-input: ",12))continue;
  unsigned long long serial;double now;float base,body,yaw,neck;int tracked,walking;Vec eye,left,right,forward;
  int n=std::sscanf(line+12,"%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",&serial,&now,&base,&body,&tracked,&walking,&yaw,&neck,&eye.x,&eye.y,&eye.z,&left.x,&left.y,&left.z,&right.x,&right.y,&right.z,&forward.x,&forward.y,&forward.z);
  if(n!=20)return 3;if(start<0)start=now;const double t=now-start;if(t>180)break;
  const char* d=std::strstr(line," drop=");float drop=d?float(std::atof(d+6)):0;
  if(b.valid)body=BodyAngle(b.yaw-b.base);
  a.Update(serial,now,eye,left,right,base,body,tracked,walking,yaw,neck,forward,drop);
  b.Update(serial,now,base,base+a.yaw,base+yaw,{}, {},walking,settings,a.head,false,true,0,a.held,true,a.heading.active?BodyAngle(base+a.heading.target):NAN,a.rotating);
#ifdef TRACE
  static double lastTrace=0;
  if(t>=18 && t<=28 && t-lastTrace>.2) {
   lastTrace=t;
   std::printf("t%.2f head%.1f body%.1f target%.1f pair%.1f origin%.1f/%.1f active%d turn%d accept%d hold%d/%d quiet%.2f cont%.2f slow%d\n",t,yaw*57.3,BodyAngle(b.yaw-base)*57.3,a.heading.target*57.3,std::atan2(right.x-left.x,right.z-left.z)*57.3,a.heading.facingOrigin*57.3,a.heading.bodyOrigin*57.3,a.heading.active,a.heading.turning,a.heading.sampleAccepted,a.inspectionHold,a.loweredInspectionHold,now-a.heading.quietSince,a.heading.continuousUntil-now,a.coherentMotion.turn);
  }
#endif
  if(!count)planted=a.visualHead;
  if(t<4.5 && (Length(Flat(Sub(a.visualHead,planted)))>.00001f || a.rotating)) {
   std::fprintf(stderr,"FAIL: recent initial planted glances changed the mesh\n");return 4;
  }
  ++count;
  for(auto& w:windows)if(t>=w.begin && t<w.end){
   float e=std::fabs(BodyAngle(b.yaw-base-yaw))*57.29578f;
   ++w.poses;if(a.rotating)++w.accepted;w.sum+=e;w.worst=std::max(w.worst,e);
  }
 }
 std::fclose(f);
 for(const auto&w:windows)std::printf("Turn %.0f..%.0fs: accepted %d/%d; mean head/body difference %.2f, max %.2f degrees\n",w.begin,w.end,w.accepted,w.poses,w.sum/std::max(1,w.poses),w.worst);
#ifdef VERIFY_IMPROVEMENT
 if(windows[0].sum/windows[0].poses>=20 || windows[3].sum/windows[3].poses>=20) {
  std::fprintf(stderr,"FAIL: recorded turn responsiveness regressed\n");return 5;
 }
 std::printf("PASS: recorded turn response and initial planted-glance checks\n");
#endif
 std::printf("Replayed %d recent poses (head/body difference is observational, not measured feet).\n",count);
}
