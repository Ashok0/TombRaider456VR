#include "FirstPersonBodyIK.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool b,const char* m) {++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
int main() {
 for(float fit:{80.f,100.f,120.f}) for(float scale:{.5f,1.f,1.5f}) for(float yaw:{-2.f,0.f,1.3f}) for(float side:{-1.f,1.f}) {
  const Frame body{RotationMatrix(AxisRotation({0,1,0},yaw)),{170,-90,240}};
  Frame arm[3]{};
  for(int i=0;i<3;++i)arm[i]=Multiply(body,{IdentityBasis(),{float(i==0?0:i==1?300:500)*scale,0,0}});
  for(Vec axis:{Vec{side,0,0},Vec{0,side,0},Vec{0,0,side},Vec{side*.6f,.8f,0}}) {
   const auto direction=Transform(body.basis,axis);Frame previous[3]{};
   for(int distance=0;distance<=2500;++distance) {
    const Frame target{Multiply(body.basis,RotationMatrix(AxisRotation({0,0,1},distance*.01f))),Add(arm[0].origin,Scale(direction,distance*scale))};
    Frame corrections[3]{},repeat[3]{};
    Check(SolveConnectedTrackedArm(arm,target,Transform(body.basis,{0,1,.2f}),arm,corrections,true,nullptr,fit),"connected solve succeeds at folded, reachable and overextended targets");
    Check(SolveConnectedTrackedArm(arm,target,Transform(body.basis,{0,1,.2f}),arm,repeat,true,nullptr,fit) && !std::memcmp(corrections,repeat,sizeof(repeat)),"stereo and repeated draws give identical connected arms");
    Frame solved[3]{};
    for(int i=0;i<3;++i) {
     solved[i]=Multiply(corrections[i],arm[i]);Check(Finite(solved[i]),"connected arm remains finite");
     for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
      float v=0;for(int k=0;k<3;++k)v+=corrections[i].basis.r[k][r]*corrections[i].basis.r[k][c];
      Check((i==2 ? std::fabs(v-(r==c?1.f:0.f))<.003f : std::isfinite(v)),"limb scale stays finite and hands never scale");
     }
     if(i<2)Check(Length(Sub(Transform(corrections[i],arm[i+1].origin),Multiply(corrections[i+1],arm[i+1]).origin))<.05f,"elbow and wrist seams remain connected");
    }
    Check(Length(Sub(solved[0].origin,arm[0].origin))<.001f,"shoulder never leaves torso");
    const float a=300*scale*fit/100,b=200*scale*fit/100,natural=a+b-std::min(a,b)*.001f;
    const float requested=distance*scale,begin=natural*1.04f,band=natural*.08f;
    const float radius=std::max(requested,std::fabs(a-b)+std::min(a,b)*.001f);
    Check(std::fabs(Length(Sub(solved[2].origin,arm[0].origin))-radius)<.05f,"far wrist follows controller with no outward limit");
    for(int i=0;i<2;++i) {
        const float fitted=i==0 ? a : b;
        const float actual=Length(Sub(solved[i+1].origin,solved[i].origin));
        Check(actual>=fitted-.05f && std::isfinite(actual),"arms extend as far as required with finite segment lengths");
        Vec bone{},width{};Unit(Sub(arm[i+1].origin,arm[i].origin),bone);Unit(Cross(bone,{0,1,0}),width);
        const auto across=Cross(bone,width);
        Check(std::fabs(Length(Transform(corrections[i].basis,width))-1)<.003f &&
              std::fabs(Length(Transform(corrections[i].basis,across))-1)<.003f,"axial extension preserves limb thickness in both perpendicular directions");
    }
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)Check(std::fabs(solved[2].basis.r[r][c]-target.basis.r[r][c])<.002f,"controller orientation remains exact at every reach");
    if(distance>1)Check(Length(Sub(solved[2].origin,previous[2].origin))<1.02f*scale,"wrist crosses reach boundaries without a position jump");
    for(int i=0;i<3;++i)previous[i]=solved[i];
   }
  }
 }
 std::printf("PASS: %d connected arm geometry checks\n",checks);
}
