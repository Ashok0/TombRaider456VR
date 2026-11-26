void TestConnectedWeaponReach() {
    using namespace tr;using namespace tr::motiongun;
    trackingtest::Clock clock;
    const auto savedReach0=g_connectedWeaponReach[0],savedReach1=g_connectedWeaponReach[1];
    const auto savedSerial=VR().m_poseSerial;
    auto& root=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    const auto savedRoot=root;const auto savedHeading=g_heading.base;
    const auto savedConfig=testConfig;
    auto& anim=*reinterpret_cast<int16_t*>(item+off::item_anim_number);
    const auto savedAnim=anim;
    Frame chain[3]={{firstperson::IdentityBasis(),{100,-600,200}},
                    {firstperson::IdentityBasis(),{100,-300,200}},
                    {firstperson::IdentityBasis(),{100,-100,200}}};
    for(int hand=0;hand<2;++hand) {
        root={};g_heading.base=0;VR().m_poseSerial=100;
        PublishConnectedWeaponReach(item,hand,chain,{});
        Frame requested{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},.7f)),{100,-600,1200}};
        auto limited=requested;LimitConnectedWeaponWrist(item,hand,limited);
        Check(firstperson::Length(Sub(limited.origin,ExpectedConnectedWrist(chain,requested).origin))<.02f,
              "weapon wrist follows the soft limit from the unstretched reference");
        Check(!std::memcmp(&limited.basis,&requested.basis,sizeof(Basis)),"reach limit retains fresh controller orientation");
        Frame inside{requested.basis,{100,-600,500}};const auto exact=inside;
        LimitConnectedWeaponWrist(item,hand,inside);
        Check(!std::memcmp(&inside,&exact,sizeof(Frame)),"reachable weapon frame is unchanged");
        const float savedFit=Cfg().firstPersonArmLengthPercent;
        for(float fit:{80.f,100.f,120.f}) {
            testConfig.firstPersonArmLengthPercent=fit;
            PublishConnectedWeaponReach(item,hand,chain,{});
            auto fitted=requested;LimitConnectedWeaponWrist(item,hand,fitted);
            Check(firstperson::Length(Sub(fitted.origin,ExpectedConnectedWrist(chain,requested,fit).origin))<.03f,
                  "live arm fit affects weapon reach without compounding extension");
        }
        testConfig.firstPersonArmLengthPercent=savedFit;PublishConnectedWeaponReach(item,hand,chain,{});
        const auto original=g_connectedWeaponReach[hand];
        for(int reason=0;reason<12;++reason) {
            auto& reach=g_connectedWeaponReach[hand];reach=original;
            if(reason==0) reach.valid=false;
            if(reason==1) reach.item=Ptr<uint8_t>(dll.lara);
            if(reason==2) ++reach.game;
            if(reason==3) ++reach.weapon;
            if(reason==4) ++reach.object;
            if(reason==5) ++reach.level;
            if(reason==6) reach.serial-=2;
            if(reason==7) ++reach.serial;
            if(reason==8) reach.time-=.11;
            if(reason==9) reach.units*=2;
            if(reason==10) ++reach.bodyOffset;
            if(reason==11) ++reach.armLength;
            auto untouched=requested;LimitConnectedWeaponWrist(item,hand,untouched);
            Check(!std::memcmp(&untouched,&requested,sizeof(Frame)),"weapon cannot inherit invalid, stale or recalibrated reach");
        }
        g_connectedWeaponReach[hand]=original;
        root.x_pos=120;root.z_pos=-75;g_heading.base=1.57079632679f;++VR().m_poseSerial;
        const Frame transport{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},g_heading.base)),{120,0,-75}};
        auto moved=Multiply(transport,requested);
        const auto wanted=Multiply(transport,limited);
        LimitConnectedWeaponWrist(item,hand,moved);
        Check(firstperson::Length(Sub(moved.origin,wanted.origin))<.02f,
              "previous-pose weapon reach follows native movement and artificial turning");
        Check(!std::memcmp(&moved.basis,&wanted.basis,sizeof(Basis)),"turn transport does not overwrite current hand rotation");
        root={};g_heading.base=0;VR().m_poseSerial=100;
        anim=24;Check(NativeLandingAnimation(item),"fixture enters native hard landing");
        auto landing=requested;LimitConnectedWeaponWrist(item,hand,landing);
        Check(!std::memcmp(&landing,&requested,sizeof(Frame)),"native landing cannot use tracked reach");
        g_connectedWeaponReach[hand]={};PublishConnectedWeaponReach(item,hand,chain,{});
        Check(!g_connectedWeaponReach[hand].valid,"native landing cannot publish tracked arm geometry");
        anim=savedAnim;
    }
    ResetBodyFollower();
    Check(!g_connectedWeaponReach[0].valid && !g_connectedWeaponReach[1].valid,"body reset invalidates both weapon reach caches");
    root=savedRoot;g_heading.base=savedHeading;VR().m_poseSerial=savedSerial;testConfig=savedConfig;
    g_connectedWeaponReach[0]=savedReach0;g_connectedWeaponReach[1]=savedReach1;
}
