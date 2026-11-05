void TestWeaponCollisionMath() {
    using namespace tr;using namespace tr::motiongun;
    for(float units:{423.f,1000.f}) for(float yaw:{0.f,.4f,1.57f,3.14159265f})
    for(float spacing:{0.f,.01f,.04f,.059f,.08f,.5f}) {
        const Frame left{firstperson::IdentityBasis(),{0,0,0}};
        const Frame right{firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},yaw)),{units*spacing,0,0}};
        const motiongun::Vec tip{0,units*.3f,0};
        const auto shift=weaponcollision::LeftOffset(left,right,tip,tip,units);
        motiongun::Vec p{},q{};weaponcollision::Closest(Add(left.origin,shift),Add(Transform(left,tip),shift),
            Sub(right.origin,shift),Sub(Transform(right,tip),shift),p,q);
        Check(firstperson::Length(Sub(p,q))>=units*.06f-.02f,"parallel, crossing and coincident gun capsules separate");
        Check(firstperson::Length(shift)<=units*.03001f,"each hand moves at most three centimeters");
        Check(weaponcollision::Finite(shift),"gun-pair correction remains finite");
        const auto repeat=weaponcollision::LeftOffset(left,right,tip,tip,units);
        Check(!std::memcmp(&repeat,&shift,sizeof(shift)),"stereo correction is deterministic");
    }
    const Frame a{firstperson::IdentityBasis(),{}},b{firstperson::IdentityBasis(),{1000,0,0}};
    Check(firstperson::Length(weaponcollision::LeftOffset(a,b,{0,100,0},{0,100,0},1000))==0,
          "separated guns retain exact tracking");
    Check(firstperson::Length(weaponcollision::LeftOffset(a,b,{0,100,0},{0,100,0},NAN))==0,
          "invalid scale cannot move guns");
}
template<class Render>
void TestWeaponCollisionRender(const Render& render,tr::Config& settings) {
    using namespace tr;using namespace tr::motiongun;
    TestWeaponCollisionMath();
    const bool oldOption=settings.firstPersonWeaponCollision;
    auto& weapon=*Ptr<int16_t>(dll.lara+4);auto& status=*Ptr<int16_t>(dll.lara+2);
    const auto oldWeapon=weapon,oldStatus=status;weapon=1;status=4;
    const auto oldLeft=testControllerOffset[0];testControllerOffset[0]=testControllerOffset[1];
    settings.firstPersonWeaponCollision=true;
    Frame raw[2]{},shown[2]{};Basis controller{};
    for(int hand=0;hand<2;++hand) Check(TrackedControllerWrist(hand,raw[hand],controller,nullptr),"dual raw wrists available");
    const auto scene=g_scenePose;
    const auto pose=render();
    for(int hand:{1,0}) Check(BuildControllerWrist(hand,shown[hand],controller),"dual corrected wrists available in either order");
    const motiongun::Vec shift=Sub(shown[0].origin,raw[0].origin);
    Check(firstperson::Length(shift)>1,"overlapping tracked guns move apart");
    Check(firstperson::Length(Add(shift,Sub(shown[1].origin,raw[1].origin)))<.01f,"both guns share separation equally");
    const auto camera=BodyPaletteCameraOrigin(item,pose[14]);
    for(int hand=0;hand<2;++hand) {
        Check(firstperson::Length(Sub(Add(pose[hand ? 10 : 13].origin,camera),shown[hand].origin))<.1f,
              "rendered arm follows separated weapon");
        GunPose gun{};Check(BuildGunPose(hand,gun),"separated gun pose remains available for firing");
        Check(firstperson::Length(Sub(gun.muzzle,Transform(shown[hand],MuzzleLocal(1,hand,WeaponCollisionGame()))))<.1f,
              "firing muzzle follows separated gun");
        Check(!std::memcmp(&shown[hand].basis,&raw[hand].basis,sizeof(Basis)),"collision preserves aim direction");
        Frame repeat{};BuildControllerWrist(hand,repeat,controller);
        Check(firstperson::Length(Sub(repeat.origin,shown[hand].origin))<.001f,"repeated eyes do not accumulate displacement");
    }
    Check(!std::memcmp(&scene,&g_scenePose,sizeof(scene)),"gun collision cannot move the view");
    for(int guard=0;guard<3;++guard) {
        settings.firstPersonWeaponCollision=guard!=0;status=guard==1 ? 2 : 4;weapon=guard==2 ? 4 : 1;
        Frame tracked{},shownGuard{};TrackedControllerWrist(1,tracked,controller,nullptr);BuildControllerWrist(1,shownGuard,controller);
        Check(firstperson::Length(Sub(tracked.origin,shownGuard.origin))<.001f,"disabled, holstering and single guns retain tracking");
    }
    testControllerOffset[0]=oldLeft;
    weapon=oldWeapon;status=oldStatus;settings.firstPersonWeaponCollision=oldOption;
}
