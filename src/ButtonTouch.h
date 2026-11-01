#pragma once
#include "MotionGunMath.h"
#include <cstring>

namespace tr::firstperson {
struct ButtonSurface { float left,right,top,bottom,z; };
struct ButtonProfile {
    int game,joints,joint;
    bool sequence;
    const char* name;
    ButtonSurface face;
};
// Press faces, not whole-object bounds. Canonical mesh space, before the
// native joint and inverse-bind transforms. Z decreases toward the user.
constexpr ButtonProfile ButtonProfiles[]={
    {0,1,0,false,"SWITCH_TYPE1_JOBY1A",{-90,89,-90,90,-25}},
    {0,1,0,false,"SWITCH_TYPE1_LOWSTRT",{-65,65,-65,65,-44}},
    {0,1,0,false,"SWITCH_TYPE1_CSPLIT1",{-340,340,-340,340,-50}},
    {0,1,0,false,"SWITCH_TYPE2_JOBY3B",{-90,89,-90,90,-20}},
    {0,1,0,false,"SWITCH_TYPE2_KARNAK1",{-90,89,-90,90,-23}},
    {0,1,0,false,"SWITCH_TYPE2_PALACES",{-340,340,-340,340,-17}},
    {0,1,0,true,"SEQUENCE_SWITCH1",{-340,340,-340,340,0}},
    {0,1,0,true,"SEQUENCE_SWITCH2",{-340,340,-340,340,0}},
    {0,1,0,true,"SEQUENCE_SWITCH3",{-340,340,-340,340,0}},
    {1,1,0,false,"SWITCH_TYPE1_ANDREA3",{-90,90,-90,90,-20}},
    {1,1,0,false,"SWITCH_TYPE1_ANDY3",{-105,105,-85,85,-25}},
    {1,1,0,false,"SWITCH_TYPE1_JOBY2",{-90,89,-90,90,-32}},
    {1,2,1,false,"SWITCH_TYPE2_RICH2",{-43,43,-31,31,-33}},
    {1,2,1,false,"SWITCH_TYPE2_RICH3_RICH2_RICH1",{-43,43,-31,31,-33}},
    {1,2,1,false,"SWITCH_TYPE3_RICH3",{-21,21,-42,42,-34}},
    {1,2,1,false,"SWITCH_TYPE4_RICH3_RICH2_RICH1",{-21,21,-42,42,-34}},
    {1,2,1,false,"SWITCH_TYPE5_RICH2_RICH1",{-21,21,-42,42,-34}},
    {1,2,1,false,"SWITCH_TYPE6_RICH3",{-43,43,-30,32,-34}},
    {1,1,0,true,"SEQUENCE_SWITCH1",{-340,340,-340,340,0}},
    {1,1,0,true,"SEQUENCE_SWITCH2",{-340,340,-340,340,0}},
    {1,1,0,true,"SEQUENCE_SWITCH3",{-340,340,-340,340,0}},
};
inline const ButtonProfile* FindButtonProfile(int game,const char* name,int joints) {
    if(!name) return nullptr;
    for(const auto& p:ButtonProfiles) if(p.game==game && p.joints==joints && !std::strcmp(p.name,name)) return &p;
    return nullptr;
}
inline bool ButtonTouch(ButtonSurface face,motiongun::Vec previous,motiongun::Vec current,float scale) {
    const auto finite=[](motiongun::Vec p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    if(!finite(previous) || !finite(current) || !std::isfinite(scale) || scale<=1) return false;
    const auto delta=motiongun::Sub(current,previous);
    if(motiongun::Dot(delta,delta)>scale*scale*.4f*.4f) return false;
    const float plane=face.z-scale*.025f;
    if(previous.z>=plane || current.z<plane || current.z>plane+scale*.12f) return false;
    const float t=(plane-previous.z)/(current.z-previous.z);
    const float x=previous.x+delta.x*t,y=previous.y+delta.y*t;
    return x>=face.left && x<=face.right && y>=face.top && y<=face.bottom;
}
}
