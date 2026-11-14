// Build with cl /std:c++17 /EHsc /Isrc tools/body_anchor_replay.cpp
// Usage: body_anchor_replay.exe path\to\TombRaiderVR.log
#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
int main(int argc,char** argv) {
    if(argc!=2) {std::fprintf(stderr,"Usage: body_anchor_replay.exe session.log\n");return 2;}
    FILE* file=nullptr;if(fopen_s(&file,argv[1],"r") || !file) return 2;
    tr::firstperson::ControllerBodyAnchor anchor;
    tr::firstperson::BodyFollower follower;float yawError=0;
    char line[4096]{};int samples=0,errors=0,lineNumber=0;float worst=0;
    while(std::fgets(line,sizeof(line),file)) {
        ++lineNumber;
        if(!std::strncmp(line,"body-input-reset",16)) {anchor.Reset();follower.Reset();continue;}
        if(std::strncmp(line,"body-input: ",12)) continue;
        unsigned long long frame=0;double time=0;float base=0,body=0,yaw=0,neck=0,x=0,z=0;
        int tracked=0,walking=0,held=0,turn=0,move=0;
        tr::motiongun::Vec eye{},left{},right{},forward{};
        const int read=std::sscanf(line+12,
            "%llu %lf %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f -> %f %f %d %d %d",
            &frame,&time,&base,&body,&tracked,&walking,&yaw,&neck,
            &eye.x,&eye.y,&eye.z,&left.x,&left.y,&left.z,&right.x,&right.y,&right.z,
            &forward.x,&forward.y,&forward.z,&x,&z,&held,&turn,&move);
        if(read!=25) {std::fprintf(stderr,"Incomplete input at line %d\n",lineNumber);++errors;continue;}
        const float predicted=follower.valid ? tr::firstperson::BodyAngle(follower.yaw-base) : body;
        if(walking) {follower.Reset();}
        else yawError=std::max(yawError,std::fabs(tr::firstperson::BodyAngle(predicted-body)));
        anchor.Update(frame,time,eye,left,right,base,walking ? body : predicted,tracked!=0,walking!=0,yaw,neck,forward);
        follower.Update(frame,time,base,base+anchor.yaw,base+yaw,{},{},walking!=0,tr::firstperson::BodyFollowSettings(),
            {},false,true,0,anchor.held,anchor.valid,
            anchor.heading.active ? base+anchor.heading.target : NAN,anchor.rotating);
        std::printf("EVID,%llu,%d,%d,%d,%.3f\n",frame,anchor.pivotEvidence.dbgStraight,anchor.pivotEvidence.dbgClose,anchor.pivotEvidence.dbgArc,anchor.pivotEvidence.until-time);
        std::printf("YAW,%llu,%.9g,%.9g\n",frame,body,predicted);
        std::printf("POSE,%llu,%.9g,%.9g,%d,%d,%d\n",frame,anchor.head.x,anchor.head.z,int(anchor.held),int(anchor.rotating),int(anchor.translating));
        const float error=std::hypot(anchor.head.x-x,anchor.head.z-z);worst=std::max(worst,error);++samples;
        if(error>.00001f || anchor.held!=(held!=0) || anchor.rotating!=(turn!=0) || anchor.translating!=(move!=0)) {
            if(errors<0) std::printf("Mismatch at line %d pose %llu: position %.4f mm, flags %d/%d/%d expected %d/%d/%d\n",
                lineNumber,frame,error*1000,int(anchor.held),int(anchor.rotating),int(anchor.translating),held,turn,move);
            ++errors;
        }
    }
    std::fclose(file);std::printf("Maximum pre-pose body yaw error %.6f deg\n",yawError*57.2957795f);
    std::printf("Replayed %d poses; %d mismatches; maximum position error %.6f mm\n",samples,errors,worst*1000);
    return samples==0 ? 2 : 0;
}
