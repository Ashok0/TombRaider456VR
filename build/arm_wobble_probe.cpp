#include "../src/FirstPersonIK.h"
#include <cstdio>
using namespace tr::firstperson;
int main() {
 auto id=IdentityBasis(); Frame bind[3]={ {id,{0,0,0}}, {id,{0,300,0}}, {id,{0,300,200}}};
 Vec spin{};Unit(bind[2].origin,spin);
 for(float targetTurn:{0.f,1.f,3.14159265359f}) {
  Frame target{RotationMatrix(AxisRotation({0,1,0},targetTurn)),bind[2].origin}; Frame first[3]{};float error=0;
  for(int step=0;step<360;++step) {
   Frame native[3]{};auto rotation=RotationMatrix(AxisRotation(spin,step*.01745329252f));
   for(int i=0;i<3;++i)native[i]={Multiply(rotation,bind[i].basis),Transform(rotation,bind[i].origin)};
   Frame changes[3]{};SolveTrackedArm(native,target,{0,1,0},bind,changes);
   for(int i=0;i<2;++i){auto actual=Multiply(changes[i],native[i]);if(!step)first[i]=actual;for(int r=0;r<3;++r)for(int c=0;c<3;++c)error=std::max(error,std::fabs(actual.basis.r[r][c]-first[i].basis.r[r][c]));}
  }
  printf("target turn %.3f: fixed-controller orientation error %f\n",targetTurn,error);
 }
}
