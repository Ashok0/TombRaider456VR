// Standalone math/input checks: no game hooks, DLL loading or VR runtime.
#include "BodyFollower.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
static int checks=0;
static void Check(bool ok,const char* why) {
    ++checks;
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1); }
}
int main() {
    HeightCrouch crouch;
    Frame native[15]{};
    const Vec p[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    for(int i=0;i<15;++i) native[i]={IdentityBasis(),p[i]};
    Frame standing[15]{};
    Check(SolveFollowingBody(native,p[14],p[14],0,IdentityBasis(),{0,0,1},1000,{},0,true,0,0,standing,1),"standing solve");
    for(float drop:{.15f,.25f,.35f,.45f,.54f}) {
        Check(!crouch.Update(true,drop),"partial squat retains visible body");
        Frame pose[15]{};
        Check(SolveFollowingBody(native,p[14],Add(p[14],{0,drop*1000,0}),0,IdentityBasis(),
            {0,0,1},1000,{},0,true,0,0,pose,StandingPostureWeight(drop)),"squat solve");
        Check(pose[0].origin.y>standing[0].origin.y+50,"squat lowers pelvis");
        for(int first:{1,4}) {
            Check(KneeBendDegrees(pose,first)>KneeBendDegrees(standing,first)+15,"knees bend before native crawl");
            Check(Length(Sub(pose[first+2].origin,p[first+2]))<.05f,"feet stay planted");
            for(int j=first;j<first+2;++j)
                Check(std::fabs(Length(Sub(pose[j+1].origin,pose[j].origin))-Length(Sub(p[j+1],p[j])))<.05f,"leg lengths preserved");
        }
    }
    Check(crouch.Update(true,.56f),"deep squat requests native crouch");
    Check(crouch.Update(true,.3f),"hysteresis prevents repeated transitions");
    Check(!crouch.Update(true,.22f),"rising releases crouch");
    Check(!crouch.Update(true,0) && !crouch.Update(true,-.1f),"standing and raised HMD stay released");
    Check(crouch.Update(true,.6f) && !crouch.Update(false,.6f),"gate releases crouch");
    Check(!crouch.Update(true,NAN),"invalid height releases crouch");
    constexpr auto duck=PhysicalCrouchHoldScope::Duck;
    for(uint8_t savedMode:{uint8_t(0),uint8_t(1)}) for(bool held:{false,true}) for(bool manual:{false,true}) {
        uint8_t mode=savedMode;
        uint64_t input=0x41|(manual ? duck : 0);
        const auto before=input;
        {
            PhysicalCrouchHoldScope scope(&input,&mode,held);
            Check(mode==0,"physical cycle uses hold semantics for both button settings");
            Check(bool(input&duck)==(held||manual),"posture releases without canceling manual Duck");
            Check((input&~duck)==(before&~duck),"other controls retained");
            input|=0x10; // A native handler's unrelated edit must survive.
        }
        Check(mode==savedMode,"button setting restored");
        Check(input==(before|0x10),"only synthetic Duck removed after native tick");
    }
    std::printf("Physical crouch: %d standalone checks passed.\n",checks);
}
