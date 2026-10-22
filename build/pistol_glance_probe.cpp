#include "../src/FirstPersonIK.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){
 const auto id=IdentityBasis(); float worst=0; int wh=0,wr=0,wyh=0,wrch=0,wj=0; float wy=0;
 for(int hand=0;hand<2;++hand) for(int roll=-32;roll<=32;++roll) for(int height=-750;height<=-200;height+=50) for(int reach=50;reach<=450;reach+=100){
  float s=hand?1.f:-1.f;
  Frame bind[3]={{id,{s*180,-700,0}},{id,{s*240,-460,30}},{id,{s*180,-250,70}}};
  Frame rest{id,{0,-650,-60}};
  Frame target{Multiply(RotationMatrix(AxisRotation({0,0,1},roll*.1f)),GunBasis(id)),{s*180,float(height),float(reach)}};
  Frame prev[3]{}; ArmRollContinuity continuity{};
  for(int step=-350;step<=350;++step){
   Frame torso{RotationMatrix(AxisRotation({0,1,0},step*.001f)),rest.origin},chain[3]{},delta[3]{};
   PistolArmReference(torso,rest,bind,1,chain);
   SolveTrackedArm(chain,target,{s*.6f,1,-.25f},bind,delta,true,&continuity);
   for(int j=0;j<2;++j){auto pose=Multiply(delta[j],chain[j]);
    auto q=RotationProduct(RotationOf(pose.basis),RotationInverse(RotationOf(prev[j].basis)));
    float a=2*std::atan2(std::sqrt(Dot(q.v,q.v)),std::fabs(q.w))*57.29578f;
    if(step>-350 && a>worst){worst=a;wh=hand;wr=roll;wy=step*.001f;wyh=height;wrch=reach;wj=j;}
    prev[j]=pose;
   }
  }
 }
 printf("max arm rotation for 0.057deg torso change: %.3fdeg hand=%d controllerRoll=%.2f torsoYaw=%.3f height=%d reach=%d segment=%d\n",worst,wh,wr*.1f,wy,wyh,wrch,wj);
}
