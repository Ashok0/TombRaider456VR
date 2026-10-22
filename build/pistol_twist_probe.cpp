#include "../src/FirstPersonIK.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){
 auto id=IdentityBasis();Frame bind[3]={ {id,{-180,-700,0}},{id,{-240,-460,30}},{id,{-180,-250,70}}};Frame rest{id,{0,-650,-60}};
 Frame target{Multiply(RotationMatrix(AxisRotation({0,0,1},-3)),GunBasis(id)),{-180,-700,50}};
 for(int n=130;n<=150;++n){
 Frame torso{RotationMatrix(AxisRotation({0,1,0},n*.001f)),rest.origin},chain[3]{},c[3]{};
 PistolArmReference(torso,rest,bind,1,chain);SolveArm(chain,target,{-.6f,1,-.25f},c,nullptr,nullptr,false,{},true);
 auto lower=Multiply(c[1],chain[1]);Vec axis{};Unit(Sub(target.origin,lower.origin),axis);
 auto rel=RotationProduct(RotationInverse(RotationOf(bind[2].basis)),RotationOf(bind[1].basis));
 auto desired=RotationProduct(RotationOf(target.basis),rel);
 auto d=RotationProduct(desired,RotationInverse(RotationOf(lower.basis)));
 printf("%d: twist confidence=%.6f angle=%.3f\n",n,std::hypot(d.w,Dot(d.v,axis)),2*std::atan2(Dot(d.v,axis),d.w));
 }
}
