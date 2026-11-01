// Offline replay of the user's read-only controller capture; no game calls.
#include <cstdio>
#include "FlareThrowMotion.h"
int main(int argc,char** argv){
    if(argc!=2)return 2;FILE* file=nullptr;fopen_s(&file,argv[1],"r");if(!file)return 3;
    tr::flarethrow::Gesture gesture;tr::motiongun::Vec p{},h{},f{},v{};double t=0;unsigned long long serial=0;int n=0,releases=0;
    while(std::fscanf(file,"%lf,%llu,%f,%f,%f,%f,%f,%f,%f,%f,%f",&t,&serial,&p.x,&p.y,&p.z,&h.x,&h.y,&h.z,&f.x,&f.y,&f.z)==11){
        ++n;if(gesture.Update(true,serial,t,p,h,f,v)==tr::flarethrow::Event::Release){++releases;std::printf("release %.3fs velocity=(%.2f,%.2f,%.2f)\n",t,v.x,v.y,v.z);}
    }
    fclose(file);std::printf("replay: %d frames, %d throws\n",n,releases);
}
