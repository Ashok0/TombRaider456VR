// Pure regression with installed PDP animation records; no game DLL is loaded.
#include "../src/ValveTurn.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

template<class T>T Read(const std::vector<char>& bytes,size_t at) {
    if(at+sizeof(T)>bytes.size()) throw std::runtime_error("asset bounds");
    T value;std::memcpy(&value,bytes.data()+at,sizeof(value));return value;
}
int main(int argc,char** argv) {
    using namespace tr::firstperson;
    if(argc!=2) return 2;
    int checks=0;
    auto check=[&](bool good,const char* text){++checks;if(!good){std::printf("FAIL: %s (%d)\n",text,checks);std::exit(1);}};
    constexpr float pi=3.14159265358979323846f;
    for(const auto* name:{"ANDREA2","JOBY3","JOBY5"}) {
        std::ifstream in(std::filesystem::path(argv[1])/"5"/"DATA"/(std::string(name)+".PDP"),std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
        size_t at=0,offsets[7]{};uint32_t counts[7]{};const int strides[]={40,6,8,2,4,2,18};
        for(int t=0;t<7;++t){counts[t]=Read<uint32_t>(bytes,at);at+=4;offsets[t]=at;at+=counts[t]*strides[t];}
        std::vector<SwitchAnim> anims(counts[0]);
        for(size_t i=0;i<anims.size();++i) {
            auto& a=anims[i];const auto p=offsets[0]+i*40;
            a.frames=reinterpret_cast<const int16_t*>(bytes.data()+offsets[5]+Read<uint32_t>(bytes,p));
            std::memcpy(reinterpret_cast<char*>(&a)+8,bytes.data()+p+4,36);
        }
        int base=-1;
        for(uint32_t o=0;o<counts[6];++o) if(Read<uint32_t>(bytes,offsets[6]+o*18)==274) base=Read<int16_t>(bytes,offsets[6]+o*18+16);
        check(base>=0,"airlock object exists");SwitchPosePath path;
        check(BuildValvePath(path,anims.data(),base,{int16_t(base),anims[base].first,0}),"native valve pose path supported");
        check(path.count==168,"complete turn and first post-use pose");
        check(!BuildValvePath(path,anims.data(),base,{int16_t(base+2),anims[base+2].first,1}),"completed one-use valve cannot be turned again");
        check(BuildValvePath(path,anims.data(),base,{int16_t(base),anims[base].first,0}),"fresh path after rejected reuse");
        float samples[256]{};
        for(int i=0;i<path.count;++i) {
            if(i==path.count-1){samples[i]=samples[i-1];break;} // native post-use/drop clip
            const auto pose=path.poses[i];const auto& a=anims[pose.animation];
            const auto* frame=a.frames+(pose.frame-a.first)*(uint16_t(a.interpolation)>>8);
            const uint16_t packed=uint16_t(frame[9]);
            check((packed>>14)==3 || packed==0x4000,"valve rotates about its wall-facing axis");
            samples[i]=(packed>>14)==3 ? (packed&4095)*2*pi/4096 : 0.f;
            for(int c=6;c<9;++c) check(std::abs(frame[c]-anims[base].frames[c])<=8,"rotation centre stable until native completion");
        }
        ValveTurn turn;check(turn.Build(samples,path.count),"unwrap actual authored valve motion");
        check(std::fabs(turn.angles[turn.count-1]-5.210854f)<.02f,"nearly 300-degree turn required");
        check(turn.Pose()==0 && !turn.Complete(),"grabbing does not advance anticipation or complete valve");
        float hand=0;turn.Grab(hand);
        for(int i=0;i<200;++i)check(turn.Track(hand) && turn.Pose()==0 && !turn.Complete(),"stationary hand cannot turn valve");
        check(turn.Track(.15f) && turn.Track(0) && turn.progress==0,"wrong-way movement at start cannot manufacture progress on return");
        for(int i=0;i<40;++i){hand-=.06f;check(turn.Track(hand),"first hand turn");}
        const auto parked=turn;const int parkedPose=turn.Pose();const float parkedAngle=turn.progress;
        // Re-grab an entirely different point on the rim, preserving progress.
        hand=2.8f;turn.Grab(hand);
        check(turn.Track(hand) && turn.Pose()==parkedPose && turn.progress==parkedAngle,"re-grab at another rim point does not snap or advance");
        for(int i=0;i<8;++i){hand+=.05f;check(turn.Track(hand),"reverse movement is accepted");}
        check(turn.progress<parkedAngle-.39f && !turn.Complete(),"reversing unwinds the valve");
        for(int i=0;i<8;++i){hand-=.05f;check(turn.Track(hand),"return to previous angle");}
        check(std::fabs(turn.progress-parkedAngle)<.001f,"back-and-forth does not create progress");
        const int beforeJump=turn.Pose();
        check(!turn.Track(hand+1.5f) && turn.Pose()==beforeJump,"tracking jump leaves parked position intact");
        turn=parked;hand=-2.4f;turn.Grab(hand);
        for(int i=0;i<20;++i){hand-=.06f;check(turn.Track(std::remainder(hand,2*pi)),"cross +/-pi without switching to an earlier revolution");}
        check(turn.progress>3.59f && !turn.Complete(),"passing 180 degrees is not premature completion");
        while(!turn.Complete()){hand-=.03f;check(turn.Track(std::remainder(hand,2*pi)),"finish full circular path");}
        check(turn.progress>5.19f,"endpoint reached through actual rotation");
        // Commit starts via native SwitchCollision (which animates once), then
        // SwitchControl must visit EVERY remaining frame, including last-1.
        int animation=base+1,frame=anims[base+1].first,linkedToggles=0;
        const int steps=path.count-1;
        for(int i=1;i<steps;++i) {
            if(animation==base+1 && frame==anims[base+1].last-1) ++linkedToggles;
            if(++frame>anims[animation].last){const auto old=anims[animation];animation=old.jump;frame=old.jumpFrame;}
        }
        check(linkedToggles==1,"native linked mechanism fires exactly once");
        check(animation==base+2 && frame==anims[base+2].first,"post-use motion remains with native game");
        std::printf("PASS: %s valve rotation, re-grab, tracking discontinuities and native trigger sequence\n",name);
    }
    for(bool markets:{false,true}) for(int i=0;i<32;++i) {
        const float a=i*2*pi/32;const tr::motiongun::Vec hand{300*std::cos(a),300*std::sin(a),0};
        const auto p=ValveRimContact(hand,markets);
        check(std::fabs(std::hypot(p.x-(markets ? 3.f : 0.f),p.y-6.4f)-112.f)<.001f,"contact lies on visible rim centre line");
        check(p.z==(markets ? -83.f : -82.f),"contact uses front wheel rim, not hub/wall");
    }
    std::printf("PASS: %d valve checks; no game DLL loaded\n",checks);
}
