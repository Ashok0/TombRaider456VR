void ApplyWeaponCollision(int hand,motiongun::Frame& wrist) {
    using namespace motiongun;
    if(hand<0 || hand>1 || !Cfg().firstPersonWeaponCollision || !MotionReady() ||
       *Ptr<int16_t>(g_boundDll->lara+2)!=4 || !WeaponCollisionDual(CurrentMotionWeapon())) return;
    // Read the other physical wrist directly: never recurse through the corrected
    // wrist builder or feed one hand's correction back into the other hand.
    Frame other{};Basis controller{};
    if(!TrackedControllerWrist(1-hand,other,controller,nullptr)) return;
    const int weapon=CurrentMotionWeapon(),game=WeaponCollisionGame();
    const auto shift=weaponcollision::LeftOffset(hand==0 ? wrist : other,hand==1 ? wrist : other,
        MuzzleLocal(weapon,0,game),MuzzleLocal(weapon,1,game),LiveWorldUnitsPerMetre());
    wrist.origin=Add(wrist.origin,Scale(shift,hand==0 ? 1.f : -1.f));
}
