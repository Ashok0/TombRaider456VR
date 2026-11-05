void TestWeaponCollisionMath() {
    using namespace tr::motiongun;
    for(float units:{423.f,1000.f}) for(int gun=1;gun<=8;++gun) for(int hand:{0,1})
    for(float yaw:{-1.f,0.f,1.4f}) for(bool corner:{false,true}) {
        const auto basis=tr::firstperson::RotationMatrix(tr::firstperson::AxisRotation({0,1,0},yaw));
        const Vec normal=Transform(basis,{0,0,1}),side=Transform(basis,{1,0,0});
        const Frame wrist{basis,Transform(basis,{units*.2f,units*.2f,units*.3f})};
        const Vec tip=MuzzleLocal(gun,hand,0);const float limit=units*.18f;
        int queries=0;
        const auto trace=[&](Vec from,Vec to,Vec& hit) {
            ++queries;float fraction=1;
            for(auto n:{normal,corner?side:normal}) {
                const float a=Dot(n,from),b=Dot(n,to);
                if(b>limit && b>a) fraction=std::min(fraction,(limit-a)/(b-a));
            }
            hit=Add(from,Scale(Sub(to,from),fraction));return fraction<1;
        };
        const Vec offset=tr::weaponcollision::Retract({},wrist,tip,units,trace);
        Check(tr::firstperson::Length(offset)>1 && queries<=54,"weapon contact retracts with a bounded number of collision queries");
        const float r=units*.05f;
        for(auto point:{Vec{},tip,Add(tip,{r,0,0}),Add(tip,{-r,0,0}),Add(tip,{0,0,r}),Add(tip,{0,0,-r})}) {
            const auto shown=Add(Transform(wrist,point),offset);
            Check(Dot(normal,shown)<=limit+.1f && (!corner || Dot(side,shown)<=limit+.1f),
                  "barrel and hand samples remain in front of walls and corners");
        }
        tr::weaponcollision::Cache cache;
        const auto first=cache.Update(10,1,&cache,1,gun,units,{},wrist,tip,trace);const int before=queries;
        const auto second=cache.Update(10,1,&cache,1,gun,units,{},wrist,tip,trace);
        Check(queries==before && !std::memcmp(&first,&second,sizeof(first)),"stereo, arm and muzzle queries reuse one collision result");
        cache.Update(11,1,&cache,1,gun,units,{},wrist,tip,trace);
        Check(queries>before,"fresh tracking samples recheck geometry");
        const auto clear=[](Vec,Vec,Vec&) {return false;};
        Check(tr::firstperson::Length(tr::weaponcollision::Retract({},wrist,tip,units,clear))==0,"free space retains exact controller pose");
        const auto invalid=[](Vec,Vec,Vec& hit) {hit={NAN,0,0};return true;};
        Check(tr::firstperson::Length(tr::weaponcollision::Retract({},wrist,tip,units,invalid))==0,"invalid collision results cannot displace weapons");
    }
}
tr::motiongun::Vec weaponTestNormal{};
float weaponTestLimit=0;
int weaponTestQueries=0;
void* __cdecl WeaponTestFloor(int32_t,int32_t,int32_t,int16_t*) { return nullptr; }
int32_t __cdecl WeaponTestLOS(tr::ShotVector* from,tr::ShotVector* to) {
    using namespace tr::motiongun;++weaponTestQueries;
    const Vec a{float(from->x),float(from->y),float(from->z)},b{float(to->x),float(to->y),float(to->z)};
    const float start=Dot(weaponTestNormal,a),end=Dot(weaponTestNormal,b);
    if(end<=weaponTestLimit || end<=start) return 1;
    const auto hit=Add(a,Scale(Sub(b,a),(weaponTestLimit-start)/(end-start)));
    to->x=int32_t(std::lround(hit.x));to->y=int32_t(std::lround(hit.y));to->z=int32_t(std::lround(hit.z));return 0;
}
template<class Render>
void TestWeaponCollisionRender(const Render& render,tr::Config& settings) {
    using namespace tr;using namespace tr::motiongun;
    TestWeaponCollisionMath();
    const auto oldFloor=dll.getFloor;const bool oldReady=g_weaponCollisionReady,oldOption=settings.firstPersonWeaponCollision;
    const auto rva=[](const void* p){return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    const auto* oldMotion=g_motionDll;auto collisionMotion=*g_motionDll;
    collisionMotion.lineOfSight=rva(reinterpret_cast<void*>(&WeaponTestLOS));g_motionDll=&collisionMotion;
    dll.getFloor=rva(reinterpret_cast<void*>(&WeaponTestFloor));
    auto& weapon=*Ptr<int16_t>(dll.lara+4);auto& status=*Ptr<int16_t>(dll.lara+2);
    const auto oldWeapon=weapon,oldStatus=status;weapon=1;status=4;
    g_weaponCollisionReady=true;settings.firstPersonWeaponCollision=true;
    Check(MotionReady(),"weapon collision render fixture has ready tracked pistols");
    Frame raw{};Basis controller{};Check(TrackedControllerWrist(1,raw,controller,nullptr),"raw tracked wrist is available");
    float hr=0,hf=0;VR().FirstPersonViewOffset(hr,hf);
    const auto horizontal=locomotion::Rotate({hr,hf},WeaponCollisionHeading());const float units=LiveWorldUnitsPerMetre();
    const Vec eye{g_scenePose.x_pos+horizontal.x*units,g_scenePose.y_pos+VR().FirstPersonVerticalOffset()*units,g_scenePose.z_pos+horizontal.z*units};
    const auto tip=MuzzleLocal(1,1,WeaponCollisionGame());
    const Vec reach=Sub(Transform(raw,tip),eye);weaponTestNormal=Scale(reach,1/firstperson::Length(reach));
    weaponTestLimit=Dot(weaponTestNormal,eye)+firstperson::Length(reach)*.7f;
    weaponTestQueries=0;g_weaponCollision[0]=g_weaponCollision[1]={};
    const auto scene=g_scenePose;
    const auto pose=render();Frame blocked{};Check(BuildControllerWrist(1,blocked,controller),"blocked wrist is available");
    Check(weaponTestQueries>0 && firstperson::Length(Sub(blocked.origin,raw.origin))>1,"native room query retracts the rendered wrist");
    float view[3][3]{},camera[3]{};Check(CameraViewFrame(view,camera),"collision fixture has a render camera frame");
    Check(firstperson::Length(Sub(Add(pose[10].origin,{camera[0],camera[1],camera[2]}),blocked.origin))<.1f,
          "body arm and weapon use the same retracted wrist");
    GunPose gun{};Check(BuildGunPose(1,gun),"blocked gun pose remains available for native firing");
    Check(firstperson::Length(Sub(gun.muzzle,Transform(blocked,tip)))<.1f,"firing muzzle follows the retracted weapon");
    Check(!std::memcmp(&blocked.basis,&raw.basis,sizeof(raw.basis)) && !std::memcmp(&scene,&g_scenePose,sizeof(scene)),
          "weapon contact cannot rotate aim or move the view");
    Frame tracked{};Check(TrackedControllerWrist(1,tracked,controller,nullptr) && !std::memcmp(&tracked,&raw,sizeof(raw)),
          "collision does not overwrite physical controller tracking");
    const int queries=weaponTestQueries;BuildControllerWrist(1,blocked,controller);
    Check(weaponTestQueries==queries,"repeated stereo/muzzle reads do not repeat native collision work");
    settings.firstPersonWeaponCollision=false;BuildControllerWrist(1,blocked,controller);
    Check(firstperson::Length(Sub(blocked.origin,raw.origin))<.001f,"INI opt-out restores exact tracked weapon position");
    settings.firstPersonWeaponCollision=true;status=2;BuildControllerWrist(1,blocked,controller);
    Check(firstperson::Length(Sub(blocked.origin,raw.origin))<.001f,"equip and holster transitions retain physical ownership");
    weapon=oldWeapon;status=oldStatus;settings.firstPersonWeaponCollision=oldOption;g_weaponCollisionReady=oldReady;
    dll.getFloor=oldFloor;g_motionDll=oldMotion;
    g_weaponCollision[0]=g_weaponCollision[1]={};
}
