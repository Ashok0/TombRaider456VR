#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){float biggest=0;int bad=0;
for(float reach:{.15f,.4f,.65f})for(float handYaw:{0.f,.01f,.03f})for(float drift:{.005f,.012f,.025f})for(float lean:{.02f,.05f,.10f})for(float speed:{.015f,.03f,.06f})for(float phase:{0.f,1.f,2.f}){
 ControllerBodyAnchor a;float peak=0;int first=0;
 for(int i=0;i<900;++i){float t=std::max(0,i-90)*speed,hy=.8f*std::sin(t),s=std::sin(t+phase)-std::sin(phase),ay=handYaw*(std::sin(t*.7f+phase)-std::sin(phase));
 Vec eye=Add(Vec{lean*std::sin(t),1.7f,-.15f},YawVector(Vec{0,0,.15f},hy));
 Vec l=Add(Vec{drift*s,0,0},YawVector(Vec{-.2f,1.3f,reach},ay)),r=Add(Vec{drift*s,0,0},YawVector(Vec{.2f,1.3f,reach},ay));
 a.Update(i+1,i/90.,eye,l,r,0,0,true,false,hy);
 float d=Length(Flat(a.head));if(d>peak){peak=d;if(!first&&d>.001f)first=i;}
 }
 if(peak>.001f){++bad;{biggest=std::max(biggest,peak);printf("reach %.2f handYaw %.3f drift %.3f lean %.2f speed %.3f phase %.1f peak %.1fmm first %d\n",reach,handYaw,drift,lean,speed,phase,peak*1000,first);}}
}printf("bad %d max %.1fmm\n",bad,biggest*1000);}
