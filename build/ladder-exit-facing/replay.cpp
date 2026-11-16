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
    char line[4096]{};int samples=0,errors=0,lineNumber=0;float worst=0; int state=0;
    while(std::fgets(line,sizeof(line),file)) {
        ++lineNumber;
        if(!std::strncmp(line,"fp-eye: state=",14)) std::sscanf(line+14,"%d",&state);
        if(!std::strncmp(line,"body-input-reset",16)) {anchor.Reset();continue;}
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
        const bool was=anchor.rotating;
        const float priorTarget=anchor.heading.target;
        anchor.Update(frame,time,eye,left,right,base,body,tracked!=0,walking!=0,yaw,neck,forward);
        if((anchor.rotating && !was) || (anchor.rotating && std::fabs(tr::firstperson::BodyAngle(anchor.heading.target-priorTarget))>.4f))
            std::printf("TURN line=%d pose=%llu state=%d walk=%d held=%d body=%.1f head=%.1f target=%.1f target-step=%.1f reference-age=%.2f pair=%.3f hands=(%.3f,%.3f) headHeight=%.3f handsY=(%.3f,%.3f)\n",
                lineNumber,frame,state,walking,anchor.held,body*57.29578f,yaw*57.29578f,anchor.heading.target*57.29578f,
                tr::firstperson::BodyAngle(anchor.heading.target-priorTarget)*57.29578f,time-anchor.heading.referenceTime,
                tr::firstperson::Length(tr::firstperson::Flat(tr::motiongun::Sub(right,left))),anchor.handTravel[0],anchor.handTravel[1],eye.y,left.y,right.y);
        if(anchor.rotating && !was) {
            const auto prior=tr::motiongun::Sub(anchor.heading.hands[1],anchor.heading.hands[0]);
            const auto current=tr::motiongun::Sub(right,left);
            std::printf("  origin-body=%.1f local-hand-turn=%.1f absolute-hand-turn=%.1f local-head-turn=%.1f\n",
                anchor.heading.bodyOrigin*57.29578f,
                tr::firstperson::BodyAngle(std::atan2(current.x,current.z)-std::atan2(prior.x,prior.z))*57.29578f,
                tr::firstperson::BodyAngle(std::atan2(current.x,current.z)-anchor.heading.facingOrigin)*57.29578f,
                tr::firstperson::BodyAngle(yaw-anchor.heading.headReference)*57.29578f);
        }
        const float error=std::hypot(anchor.head.x-x,anchor.head.z-z);worst=std::max(worst,error);++samples;
        if(error>.00001f || anchor.held!=(held!=0) || anchor.rotating!=(turn!=0) || anchor.translating!=(move!=0)) {
            if(errors<8) std::printf("Mismatch at line %d pose %llu: position %.4f mm, flags %d/%d/%d expected %d/%d/%d\n",
                lineNumber,frame,error*1000,int(anchor.held),int(anchor.rotating),int(anchor.translating),held,turn,move);
            ++errors;
        }
    }
    std::fclose(file);
    std::printf("Replayed %d poses; %d mismatches; maximum position error %.6f mm\n",samples,errors,worst*1000);
    return samples==0 ? 2 : errors ? 1 : 0;
}
