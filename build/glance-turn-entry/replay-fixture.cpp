#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* what) {
    ++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}
}
int main(int argc,char** argv) {
    Check(argc==2,"stationary glance capture supplied");
    FILE* f=nullptr;fopen_s(&f,argv[1],"r");Check(f!=nullptr,"capture opens");
    char line[4096]{};ControllerBodyAnchor a;float facing=0;int samples=0;
    while(std::fgets(line,sizeof(line),f)) {
        unsigned long long frame;double time;float base,body,yaw,neck,x,z,neutral;
        int tracked,walking,held,turn,move;Vec eye{},left{},right{},forward{};
        const int n=std::sscanf(line+12,
            "%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f -> %f %f %d %d %d %f",
            &frame,&time,&base,&body,&tracked,&walking,&yaw,&neck,
            &eye.x,&eye.y,&eye.z,&left.x,&left.y,&left.z,&right.x,&right.y,&right.z,
            &forward.x,&forward.y,&forward.z,&x,&z,&held,&turn,&move,&neutral);
        Check(n==26 && !walking,"capture contains complete stationary samples");
        if(!samples) facing=body;
        a.Update(frame,time,eye,left,right,base,facing,tracked!=0,false,yaw,neck,forward
#ifndef OLD_HEIGHT_API
            ,std::max(0.f,neutral-eye.y)
#endif
        );
        Check(!a.rotating && !a.heading.active,"repeated stationary glances cannot become a body turn after several seconds");
        std::printf("body-input: %llu %.17g %.9g %.9g %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g -> %.9g %.9g %d %d %d drop=%.9g\n",
            frame,time,base,facing,tracked,walking,yaw,neck,eye.x,eye.y,eye.z,left.x,left.y,left.z,right.x,right.y,right.z,
            forward.x,forward.y,forward.z,a.head.x,a.head.z,int(a.held),int(a.rotating),int(a.translating),std::max(0.f,neutral-eye.y));

        ++samples;
    }
    std::fclose(f);Check(samples>=1100,"entire repeated-glance sequence replayed");
#ifndef OLD_HEIGHT_API
    // A sustained lowered inspection does not expire or release at centre.
    ControllerBodyAnchor hold;
    Vec eye{0,1.5f,.12f},left{-.18f,1.2f,.3f},right{.18f,1.2f,.3f};
    for(int i=0;i<1800;++i) {
        const float yaw=.65f*std::sin(i*.025f);
        const auto forward=YawVector({0,-.93f,.36756f},yaw);
        hold.Update(i+1,1+i/60.,YawVector(eye,yaw),YawVector(left,yaw),YawVector(right,yaw),0,0,true,false,yaw,.15f,forward,.25f);
        Check(!hold.rotating && !hold.heading.active,"30 seconds of correlated inspection motion cannot expire the yaw hold");
    }
    // Independent straight travel releases the hold, regardless of direction.
    for(Vec direction:{Vec{1,0,0},Vec{-1,0,0},Vec{0,0,1},Vec{0,0,-1}}) {
        ControllerBodyAnchor step;
        for(int i=0;i<30;++i) step.Update(i+1,1+i/60.,eye,left,right,0,0,true,false,0,.15f,{0,-.93f,.36756f},.25f);
        Check(step.loweredInspectionHold,"deep lowered inspection is held before walking");
        const auto before=step.head;
        for(int i=0;i<90;++i) {
            const auto travel=Scale(direction,(i+1)*.008f);
            step.Update(i+31,1.5+i/60.,Add(eye,travel),Add(left,travel),Add(right,travel),0,0,true,false,0,.15f,{0,-.93f,.36756f},.25f);
        }
        Check(!step.loweredInspectionHold && Dot(Sub(step.head,before),direction)>.55f,
            "four-direction walking releases the inspection without freezing roomscale travel");
    }
    hold.Update(1801,31.02,eye,left,right,0,0,true,false,0,.15f,{0,0,1},.25f);
    Check(!hold.loweredInspectionHold,"looking up restores normal facing inference");
#endif
    std::printf("PASS: %d sustained-inspection checks\n",checks);
}
