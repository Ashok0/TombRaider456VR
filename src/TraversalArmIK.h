// Render-only traversal arm continuity. Hands stay exact while gripping;
// only the release back to tracked wrists is blended.
#pragma once
#include "FirstPersonBodyIK.h"
#include <cstdint>

namespace tr::firstperson {
struct TraversalArmIK {
    struct Sample {
        bool valid=false,grip=false,releasing=false;
        double started=0;
        Frame wrist{},from{};
        Vec axis{},bend{};
    } seed{},current{};
    const void* owner=nullptr;
    uint64_t serial=0;
    double time=0;
    float elapsed=1.f/90.f;
    bool mode=false;
    void Reset() { *this={}; }
    // Reference is the same eye-relative frame for both eyes and attachments.
    // Re-evaluate from the previous frame, never from an earlier palette draw.
    bool Prepare(const void* item,uint64_t frame,double now,bool grip,const Frame& reference,
                 const Frame (&native)[3],const Frame (&chain)[3],Frame& target,Vec& pole) {
        Frame inverse{};
        if(!std::isfinite(now) || !Inverse(reference,inverse)) { Reset();return false; }
        if(owner!=item || now<time || now-time>.25) { Reset();owner=item;serial=frame;mode=grip;time=now; }
        else if(serial!=frame || mode!=grip) {
            elapsed=std::clamp(float(now-time),0.f,.05f);
            seed=current;serial=frame;mode=grip;time=now;
        }
        current=seed;
        if(grip) {
            if(!seed.valid || !seed.grip) current.started=time;
            current.releasing=false;
        } else if(seed.valid && seed.grip) {
            current.started=time;current.from=seed.wrist;current.releasing=true;
        }
        float release=1;
        if(!grip && current.releasing) {
            release=std::clamp(float((time-current.started)/.18),0.f,1.f);
            const float t=release*release*(3-2*release);
            const Frame from=Multiply(reference,current.from);
            target.origin=Add(Scale(from.origin,1-t),Scale(target.origin,t));
            target.basis=BlendRotation(from.basis,target.basis,t);
            if(release>=1) current.releasing=false;
        }
        Vec axis{};
        if(!Unit(Sub(target.origin,chain[0].origin),axis) &&
           !Unit(Sub(chain[2].origin,chain[0].origin),axis)) return false;
        const auto project=[&](Vec v) { return Sub(v,Scale(axis,Dot(v,axis))); };
        Vec fallback{};
        if(!Unit(project(pole),fallback))
            Unit(Cross(axis,std::fabs(axis.x)<.8f ? Vec{1,0,0} : Vec{0,1,0}),fallback);
        Vec carried=fallback;
        if(seed.valid) {
            const Vec oldAxis=Transform(reference.basis,seed.axis);
            const Vec oldBend=Transform(reference.basis,seed.bend);
            Unit(project(Transform(Align(oldAxis,axis),oldBend)),carried);
        }
        if(grip) {
            Vec nativeAxis{};
            Unit(Sub(native[2].origin,native[0].origin),nativeAxis);
            const Vec upper=Sub(native[1].origin,native[0].origin);
            const Vec authored=Sub(upper,Scale(nativeAxis,Dot(upper,nativeAxis)));
            const float length=Length(upper)+Length(Sub(native[2].origin,native[1].origin));
            const float confidence=length>.001f ? Length(authored)/length : 0;
            Vec desired=carried;
            if(confidence>.005f) {
                Vec bend{};Unit(Transform(Align(nativeAxis,axis),authored),bend);
                // Close to extension the authored pole is ill-conditioned.
                // Turn toward it at a bounded speed, rather than flipping sides.
                const float w=std::clamp((confidence-.005f)/.075f,0.f,1.f);
                Basis turn=BlendRotation(IdentityBasis(),Align(carried,bend),w*w*(3-2*w));
                // Keep this limit while leaving the ambiguous band too: the
                // recovered authored pole must not release its error as a pop.
                if(seed.valid) turn=LimitRotation(turn,10*elapsed);
                desired=Transform(turn,carried);
            }
            const float entry=std::clamp(float((time-current.started)/.12),0.f,1.f);
            // At catch, keep the preceding elbow side while snapping only the
            // wrist to its required contact. Authored motion takes over briefly.
            pole=seed.valid ? Transform(BlendRotation(IdentityBasis(),Align(carried,desired),entry*entry*(3-2*entry)),carried) : desired;
        } else if(current.releasing || release<1) {
            pole=Transform(BlendRotation(IdentityBasis(),Align(carried,fallback),release*release*(3-2*release)),carried);
        }
        current.valid=true;current.grip=grip;
        current.wrist=Multiply(inverse,target);
        current.axis=Transform(inverse.basis,axis);
        Vec prepared{};if(!Unit(project(pole),prepared)) prepared=fallback;
        current.bend=Transform(inverse.basis,prepared);
        return true;
    }
    void Finish(const Frame& reference,const Frame (&solved)[3]) {
        Frame inverse{};if(!Inverse(reference,inverse)) { Reset();return; }
        Vec axis{},bend{};
        Unit(Sub(solved[2].origin,solved[0].origin),axis);
        const Vec upper=Sub(solved[1].origin,solved[0].origin);
        const Vec projected=Sub(upper,Scale(axis,Dot(upper,axis)));
        const float length=Length(upper)+Length(Sub(solved[2].origin,solved[1].origin));
        if(Length(projected)<length*.005f || !Unit(projected,bend)) {
            // A fully straight arm has no measured elbow side. Preserve the
            // prepared pole instead of manufacturing a direction from noise.
            bend=Transform(reference.basis,current.bend);
        }
        current.axis=Transform(inverse.basis,axis);
        current.bend=Transform(inverse.basis,bend);
    }
};
}
