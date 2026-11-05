bool g_weaponCollisionReady=false;
weaponcollision::Cache g_weaponCollision[2];
void ApplyWeaponCollision(int hand,motiongun::Frame& wrist) {
    using namespace motiongun;
    if(hand<0 || hand>1) return;
    if(!g_weaponCollisionReady || !Cfg().firstPersonWeaponCollision || !MotionReady() ||
       *Ptr<int16_t>(g_boundDll->lara+2)!=4 || !g_boundDll->getFloor || !WeaponCollisionLOS() ||
       (hand==0 && !WeaponCollisionDual(CurrentMotionWeapon()))) { g_weaponCollision[hand]={};return; }
    const auto* item=*Ptr<uint8_t*>(g_boundDll->laraItem);
    const float units=LiveWorldUnitsPerMetre();
    float right=0,forward=0;VR().FirstPersonViewOffset(right,forward);
    const auto horizontal=locomotion::Rotate({right,forward},WeaponCollisionHeading());
    const Vec eye{g_scenePose.x_pos+horizontal.x*units,
        g_scenePose.y_pos+VR().FirstPersonVerticalOffset()*units,g_scenePose.z_pos+horizontal.z*units};
    const int weapon=CurrentMotionWeapon();
    const auto tip=motiongun::MuzzleLocal(weapon,hand,WeaponCollisionGame());
    using Floor=void* (__cdecl*)(int32_t,int32_t,int32_t,int16_t*);
    using LOS=int32_t (__cdecl*)(ShotVector*,ShotVector*);
    const auto trace=[&](Vec from,Vec to,Vec& hit) {
        for(float value:{from.x,from.y,from.z,to.x,to.y,to.z})
            if(!std::isfinite(value) || std::fabs(value)>1.e7f) return false;
        ShotVector start{int32_t(std::lround(from.x)),int32_t(std::lround(from.y)),int32_t(std::lround(from.z)),
            WeaponCollisionRoom(item),0};
        reinterpret_cast<Floor>(g_boundBase+g_boundDll->getFloor)(start.x,start.y,start.z,&start.room);
        ShotVector end{int32_t(std::lround(to.x)),int32_t(std::lround(to.y)),int32_t(std::lround(to.z)),start.room,0};
        const bool blocked=!reinterpret_cast<LOS>(g_boundBase+WeaponCollisionLOS())(&start,&end);
        hit={float(end.x),float(end.y),float(end.z)};return blocked;
    };
    const Vec shift=g_weaponCollision[hand].Update(VR().PoseSerial(),g_boundBase,item,WeaponCollisionLevel(),
        weapon,units,eye,wrist,tip,trace);
    wrist.origin=Add(wrist.origin,shift);
}
