#pragma once
#include "MotionGunMath.h"
#include <cstdint>

namespace tr::keypad {
// PUZZLE_HOLE8_1: front is -Y; columns run from +X to -X and rows
// run along +Z. These are the twelve key faces, not the panel's bounds.
constexpr float ColumnLeft[]={14.63f,-13.94f,-42.52f};
constexpr float ColumnRight[]={42.46f,13.89f,-14.69f};
constexpr float RowTop[]={-55.31f,-25.79f,3.88f,33.44f};
constexpr float RowBottom[]={-26.35f,3.17f,32.84f,62.40f};
constexpr float Face=-26.f;
constexpr int Clear=9,Zero=10,Enter=11;
struct PressLight {
    int key=-1;
    uint64_t time=0;
    float Strength(uint64_t now) const {
        if(key<0 || key>=12 || now<time || now-time>=350) return 0;
        return now-time<=150 ? 1.f : float(350-(now-time))/200.f;
    }
};
inline bool Finite(motiongun::Vec p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
inline int Touch(motiongun::Vec previous,motiongun::Vec current,float scale) {
    if(!Finite(previous)||!Finite(current)||!std::isfinite(scale)||scale<=1) return -1;
    const auto d=motiongun::Sub(current,previous);
    const float plane=Face-scale*.006f;
    if(motiongun::Dot(d,d)>scale*scale*.35f*.35f ||
       previous.y>=plane || current.y<plane || current.y>Face+scale*.08f) return -1;
    const float t=(plane-previous.y)/d.y;
    const float x=previous.x+d.x*t,z=previous.z+d.z*t;
    for(int row=0;row<4;++row) for(int col=0;col<3;++col)
        if(x>=ColumnLeft[col] && x<=ColumnRight[col] && z>=RowTop[row] && z<=RowBottom[row]) return row*3+col;
    return -1;
}
struct Entry {
    uint8_t count=0,x=0,y=0,pause=0,digits[4]{};
    uint32_t mesh=0x1fff;
    int Code() const {
        if(count!=4) return -1;
        int code=0;for(auto d:digits) { if(d>9) return -1;code=code*10+d; }return code;
    }
    bool Birthday() const {
        return (count==3 && digits[0]==1 && digits[1]==4 && digits[2]==2) ||
            (count==4 && Code()==1402);
    }
};
}
