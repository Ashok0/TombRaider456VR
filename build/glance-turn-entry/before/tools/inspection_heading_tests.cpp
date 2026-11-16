#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* message) {
    ++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
#ifndef BASELINE_CAPTURE_ONLY
#include "close_hands_motion_regression.inl"
#endif
int main(int argc,char** argv) {
    Check(argc==2,"captured ladder-exit glance supplied");
    FILE* f=nullptr;fopen_s(&f,argv[1],"r");Check(f!=nullptr,"capture opens");
    ControllerBodyAnchor recorded;
    char line[4096]{};int samples=0;float planted=0;
    while(std::fgets(line,sizeof(line),f)) {
        unsigned long long frame=0;double time=0;float base=0,body=0,yaw=0,neck=0;
        int tracked=0,walking=0;Vec eye{},left{},right{},forward{};
        const int n=std::sscanf(line+12,
            "%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            &frame,&time,&base,&body,&tracked,&walking,&yaw,&neck,
            &eye.x,&eye.y,&eye.z,&left.x,&left.y,&left.z,&right.x,&right.y,&right.z,
            &forward.x,&forward.y,&forward.z);
        Check(n==20,"recorded sample parses");
        if(!samples || walking) planted=body;
        recorded.Update(frame,time,eye,left,right,base,planted,tracked!=0,walking!=0,yaw,neck,forward);
        if(frame>=21030) Check(!recorded.rotating && !recorded.heading.active,
            "captured downward glance after ladder cannot authorize a body turn");
        ++samples;
    }
    std::fclose(f);Check(samples>=150,"complete captured glance replayed");
#ifndef BASELINE_CAPTURE_ONLY
    TestCloseHandsRecordedMotion();
    // Intentional, coordinated roomscale turns work looking ahead or down,
    // in either direction, at different controller extensions and frame rates.
    for(float pitch:{0.f,-.75f}) for(float sign:{-1.f,1.f})
    for(float reach:{.25f,.5f,.7f}) for(int hz:{60,90,120}) {
        ControllerBodyAnchor a;float body=0;bool accepted=false;
        for(int i=0;i<=hz*3;++i) {
            const float angle=sign*1.2f*std::min(1.f,float(i)/(hz*2));
            auto eye=YawVector({0,1.7f,.12f},angle);
            auto left=YawVector({-.18f,1.4f,reach},angle),right=YawVector({.18f,1.4f,reach},angle);
            const auto forward=YawVector({0,pitch,std::sqrt(1-pitch*pitch)},angle);
            a.Update(i+1,1.+double(i)/hz,eye,left,right,0,body,true,false,angle,.15f,forward);
            if(a.heading.active) {body=a.heading.target;accepted=true;}
            Check(!a.inspectionHold,"coordinated roomscale turn is not mistaken for a head-led inspection");
        }
        Check(accepted && std::fabs(BodyAngle(body-sign*1.2f))<.22f,
            "roomscale turns remain responsive in both directions, including downward gaze");
    }
    // Looking over a holster must not stop real translation in any direction.
    for(Vec direction:{Vec{1,0,0},Vec{-1,0,0},Vec{0,0,1},Vec{0,0,-1}}) {
        ControllerBodyAnchor a;Vec eye{0,1.7f,0},left{-.18f,1.4f,.3f},right{.18f,1.4f,.3f};
        const Vec forward=YawVector({0,-.75f,std::sqrt(1-.75f*.75f)},.9f);
        for(int i=0;i<30;++i) a.Update(i+1,1.+i/60.,eye,left,right,0,0,true,false,.9f,.15f,forward);
        const Vec before=a.head;
        for(int i=0;i<90;++i) {
            eye=Add(eye,Scale(direction,.008f));left=Add(left,Scale(direction,.008f));right=Add(right,Scale(direction,.008f));
            a.Update(i+31,1.5+i/60.,eye,left,right,0,0,true,false,.9f,.15f,forward);
        }
        Check(Dot(Sub(a.head,before),direction)>.55f,"inspection hold preserves roomscale walking in all four directions");
        a.Update(122,3.02,eye,left,right,0,0,true,true,.9f,.15f,forward);
        Check(!a.inspectionHold,"explicit stick movement releases inspection hold");
        a.Update(123,3.04,eye,left,right,0,0,true,false,0,.15f,{0,0,1});
        Check(!a.inspectionHold,"raising and centering gaze restores normal turn inference");
    }
#endif
    std::printf("PASS: %d inspection/turn/walking checks\n",checks);
}
