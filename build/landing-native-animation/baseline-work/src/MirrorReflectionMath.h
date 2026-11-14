#pragma once
#include "StereoMath.h"

namespace tr::mirror {
struct RoomProfile {
    int room,originX,originZ;
    unsigned first,count;
    float planeX,top,bottom,glassZ[3][2];
};
// Audited glass only, excluding frames/sinks. Both RICH2 restrooms use three
// coplanar panes; room 135 subdivides its last pane into extra triangles.
inline constexpr RoomProfile Profiles[]={
    {134,32768,72704,3989463,36,33792,-24930,-24558,{{76157,76419},{77179,77438},{78204,78465}}},
    {135,29696,74752,4026003,48,30720,-24961,-24591,{{77183,77435},{78202,78465},{79230,79486}}},
};
// Native number_rooms and HD TRG record count are different. The former was
// verified in the running retail RICH2 scene; never gate it on the TRG count.
constexpr int NativeRooms=179, RenderRooms=180;
inline bool CapturePass(int pass,unsigned completed) {
    return pass>=0 && pass<3 && completed==((1u<<pass)-1);
}
inline void ObserverOffset(const Affine& eyeView,const float nativeCamera[3],float out[4],const RoomProfile& profile=Profiles[0]) {
    const auto eye=InvertRigid(eyeView);
    out[0]=2*profile.planeX-eye.r[0][3]-nativeCamera[0];
    out[1]=eye.r[1][3]-nativeCamera[1];
    out[2]=eye.r[2][3]-nativeCamera[2];out[3]=0;
}
inline const RoomProfile* FindProfile(int game,int level,int room,int rooms,int x,int z,
                    int chunks,unsigned first,unsigned count) {
    if(game!=1 || level!=12 || rooms!=NativeRooms || chunks!=1) return nullptr;
    for(const auto& p:Profiles)
        if(room==p.room && x==p.originX && z==p.originZ && first==p.first && count==p.count) return &p;
    return nullptr;
}
inline bool Profile(int game,int level,int room,int rooms,int x,int z,int chunks,unsigned first,unsigned count) {
    return FindProfile(game,level,room,rooms,x,z,chunks,first,count)!=nullptr;
}
inline Affine Reflection(const RoomProfile& profile=Profiles[0]) {
    Affine r=Affine::Identity();r.r[0][0]=-1;r.r[0][3]=2*profile.planeX;return r;
}
inline Affine WorldView(const float rot[3][3],const float pos[3]) {
    Affine v{};
    for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
        v.r[r][c]=rot[r][c]*(r==2 ? -1.f : 1.f);
        v.r[r][3]-=v.r[r][c]*pos[c];
    }
    return v;
}
inline Affine CameraReflection(const Affine& worldView,const RoomProfile& profile=Profiles[0]) {
    return Mul(Mul(worldView,Reflection(profile)),InvertRigid(worldView));
}
inline bool InFront(const Affine& eyeView,const RoomProfile& profile=Profiles[0]) {
    const auto eye=InvertRigid(eyeView);
    const float x=eye.r[0][3],y=eye.r[1][3],z=eye.r[2][3];
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
        x>profile.planeX+8 && x<profile.planeX+4096 && y>profile.top-1024 && y<profile.bottom+1024 &&
        z>profile.glassZ[0][0]-1024 && z<profile.glassZ[2][1]+1024;
}
} // namespace tr::mirror
