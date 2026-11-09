#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
struct TrackedTriangle {
    Vec axes[3]{};
    bool Capture(Vec neck,Vec left,Vec right) {
        const Vec mid=Scale(Add(left,right),.5f);
        return Unit(Sub(right,left),axes[0]) && Unit(Cross(axes[0],Sub(neck,mid)),axes[2]) && Unit(Cross(axes[2],axes[0]),axes[1]);
    }
    Vec RotateFrom(const TrackedTriangle& old,Vec v) const {
        Vec out{};for(int i=0;i<3;++i) out=Add(out,Scale(axes[i],Dot(old.axes[i],v)));return out;
    }
};
}
