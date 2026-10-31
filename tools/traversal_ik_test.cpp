#include "TraversalArmIK.h"
#include "BodyFollower.h"
#include "LocomotionMath.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
using namespace tr::firstperson;
static int checks=0;
static void Check(bool ok,const char* message) {
    ++checks;if(!ok) { std::fprintf(stderr,"FAIL: %s (%d)\n",message,checks);std::exit(1); }
}
static bool Near(Vec a,Vec b,float tolerance=.002f) { return Length(Sub(a,b))<tolerance; }
static bool Near(const Frame& a,const Frame& b,float tolerance=.002f) {
    if(!Near(a.origin,b.origin,tolerance)) return false;
    for(int r=0;r<3;++r) for(int c=0;c<3;++c)
        if(std::fabs(a.basis.r[r][c]-b.basis.r[r][c])>tolerance) return false;
    return true;
}
static void Body(Frame (&body)[15],bool overhead,float elbow) {
    for(auto& f:body) f={IdentityBasis(),{}};
    body[7].origin={0,-180,0};body[14].origin={0,-300,0};
    for(int side:{-1,1}) {
        const int leg=side<0 ? 1 : 4,arm=side<0 ? 11 : 8;
        body[leg].origin={side*60.f,0,0};
        body[leg+1].origin={side*60.f,180,20};body[leg+2].origin={side*60.f,360,0};
        body[arm].origin={side*80.f,-200,0};
        body[arm+1].origin=overhead ? Vec{side*(80.f+elbow),-350,elbow} : Vec{side*(80.f+elbow),-240-elbow,125};
        body[arm+2].origin=overhead ? Vec{side*80.f,-500,0} : Vec{side*80.f,-280,250};
    }
}
static void BodyReach() {
    bool limited=false,allowed=false,oldStretch=false;
    for(bool overhead:{false,true}) for(float elbow:{0.f,.01f,5.f,30.f,80.f})
    for(float yaw:{-2.f,-.5f,0.f,.5f,2.f}) for(float x:{-400.f,0.f,400.f})
    for(float y:{-350.f,0.f,350.f}) for(float z:{-400.f,0.f,400.f}) {
        Frame native[15]{},posed[15]{},unlimited[15]{};Body(native,overhead,elbow);
        const auto rotation=RotationMatrix(AxisRotation({0,1,0},yaw));
        Check(SolveBody(native,{x,y,z},rotation,{0,0,1},500,posed,true),"fixed-contact body solves extreme HMD offsets");
        Check(SolveBody(native,{x,y,z},rotation,{0,0,1},500,unlimited),"existing body path remains available");
        limited|=!Near(posed[7],unlimited[7]);allowed|=!Near(posed[7],native[7]);
        Check(Near(posed[3],native[3]) && Near(posed[6],native[6]),"foot contacts remain exact");
        Frame inverse{};Inverse(native[7],inverse);const Frame chest=Multiply(posed[7],inverse);
        for(int first:{8,11}) {
            const float reach=Length(Sub(native[first+1].origin,native[first].origin))+
                Length(Sub(native[first+2].origin,native[first+1].origin));
            oldStretch|=Length(Sub(native[first+2].origin,unlimited[first].origin))>reach+.1f;
            const Frame arm[3]={posed[first],posed[first+1],posed[first+2]};
            Check(Near(Transform(chest,native[first].origin),arm[0].origin),"shoulder remains attached to chest");
            Frame changes[3]{};
            Check(SolveArm(arm,native[first+2],{first==8 ? 1.f : -1.f,0,-1},changes,nullptr,nullptr,false),"contact arm solves");
            Frame solved[3];for(int i=0;i<3;++i) solved[i]=Multiply(changes[i],arm[i]);
            Check(Near(solved[2],native[first+2]),"authored wrist position AND rotation remain exact");
            for(int i=0;i<2;++i) {
                Check(std::fabs(Length(Sub(solved[i+1].origin,solved[i].origin))-
                    Length(Sub(native[first+i+1].origin,native[first+i].origin)))<.005f,"arm lengths do not stretch");
                for(int col=0;col<3;++col) {
                    const Vec axis{col==0 ? 1.f : 0.f,col==1 ? 1.f : 0.f,col==2 ? 1.f : 0.f};
                    Check(std::fabs(Length(Transform(solved[i].basis,axis))-1)<.0001f,"arm meshes retain rigid scale");
                }
            }
        }
    }
    Check(limited && allowed,"reach constraint limits excessive motion but permits reachable leaning");
    Check(oldStretch,"fixtures reproduce arm stretching in the previous unconstrained solve");
}
static void Continuity() {
    Frame native[3]={{IdentityBasis(),{0,0,0}},{IdentityBasis(),{20,-150,0}},{IdentityBasis(),{0,-300,0}}};
    TraversalArmIK history;
    const auto owner=&history;
    auto pose=[&](uint64_t frame,double time,bool grip,Frame target,const Frame& ref=Frame{IdentityBasis(),{}}) {
        Frame chain[3];for(int i=0;i<3;++i) chain[i]=Multiply(ref,native[i]);
        Frame authored[3];for(int i=0;i<3;++i) authored[i]=chain[i];
        target=Multiply(ref,target);Vec pole=Transform(ref.basis,{1,0,-.25f});
        Check(history.Prepare(owner,frame,time,grip,ref,authored,chain,target,pole),"traversal history prepares");
        Frame changes[3],solved[3];
        Check(SolveArm(chain,target,pole,changes,nullptr,nullptr,false,{},!grip),"history arm solves");
        for(int i=0;i<3;++i) solved[i]=Multiply(changes[i],chain[i]);
        history.Finish(ref,solved);
        return target;
    };
    const Frame tracked{RotationMatrix(AxisRotation({0,0,1},1.2f)),{80,-150,90}};
    auto target=pose(1,1,false,tracked);
    Check(Near(target,tracked),"ordinary controller pose is unchanged");
    target=pose(2,1.01,true,native[2]);
    Check(Near(target,native[2]),"catch snaps to exact native contact");
    const auto catchBend=history.current.bend;
    target=pose(2,1.011,true,native[2]);
    Check(Near(history.current.bend,catchBend),"duplicate palette calls cannot advance elbow history");
    Vec lastBend=history.current.bend;
    for(int i=3;i<90;++i) {
        native[1].origin.x=i%2 ? .01f : -.01f;
        target=pose(i,1.+i*.01,true,native[2]);
        Check(Near(target,native[2]),"straight-arm jitter never moves the grip");
        Check(Dot(lastBend,history.current.bend)>.99f,"near-straight noisy elbows cannot flip");
        lastBend=history.current.bend;
    }
    const auto last=target;
    target=pose(90,1.9,false,tracked);
    Check(Near(target,last),"release begins at the last displayed grip");
    const Frame shifted{RotationMatrix(AxisRotation({0,1,0},.8f)),{700,-100,400}};
    const auto sameFrame=pose(90,1.901,false,tracked,shifted);
    Check(Near(sameFrame,Multiply(shifted,last)),"other eye/camera and snap yaw transport the same release pose");
    const auto middle=pose(99,1.99,false,tracked);
    Check(Near(middle.origin,Scale(Add(last.origin,tracked.origin),.5f)),"release reaches smooth midpoint");
    target=pose(108,2.08,false,tracked);
    Check(Near(target,tracked),"controller tracking is exact after 180ms");
    pose(109,2.09,true,native[2]);pose(110,2.10,false,tracked);
    target=pose(111,2.11,true,native[2]);
    Check(Near(target,native[2]) && !history.current.releasing,"regrab cancels release and restores exact contact");
    target=pose(112,3,false,tracked);
    Check(Near(target,tracked),"tracking gaps discard stale handoff");
    target=pose(113,2,false,tracked);
    Check(Near(target,tracked),"clock reversal discards stale history");
    history.Reset();Check(!history.current.valid,"view/level reset clears traversal history");
    // Recovering a clear bend must not pop after the near-straight band.
    native[1].origin.x=.01f;
    pose(120,4,true,native[2]);pose(121,4.01,true,native[2]);
    lastBend=history.current.bend;
    native[1].origin.x=-80;
    pose(122,4.02,true,native[2]);
    Check(Dot(lastBend,history.current.bend)>.99f,"leaving the singular band does not release a sudden elbow flip");
    // A native state can change between draws of one tracking sample.
    pose(122,4.021,false,tracked);
    Check(history.current.releasing,"same-sample release starts the handoff");
    target=pose(122,4.022,true,native[2]);
    Check(Near(target,native[2]) && !history.current.releasing,"same-sample regrab cancels the handoff");
}
static void ShoulderRecovery() {
    for(float hz:{30.f,72.f,90.f,120.f}) {
        TorsoPoseFilter filter;
        Frame original[15];Body(original,false,30);
        for(auto& joint:original) joint.origin=Scale(joint.origin,1.f/500);
        const Frame reference{IdentityBasis(),{}};
        Frame pose[15];for(int j=0;j<15;++j) pose[j]=original[j];
        Check(filter.Update(1,1,reference,reference,false,true,pose,{0,0,1}),"capture climbing torso");
        const Vec offset{.3f,-.1f,.1f};
        for(int frame=1;frame<=int(std::ceil(hz*.18f));++frame) {
            for(int j=0;j<15;++j) { pose[j]=original[j];pose[j].origin=Add(pose[j].origin,offset); }
            Check(filter.Update(1+frame,1+frame/double(hz),reference,reference,true,false,pose,{0,0,1}),
                "post-climb shoulder recovery solves");
        }
        const float remaining=Length(Sub(filter.cached[1].origin,Add(original[7].origin,offset)));
        Check(remaining>.02f && remaining<.2f,
            "original slower shoulder recovery is restored rather than the accelerated handoff");
    }
}
int main() {
    const auto start=std::chrono::steady_clock::now();
    for(int s=0;s<160;++s) {
        const bool expected=s==10 || s==19 || s==30 || s==31 || s==54 || s==55 ||
            (s>=75 && s<=79) || s==82 || s==83 || s==139;
        Check(tr::locomotion::IsTraversalGripState(s)==expected,"only traversal states own native grips");
        if(tr::locomotion::IsMonkeyBarState(s)) Check(!tr::locomotion::IsLedgeMountState(s),"monkey states never enable ledge mount");
    }
    BodyReach();Continuity();ShoulderRecovery();
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("Traversal IK: %d checks passed in %.3fs\n",checks,seconds);
}
