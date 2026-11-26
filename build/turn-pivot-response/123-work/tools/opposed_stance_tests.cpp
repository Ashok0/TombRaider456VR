#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* message) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
#ifndef BASELINE_RECORDING
void Synthetic() {
    {
        ControllerBodyAnchor disabled;
        for(int i=0;i<240;++i) {
            const float yaw=i<30 ? 0.f : 3.14159265f;
            disabled.Update(i+1,1+i/60.,YawVector({0,1.45f,.15f},yaw),
                YawVector({-.18f,1.2f,.4f},yaw),YawVector({.18f,1.2f,.4f},yaw),
                0,0,true,false,yaw,.15f,YawVector({0,-.93f,.36756f},yaw),.25f,false);
            Check(!disabled.opposedRecovery.active,"disabled hand-assisted body yaw cannot trigger forced turn recentering");
        }
    }
    for(int hz:{60,90,120}) for(float sign:{-1.f,1.f}) for(float seconds:{4.f,8.f,16.f}) {
        ControllerBodyAnchor anchor;BodyFollower body;BodyFollowSettings settings;
        const Vec eye{0,1.45f,.15f},left{-.18f,1.15f,.4f},right{.18f,1.15f,.4f};
        double now=1;uint64_t frame=0;float actual=0;bool recovered=false;
        auto update=[&](float yaw,float hands,float pitch,float height) {
            Vec e=YawVector(eye,yaw);e.y+=height;
            auto l=YawVector(left,hands),r=YawVector(right,hands);l.y+=height;r.y+=height;
            const auto before=anchor.head;const float beforeYaw=body.yaw;
            const auto forward=YawVector({0,-std::sin(pitch),std::cos(pitch)},yaw);
            anchor.Update(++frame,now,e,l,r,0,body.yaw,true,false,yaw,.15f,forward,.25f-height);
            body.Update(frame,now,0,yaw,0,{}, {},false,settings,anchor.head,false,true,0,true,true,
                anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
            if(frame>1) {
                Check(std::fabs(BodyAngle(body.yaw-beforeYaw))<=3.141593f/hz+.001f,"turn catch-up stays within the body yaw rate limit");
                if(anchor.opposedRecovery.active) Check(Length(Flat(Sub(anchor.head,before)))<=1.35f/hz+.001f,
                    "opposed stance recenters smoothly instead of teleporting the mesh");
            }
            recovered=recovered || anchor.opposedRecovery.active;now+=1./hz;
        };
        for(int i=0;i<hz;++i) update(0,0,1.2f,0);
        for(int i=0;i<int(seconds*hz);++i) {
            actual=sign*3.14159265f*(i+1)/(seconds*hz);
            // Natural head elevation/pitch changes intentionally violate the
            // short rigid-orbit detector that rejected the reported turn.
            update(actual,actual,1.2f+.18f*std::sin(i*4.f/hz),.07f*std::sin(i*3.f/hz));
        }
        for(int i=0;i<hz*3;++i) update(actual,actual,1.2f,0);
        if(std::fabs(BodyAngle(body.yaw-actual))>=.06f) std::printf("case hz=%d sign=%.0f seconds=%.0f yaw=%.1f actual=%.1f recovered=%d active=%d held=%d target=%.1f facingOrigin=%.1f bodyOrigin=%.1f\n",hz,sign,seconds,body.yaw*57.29578f,actual*57.29578f,recovered,anchor.opposedRecovery.active,anchor.loweredInspectionHold,anchor.heading.target*57.29578f,anchor.heading.facingOrigin*57.29578f,anchor.heading.bodyOrigin*57.29578f);
        Check(std::fabs(BodyAngle(body.yaw-actual))<.06f,"slow half-turn recovers even after all tracked movement stops");
        Check(Length(Flat(Sub(anchor.head,YawVector(eye,actual))))<.10f,"recovered body does not stay on the old side of the player");
        const auto settled=anchor.head;const float settledYaw=body.yaw;
        for(int i=0;i<hz*3;++i) {
            update(actual+.5f*std::sin(i*3.f/hz),actual,1.2f,0);
            Check(std::fabs(BodyAngle(body.yaw-settledYaw))<.002f && Length(Flat(Sub(anchor.head,settled)))<.002f,
                "head-only glances after catch-up cannot drag the mesh");
        }
    }
    OpposedStanceRecovery recovery;
    for(int i=0;i<200;++i) Check(!recovery.Update(i/90.,0,3.14f,0,.3f,{}, {},true,false),
        "head turning alone cannot request opposed-stance recovery");
    for(int i=0;i<200;++i) Check(!recovery.Update(3+i/90.,0,0,3.14f,.3f,{}, {},true,false),
        "controller gestures alone cannot request opposed-stance recovery");
    for(int i=0;i<150;++i) recovery.Update(6+i/90.,0,3.14f,3.14f,.3f,{.2f,0,0},{},true,false);
    Check(recovery.active,"opposed pose can authorize catch-up without frame-to-frame motion");
    const float target=recovery.target;const auto position=recovery.position;
    recovery.Update(7.7,0,2.5f,2.8f,.3f,{.8f,0,.4f},{},false,false);
    Check(recovery.active && recovery.target==target && Length(Sub(position,recovery.position))==0,
          "accepted recovery pose is latched rather than following subsequent glances");
    recovery.Update(7.71,0,3.14f,3.14f,.3f,{}, {},true,true);
    Check(!recovery.active,"explicit stick walking immediately cancels catch-up");
}
#endif
void Recorded(const char* path) {
    FILE* file=nullptr;fopen_s(&file,path,"r");Check(file!=nullptr,"reported slow-turn capture opens");
    ControllerBodyAnchor anchor;BodyFollower follower;BodyFollowSettings settings;
    char line[4096]{};double start=-1;bool simulate=false;int observed=0,corrected=0;float worst=0;
    while(std::fgets(line,sizeof(line),file)) {
        if(!std::strncmp(line,"body-input-reset",16)){anchor.Reset();continue;}
        if(std::strncmp(line,"body-input: ",12)) continue;
        unsigned long long serial;double time;float base,body,yaw,neck;int tracked,walking;Vec eye{},left{},right{},forward{};
        const int n=std::sscanf(line+12,"%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            &serial,&time,&base,&body,&tracked,&walking,&yaw,&neck,&eye.x,&eye.y,&eye.z,&left.x,&left.y,&left.z,
            &right.x,&right.y,&right.z,&forward.x,&forward.y,&forward.z);
        Check(n==20,"capture contains complete head and controller positions");if(start<0)start=time;
        const double t=time-start;if(t>=47)break;
        if(t>=31 && !simulate) {
            simulate=true;follower.Update(serial-1,time-1./60.,base,base+body,base+body,{}, {},false,settings,anchor.head);
        }
        if(simulate)body=BodyAngle(follower.yaw-base);
        const char* drop=std::strstr(line," drop=");
        anchor.Update(serial,time,eye,left,right,base,body,tracked!=0,walking!=0,yaw,neck,forward,drop?float(std::atof(drop+6)):0);
        if(simulate)follower.Update(serial,time,base,base+yaw,base+body,{}, {},false,settings,anchor.head,false,true,0,true,true,
            anchor.heading.active ? BodyAngle(base+anchor.heading.target) : NAN,anchor.rotating);
        if(t>=36 && t<38) {
            ++observed;const float error=std::fabs(BodyAngle(follower.yaw-base-yaw));worst=std::max(worst,error);
            if(error<.65f)++corrected;
        }
    }
    std::fclose(file);
    std::printf("Closed-loop slow turn: %d/%d samples recovered, maximum head/body difference %.1f degrees\n",corrected,observed,worst*57.29578f);
    Check(observed>90 && corrected>.95f*observed,"reported completed half-turn no longer leaves Lara facing the opposite way");
}
int main(int argc,char** argv) {
    Check(argc==2,"capture supplied");
#ifndef BASELINE_RECORDING
    Synthetic();
#endif
    Recorded(argv[1]);std::printf("PASS: %d opposed stance checks\n",checks);
}
