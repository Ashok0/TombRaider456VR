#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
int main(){for(float yaw:{-1.2f,1.2f})for(int d=0;d<4;++d){ControllerBodyAnchor a;Vec eye{0,1.7f,0},l{-.2f,1,.3f},r{.2f,1,.3f};a.Update(1,0,eye,l,r,0,0,true,false,yaw);a.Update(2,.2,eye,l,r,0,0,true,false,yaw);for(int i=1;i<=20;++i){Vec n=YawVector({i/20.f,0,0},d*1.5707963f);a.Update(i+2,.2+i/90.,Add(eye,Scale(n,.03f)),Add(l,Scale(n,.041f)),Add(r,Scale(n,.010f)),0,0,true,false,yaw);}printf("yaw%.1f dir%d moved%.2fmm held%d\n",yaw,d,Length(Flat(Sub(a.head,eye)))*1000,a.held);}}
