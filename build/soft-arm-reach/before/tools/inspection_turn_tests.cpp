#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* msg) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
void Synthetic() {
    {
        ControllerBodyAnchor hold;
        const Vec eye{0,1.45f,.12f},left{-.18f,1.2f,.35f},right{.18f,1.2f,.35f};
        for(int i=0;i<120;++i) {
            const float angle=.5f*std::min(1.f,i/60.f);
            hold.Update(i+1,1+i/60.,eye,YawVector(left,angle),YawVector(right,angle),0,0,true,false,0,.15f,{0,-.93f,.36756f},.25f);
        }
        Check(hold.loweredInspectionHold,"arm gestures cannot release a downward inspection");
        const Vec pair=YawVector(Sub(right,left),.5f);
        hold.Update(121,3,eye,YawVector(left,.5f),YawVector(right,.5f),0,0,true,false,0,.15f,{0,0,1},0);
        Check(!hold.loweredInspectionHold && !hold.heading.active &&
              std::fabs(BodyAngle(hold.heading.facingOrigin-std::atan2(pair.x,pair.z)))<.001f,
              "ordinary inspection exit discards arm gestures instead of saving them for a later turn");
    }
    for(int hz:{60,90,120}) for(float direction:{-1.f,1.f}) for(float speed:{.7f,1.4f}) {
        ControllerBodyAnchor anchor;BodyFollower body;BodyFollowSettings settings;
        const Vec eye{0,1.45f,.12f},left{-.18f,1.2f,.35f},right{.18f,1.2f,.35f},forward{0,-.93f,.36756f};
        double now=1;uint64_t serial=0;float yaw=0,angle=0;bool turned=false;
        auto update=[&](float head,float hands) {
            const float before=yaw;
            anchor.Update(++serial,now,YawVector(eye,head),YawVector(left,hands),YawVector(right,hands),
                0,yaw,true,false,head,.15f,YawVector(forward,head),.25f);
            body.Update(serial,now,0,head,0,{}, {},false,settings,{},false,false,0,true,true,
                anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
            yaw=body.yaw;now+=1./hz;
            Check(std::fabs(BodyAngle(yaw-before))<=3.141593f/hz+.0001f,"reacquired turn uses bounded existing body blending");
            turned=turned || anchor.rotating;
        };
        for(int i=0;i<hz;++i) update(0,0);
        Check(anchor.loweredInspectionHold,"fixture starts in a latched downward inspection");
        // Cross +/-pi and continue beyond a revolution, without looking up.
        for(int i=0;i<int(7.f/speed*hz);++i) {angle=direction*speed*i/hz;update(angle,angle);}
        Check(turned && std::fabs(BodyAngle(yaw-angle))<.15f,"looking down cannot permanently disable physical turning");
        Check(!anchor.loweredInspectionHold,"confirmed turn releases the lowered hold");
        for(int i=0;i<hz;++i) update(angle,angle);
        const float settled=yaw;
        Check(!anchor.rotating,"stopping physical motion ends turn continuation");
        for(int i=0;i<hz*3;++i) {
            update(angle+.5f*std::sin(i*3.f/hz),angle);
            Check(std::fabs(BodyAngle(yaw-settled))<.001f,"head-only glances after a turn cannot reuse its evidence");
        }
        for(int i=0;i<hz*3;++i) {
            update(angle,angle+.45f*std::sin(i*3.f/hz));
            Check(std::fabs(BodyAngle(yaw-settled))<.001f,"hand gestures after a turn cannot reuse its evidence");
        }
    }
}
void Recorded(const char* path) {
    FILE* file=nullptr;fopen_s(&file,path,"r");Check(file!=nullptr,"reported failed-turn capture opens");
    ControllerBodyAnchor anchor;char line[4096]{};double start=-1;int count[3]{},aligned[3]{};
    while(std::fgets(line,sizeof(line),file)) {
        if(!std::strncmp(line,"body-input-reset",16)){anchor.Reset();continue;}
        if(std::strncmp(line,"body-input: ",12)) continue;
        unsigned long long serial;double time;float base,body,yaw,neck;int tracked,walking;
        Vec eye{},left{},right{},forward{};
        const int n=std::sscanf(line+12,"%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            &serial,&time,&base,&body,&tracked,&walking,&yaw,&neck,&eye.x,&eye.y,&eye.z,
            &left.x,&left.y,&left.z,&right.x,&right.y,&right.z,&forward.x,&forward.y,&forward.z);
        Check(n==20,"recorded tracking sample is complete");if(start<0)start=time;
        const char* drop=std::strstr(line," drop=");
        anchor.Update(serial,time,eye,left,right,base,body,tracked!=0,walking!=0,yaw,neck,forward,drop?float(std::atof(drop+6)):0.f);
        const double t=time-start;
        const int interval=t>=53.8 && t<55.5 ? 0 : t>=59 && t<60 ? 1 : t>=243 && t<244.5 ? 2 : -1;
        if(interval>=0) {
            ++count[interval];
            if(anchor.rotating && anchor.heading.active && std::fabs(BodyAngle(anchor.heading.target-yaw))<1.04719755f) ++aligned[interval];
        }
    }
    std::fclose(file);
    for(int i=0;i<3;++i) {
        std::printf("Recorded turn %d: %d/%d samples active and within 60 degrees of observed facing\n",i+1,aligned[i],count[i]);
        Check(count[i]>45 && aligned[i]>.8f*count[i],"reported sustained turns recover their absolute facing instead of restarting midway");
    }
}
int main(int argc,char** argv) {
    Check(argc==2,"failed-turn recording supplied");Synthetic();Recorded(argv[1]);
    std::printf("PASS: %d inspection-turn recovery checks\n",checks);
}
