#pragma once
#include "FirstPersonBodyIK.h"

namespace tr::firstperson {
// Conservative anatomical envelope in the chest joint's bind frame. It follows
// the outfit's skeleton and current body scale, rather than the animated arms
// or the pitched, tracked head. Y is down in the game's coordinate system.
struct TorsoEnvelope {
    Vec centre{},radius{};
    bool Capture(const Frame (&bind)[15]) {
        Frame inverse{};
        if(!Inverse(bind[7],inverse)) return false;
        const auto hip=Transform(inverse,bind[0].origin);
        const auto neck=Transform(inverse,bind[14].origin);
        const auto left=Transform(inverse,bind[11].origin),right=Transform(inverse,bind[8].origin);
        // Stop at the collar between shoulders and head pivot. The hidden
        // head itself must not trigger a permanent forward-facing bend.
        const auto collar=Scale(Add(neck,Scale(Add(left,right),.5f)),.5f);
        const float width=Length(Sub(right,left))*.5f,height=std::fabs(collar.y-hip.y)*.5f;
        if(!std::isfinite(width+height) || width<.001f || height<.001f) return false;
        centre=Scale(Add(hip,collar),.5f);
        radius={width*.9f,height,width*.6f};
        return true;
    }
    float Distance(const Frame& chest,Vec eye,float margin) const {
        Frame inverse{};if(!Inverse(chest,inverse)) return INFINITY;
        const auto p=Sub(Transform(inverse,eye),centre);
        float sum=0;
        const float point[3]={p.x,p.y,p.z},r[3]={radius.x,radius.y,radius.z};
        for(int col=0;col<3;++col) {
            const float scale=std::sqrt(chest.basis.r[0][col]*chest.basis.r[0][col]+
                chest.basis.r[1][col]*chest.basis.r[1][col]+chest.basis.r[2][col]*chest.basis.r[2][col]);
            if(scale<1.e-5f) return INFINITY;
            const float value=point[col]/(r[col]+margin/scale);sum+=value*value;
        }
        return sum;
    }
};

// Rotate the coherent upper body about the pelvis, preserving the entire lower
// body and spine length. The caller solves both arms AFTER this correction.
struct TorsoClearance {
    static constexpr float MaxAngle=.20943951f; // 12 degrees, no torso translation/stretch.
    const void* owner=nullptr;
    uint64_t serial=0;
    double time=0;
    float angle=0,units=0;
    bool valid=false;
    void Reset() { *this={}; }
    static Frame Bend(Vec hip,Vec right,float angle) {
        const auto rotation=RotationMatrix(AxisRotation(right,angle));
        return {rotation,Sub(hip,Transform(rotation,hip))};
    }
    static float Required(const TorsoEnvelope& shape,const Frame& chest,Vec hip,Vec right,Vec eye,float units) {
        const float margin=units*.05f; // Shared centre-eye clearance for both eyes.
        const float initial=shape.Distance(chest,eye,margin);
        if(!std::isfinite(initial) || initial>=1) return 0;
        float best=0,bestDistance=initial,solution=INFINITY;
        for(float sign:{1.f,-1.f}) {
            float previous=0;
            for(int step=1;step<=12;++step) {
                const float next=sign*MaxAngle*step/12;
                const float distance=shape.Distance(Multiply(Bend(hip,right,next),chest),eye,margin);
                if(distance>bestDistance) {bestDistance=distance;best=next;}
                if(distance>=1) {
                    float low=previous,high=next;
                    for(int k=0;k<12;++k) {
                        const float mid=(low+high)*.5f;
                        if(shape.Distance(Multiply(Bend(hip,right,mid),chest),eye,margin)>=1) high=mid;
                        else low=mid;
                    }
                    if(std::fabs(high)<std::fabs(solution)) solution=high;
                    break;
                }
                previous=next;
            }
        }
        // When tracking moves deep inside the body, retain the bound instead
        // of folding/stretching Lara to chase an unreachable camera position.
        return std::isfinite(solution) ? solution : best;
    }
    bool Apply(const void* model,Frame (&pose)[15],const Frame (&bind)[15],Vec eye,float heading,
               float worldUnits,uint64_t frame,double now,bool mainBody) {
        if(worldUnits<=1 || !std::isfinite(worldUnits+heading) || !std::isfinite(now)) return false;
        TorsoEnvelope shape;if(!shape.Capture(bind)) { if(mainBody) Reset();return true; }
        const Vec right{std::cos(heading),0,-std::sin(heading)};
        if(owner!=model || std::fabs(units-worldUnits)>.01f) Reset();
        if(mainBody && (!valid || serial!=frame)) {
            const float target=Required(shape,pose[7],pose[0].origin,right,eye,worldUnits);
            if(!valid) angle=target;
            else {
                const float dt=float(std::clamp(now-time,0.,.05));
                const float delta=(target-angle)*(1-std::exp(-12*dt));
                angle+=std::clamp(delta,-.78539816f*dt,.78539816f*dt); // At most 45 deg/s.
            }
            owner=model;units=worldUnits;time=now;serial=frame;valid=true;
        }
        if(!valid || std::fabs(angle)<1.e-6f) return true;
        const auto correction=Bend(pose[0].origin,right,angle);
        for(int j=7;j<14;++j) pose[j]=Multiply(correction,pose[j]);
        return true;
    }
};
}
