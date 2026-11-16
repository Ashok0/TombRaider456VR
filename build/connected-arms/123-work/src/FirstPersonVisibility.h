#pragma once
#include <cstdint>
#include <cmath>
#include "LocomotionMath.h"

namespace tr::firstperson {
constexpr uint32_t HeadMeshBit = 1u << 14;
constexpr uint32_t ArmMeshBits = 0x3f00u;
constexpr uint32_t HandMeshBits = 0x2400u;

inline bool TraversalTransitionHandsOnly(int state,int game,uint32_t recoveryMask) {
    // Reveal arms on the first upright frame, including immediate movement.
    // Torso/camera/wrist recovery can continue without hiding the arms.
    if(locomotion::IsGroundLocomotionState(state,game) || state==15 || state==28) return false;
    return locomotion::IsLedgeMountState(state,game) ||
        (!locomotion::IsLedgeArmsOnlyState(state,game) && recoveryMask==ArmMeshBits);
}

// Native simulation and stereo draws can cross a mount/landing boundary
// before the torso palette settles. Hold its restriction through recovery.
struct BodyVisibilityRecovery {
    const void* owner=nullptr;
    int level=-1;
    uint32_t mask=UINT32_MAX;
    double until=0,last=0;
    uint32_t Update(const void* item,int currentLevel,double now,bool armsOnly,
                    bool landing,bool recovering,bool freeJump=false) {
        if(owner!=item || level!=currentLevel || now<last) *this={};
        owner=item;level=currentLevel;last=now;
        // A new ordinary flight supersedes the previous mount/landing mask.
        // Letting that timer expire in midair makes the torso blink back on.
        if(freeJump && !armsOnly && !landing) { mask=UINT32_MAX;until=0; }
        if(armsOnly || landing) {
            mask=armsOnly ? ArmMeshBits : ~(1u<<7);
            until=now+.35;
        } else if(now>=until && !recovering) mask=UINT32_MAX;
        return mask;
    }
};

inline bool HideDirectionalJumpBody(int state) {
    return state==25 || state==26 || state==27;
}

inline bool UseHeadCamera(int hitPoints) { return hitPoints > 0; }

inline bool QuestItemUsesThirdPerson(int game,int state) {
    // Native USEKEY/USEPUZZLE share these state IDs in TR1, TR2 and TR3.
    // TR4/5's custom socket state 89 is a crawling state in TR3.
    return game>=1 && game<=3 && (state==42 || state==43);
}

// Unarmed ground-movement arms are revealed only by looking down. Separate
// show/hide thresholds avoid flickering at the edge of a downward glance.
struct UnarmedArmVisibility {
    bool lookingDown=false;
    bool Hide(bool eligible,float pitch) {
        if (!eligible) { lookingDown=false; return false; }
        if (!std::isfinite(pitch)) { lookingDown=false; return true; }
        constexpr float radians=3.14159265358979323846f/180.f;
        if (pitch<=-15.f*radians) lookingDown=true;
        else if (pitch>=-10.f*radians) lookingDown=false;
        return !lookingDown;
    }
};

// HD palette slots are mapped to classic mesh bits by each outfit.
inline uint32_t SkinJointMask(uint32_t meshBits, const int32_t* mapping, int count) {
    uint32_t result=0;
    for (int i=0;i<count && i<32;++i) {
        const int mesh=mapping ? mapping[i] : i;
        if (mesh>=0 && mesh<32 && (meshBits&(uint32_t(1)<<mesh)))
            result|=uint32_t(1)<<i;
    }
    return result;
}

// A hidden head must not pull shared collar vertices toward the tracked eye
// while the fitted/filtered torso rises from a crouch. Skin matrices already
// share mesh coordinates: use the final chest transform for hidden head weights.
// Only the eye-view upload is changed; keep native physics, camera and shadow
// palettes, visible heads and arms-only attachments untouched.
inline bool StabilizeHiddenNeck(float* palette,int count,const int32_t* mapping,uint64_t visible) {
    if(!palette || count<1 || count>32) return false;
    const auto head=SkinJointMask(HeadMeshBit,mapping,count);
    const auto chest=SkinJointMask(1u<<7,mapping,count)&visible;
    if(!head || (head&visible) || !chest) return false;
    int source=0;while(!(chest&(uint64_t(1)<<source))) ++source;
    for(int k=0;k<12;++k) if(!std::isfinite(palette[source*12+k])) return false;
    for(int i=0;i<count;++i) if(head&(uint64_t(1)<<i))
        for(int k=0;k<12;++k) palette[i*12+k]=palette[source*12+k];
    return true;
}

// DrawLaraHD passes zero for an unmasked body. Its effective native mask is
// then all meshes, regardless of the persistent ITEM_INFO::mesh_bits value.
// Restrict that draw locally so a stale item mask cannot remove hanging hands.
inline uint32_t HdDrawMeshBits(uint32_t itemBits, bool nativeMaskedPass,
                               bool ledgeArmsOnly, bool hideArms=false, bool hideHead=true) {
    return (nativeMaskedPass ? itemBits : UINT32_MAX) &
           (ledgeArmsOnly ? ArmMeshBits :
            ~(hideHead ? HeadMeshBit : 0u) & ~(hideArms ? ArmMeshBits : 0u));
}
} // namespace tr::firstperson
