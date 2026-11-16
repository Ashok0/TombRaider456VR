#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tr::firstperson;using namespace tr::motiongun;
void Check(bool ok,const char* msg){if(!ok){std::printf("FAIL: %s\n",msg);std::exit(1);}}
void Recorded(const char* path) {
    FILE* file=nullptr;fopen_s(&file,path,"r");Check(file!=nullptr,"reported slow-turn capture opens");
    ControllerBodyAnchor anchor;BodyFollower follower;BodyFollowSettings settings;
    char line[4096]{};double start=-1;bool simulate=false;int observed=0,corrected=0,positionSamples=0;float worst=0,positionSum=0;
    while(std::fgets(line,sizeof(line),file)) {
        if(!std::strncmp(line,"body-input-reset",16)){anchor.Reset();continue;}
        if(std::strncmp(line,"body-input: ",12)) continue;
        unsigned long long serial;double time;float base,body,yaw,neck;int tracked,walking;Vec eye{},left{},right{},forward{};
        const int n=std::sscanf(line+12,"%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            &serial,&time,&base,&body,&tracked,&walking,&yaw,&neck,&eye.x,&eye.y,&eye.z,&left.x,&left.y,&left.z,
            &right.x,&right.y,&right.z,&forward.x,&forward.y,&forward.z);
        Check(n==20,"capture contains complete head and controller positions");if(start<0)start=time;
        const double t=time-start;if(t>=252)break;
        if(t>=197 && !simulate) {
            simulate=true;follower.Update(serial-1,time-1./60.,base,base+body,base+body,{}, {},false,settings,anchor.head);
        }
        if(simulate)body=BodyAngle(follower.yaw-base);
        const char* drop=std::strstr(line," drop=");
        anchor.Update(serial,time,eye,left,right,base,body,tracked!=0,walking!=0,yaw,neck,forward,drop?float(std::atof(drop+6)):0);
        if(simulate)follower.Update(serial,time,base,base+yaw,base+body,{}, {},false,settings,anchor.head,false,true,0,true,true,
            anchor.heading.active ? BodyAngle(base+anchor.heading.target) : NAN,anchor.rotating);
        if(t>=238 && t<248 && walking==0) { 
#ifdef NEW_VISUAL_ANCHOR
            positionSum+=Length(Flat(Sub(anchor.visualHead,eye)));
#else
            positionSum+=Length(Flat(Sub(anchor.head,eye)));
#endif
++positionSamples; }
        if(t>=200 && t<205) {
            ++observed;const float error=std::fabs(BodyAngle(follower.yaw-base-yaw));worst=std::max(worst,error);
            if(error<.35f)++corrected;
        }
    }
    std::fclose(file);std::printf("Mean eye-anchor separation in late sidesteps: %.1f mm (%d poses)\n",positionSum*1000/std::max(1,positionSamples),positionSamples);
    std::printf("Closed-loop slow turn: %d/%d samples recovered, maximum head/body difference %.1f degrees\n",corrected,observed,worst*57.29578f);
    
}
int main(int argc,char**argv){Check(argc==2,"capture supplied");Recorded(argv[1]);}
