namespace {
void TestHolsterGestures() {
    using tr::motiongun::HolsterInput;
    for(int hand:{0,1}) for(float distance:{.14f,.22f,.26f,.29f,.30f}) {
        HolsterInput h;
        Check(h.Hover(hand,hand,1,distance,0,1),"expanded draw zone advertises hover readiness without Grip");
        Check(h.Poll(hand,true,true,hand,1,distance,0,1),"off-centre grip inside 30 cm can start drawing");
        h.Poll(hand,true,true,hand,1,.24f,0,1);
        Check(!h.active,"wider capture zone cannot draw from minor grip jitter");
        h.Poll(hand,true,true,hand,1,.44f,0,1);
        h.Poll(hand,false,true,hand,1,.44f,4,1);
        Check(h.mask==(1u<<hand),"withdrawn gun stays equipped when Grip releases outside capture zone");
        Check(h.Hover(hand,hand,1,distance,4,1),"expanded stow zone has the same hover feedback as draw zone");
        h.Poll(hand,true,true,hand,1,distance,4,1);
        h.Poll(hand,false,true,hand,1,distance,4,1);
        Check(h.mask==0,"off-centre release inside 30 cm returns only that hand's gun");
    }
    HolsterInput zone;
    Check(!zone.Hover(1,1,1,.301f,0,1) && !zone.Hover(0,0,2,.1f,0,2) &&
          !zone.Hover(1,2,4,.1f,4,1) && !zone.Hover(1,1,1,.1f,1,1),
          "hover rejects out-of-reach, wrong-hand, incompatible and busy weapon states");
    for(int weapon:{1,3}) for(int first:{0,1}) {
        HolsterInput h;const int other=1-first;
        Check(!h.Poll(first,true,true,first,weapon,.5f,0,weapon),"grip away from a holster remains a normal button");
        Check(!h.Poll(first,true,true,first,weapon,.02f,0,weapon),"holding a normal grip cannot accidentally pick up a gun on entry");
        h.Poll(first,false,true,first,weapon,.02f,0,weapon);
        Check(h.Poll(first,true,true,first,weapon,.02f,0,weapon),"grip at matching hip latches the gun");
        h.Poll(first,true,true,first,weapon,.1f,0,weapon);
        Check(!h.active,"small hand movement inside holster cannot draw");
        h.Poll(first,true,true,first,weapon,.40f,0,weapon);
        Check(h.active && h.mask==(1u<<first) && h.request,"withdrawal equips only that hand");
        h.request=false;
        Check(!h.CanFire(first,true) && !h.CanFire(other,true),"grip transfer and holstered hand cannot fire");
        h.Poll(first,false,true,first,weapon,.40f,4,weapon);
        Check(h.Has(first) && !h.CanFire(first,true),"release away keeps gun held and requires trigger release");
        Check(h.CanFire(first,false) && h.CanFire(first,true),"released trigger re-arms drawn gun only");
        h.Poll(other,true,true,other,weapon,.02f,4,weapon);
        h.Poll(other,true,true,other,weapon,.40f,4,weapon);
        Check(h.mask==3 && !h.request,"second gun draws without toggling shared native equip");
        h.Poll(other,false,true,other,weapon,.40f,4,weapon);
        h.Poll(first,true,true,first,weapon,.4f,4,weapon);
        Check(h.Poll(first,true,true,first,weapon,.06f,4,weapon),"armed gun can enter holster with grip already held");
        h.Poll(first,false,true,first,weapon,.06f,4,weapon);
        Check(h.mask==(1u<<other) && !h.request,"returning one gun leaves other armed");
        h.Poll(other,true,true,other,weapon,.06f,4,weapon);
        h.Poll(other,false,true,other,weapon,.06f,4,weapon);
        Check(h.mask==0 && h.request,"last returned gun requests native holster");
    }
    for(int failure=0;failure<3;++failure) {
        HolsterInput h;h.Poll(1,true,true,1,1,.04f,0,1);
        if(failure==0) h.Poll(1,false,true,1,1,.05f,0,1);
        else h.Poll(1,true,false,1,1,.40f,0,1);
        Check(!h.active && !h.request,"release or tracking loss cancels incomplete draw");
        h.Poll(1,true,true,1,1,.4f,0,1);
        Check(!h.active,"reacquired distant controller never completes canceled grab");
    }
    for(int weapon:{2,4,5,6}) {
        HolsterInput h;
        Check(!h.Poll(0,true,true,0,weapon,.01f,0,weapon),"single weapons reject left-hand pickup");
        Check(h.Poll(1,true,true,2,weapon,.01f,0,weapon),"single weapon accepts right-hand pickup");
        h.Poll(1,true,true,2,weapon,.40f,0,weapon);
        Check(h.mask==2,"single weapon occupies right hand only");
    }
    HolsterInput both;
    for(int hand=0;hand<2;++hand) both.Poll(hand,true,true,hand,3,.02f,0,3);
    for(int hand=0;hand<2;++hand) both.Poll(hand,true,true,hand,3,.40f,0,3);
    Check(both.mask==3 && both.request,"simultaneous grip withdrawal equips both independent guns");
    HolsterInput incompatible;
    Check(!incompatible.Poll(1,true,true,2,4,.02f,4,1),"back weapon cannot replace a gun still held in other hand");
    for(int hand:{0,1}) {
        HolsterInput nativePair;
        nativePair.Poll(hand,true,true,hand,1,.04f,4,1);
        nativePair.Poll(hand,false,true,hand,1,.04f,4,1);
        Check(nativePair.active && nativePair.mask==(1u<<(1-hand)) && !nativePair.request,
              "first physical stow after normal LT draw adopts native pair and removes only that gun");
    }
}

void TestHolsterHoverHaptics() {
    for(uint64_t step:{1,7,11,16,33}) {
        tr::GunHaptics h;int count=0;uint64_t last=0;
        for(uint64_t now=100;now<500;now+=step) {
            h.Hover(0,true,now);
            h.Update(now,[&](int hand,int duration) {
                Check(hand==0 && duration==700,"holster hover rumbles only matching controller at gentle strength");
                Check(!count || now-last>=60,"hover pulse rate stays bounded across input/render rates");
                last=now;++count;
            });
        }
        Check(count>=5 && count<=7,"hover feedback repeats while the hand stays in range");
        h.Hover(0,false,500);
        h.Update(550,[&](int,int){Check(false,"leaving holster immediately cancels hover pulses");});
        h.Hover(1,true,600);
        h.Update(701,[&](int,int){Check(false,"stale input cannot leave a controller rumbling");});
    }
    tr::GunHaptics h;int durationSeen=0;
    h.Hover(1,true,100);h.Shot(1,100);
    h.Update(100,[&](int hand,int duration){Check(hand==1 && duration==3999,"shot/transfer burst takes priority over proximity rumble");});
    h.Hover(1,false,102);h.Hover(1,true,103);
    h.Update(104,[&](int,int){Check(false,"combined haptics retain OpenVR's minimum pulse interval");});
    h.Hover(1,true,180);h.Update(180,[&](int,int duration){durationSeen=duration;});
    Check(durationSeen==700,"gentle hover resumes after strong confirmation ends");
    h.Hover(1,false,181);h.Hover(1,true,182);
    h.Update(190,[&](int,int){Check(false,"zone-edge jitter cannot spam proximity pulses");});
    h.Reset();Check(!h.hoverUntil[0] && !h.hoverUntil[1],"VR reset clears proximity feedback");
}

void __cdecl FakeHolsterJoint(uint8_t*,tr::PHD_VECTOR* p,int,int) { *p={1000,2000,3000}; }

template<class Rva> void TestPhysicalHolsters(Rva rva) {
    using namespace tr;
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
    const auto savedStatus=status,savedGun=gun;
    const auto savedObjects=dll.objects,savedJoint=dll.getJointAbsPositionLerp;
    const auto savedBits=*reinterpret_cast<uint32_t*>(itemMemory+12);
    const auto savedObject=*reinterpret_cast<int16_t*>(itemMemory+16);
    const auto savedScope=g_bodySkinScope;
    const auto savedConfig=config;
    const auto savedOutfit=*reinterpret_cast<uint8_t**>(appMemory+0x4f8);
    uint8_t outfit[64]{};
    *reinterpret_cast<int32_t*>(outfit+0x1c)=1;
    *reinterpret_cast<int32_t*>(outfit+0x10)=*reinterpret_cast<int32_t*>(outfit+0x14)=1;
    *reinterpret_cast<uint8_t**>(appMemory+0x4f8)=outfit;
    uint8_t savedSelectors[5];std::memcpy(savedSelectors,appMemory+0x508,5);
    const auto oldState=*reinterpret_cast<int16_t*>(itemMemory+18);
    *reinterpret_cast<int16_t*>(itemMemory+18)=2;
    config.firstPersonMotionGuns=true;appMemory[0x9e4]=1;
    status=4;gun=1;ResetPhysicalHolsters();
    Check(MotionReady(),"holster regression has valid tracked weapon context");
    appMemory[0x508]=appMemory[0x509]=5;appMemory[0x50a]=appMemory[0x50b]=0;appMemory[0x50c]=5;
    { HolsterRenderScope scope(itemMemory,appMemory); }
    g_holsters.active=true;g_holsters.weapon=1;g_holsters.mask=2;
    for(int nativeStatus:{2,3,4}) {
        status=int16_t(nativeStatus);
        Check(MotionReady(),"physical weapon transitions retain tracked hand rendering");
        {
            HolsterRenderScope scope(itemMemory,appMemory);
            Check(appMemory[0x508]==1 && appMemory[0x509]==5 && appMemory[0x50a]==1 &&
                appMemory[0x50b]==0 && appMemory[0x50c]==5,"right-only draw renders left rest hand and full left holster");
        }
        Check(appMemory[0x508]==5 && !appMemory[0x50a],"render scope restores native pair selectors");
    }
    status=4;
    const auto* savedShadow=g_shadowDll;
    ShadowDll shadow{};shadow.renderPass=rva(&nativeShadowPass);
    g_shadowDll=&shadow;nativeShadowPass=4;
    { HolsterRenderScope scope(itemMemory,appMemory);
      Check(!scope.app && appMemory[0x508]==5,"native shadow render never receives first-person hand selectors"); }
    nativeShadowPass=0;g_shadowDll=savedShadow;
    const auto oldDraw=g_hDrawCreatureHD.m_trampoline;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(+[](uint8_t* item,int32_t useBits,int32_t) {
        const auto bits=*reinterpret_cast<uint32_t*>(item+12);
        Check((bits==2 || bits==16 || bits==128) && useBits==1 && g_bodySkinScope && g_renderArm<0,
              "armed hip/back attachments render with body skinning instead of disappearing");
    });
    for(uint32_t bits:{2u,16u,128u}) {
        *reinterpret_cast<uint32_t*>(itemMemory+12)=bits;
        Detour_DrawCreatureHD(itemMemory,1,0);
        Check(*reinterpret_cast<uint32_t*>(itemMemory+12)==bits,"attachment draw preserves native mesh mask");
    }
    g_hDrawCreatureHD.m_trampoline=oldDraw;
    for(int kind:{0,1,2}) {
        *reinterpret_cast<int32_t*>(outfit+0x1c)=kind;
        { HolsterRenderScope scope(itemMemory,appMemory); }
        Check(g_holsterVisible[0]==(kind==1) && g_holsterVisible[2]==(kind==1),
              "outfits without native weapon meshes cannot expose invisible holsters");
    }
    *reinterpret_cast<int32_t*>(outfit+0x1c)=1;
    for(int style:{0,1}) {
        ResetPhysicalHolsters();g_gunEquip.Reset();g_gunTriggers.Reset();
        appMemory[0x9ec]=uint8_t(style);status=0;gun=1;
        g_holsters.Poll(1,true,true,1,1,.02f,0,1);
        g_holsters.Poll(1,true,true,1,1,.40f,0,1);
        uint8_t lt=0,rt=0;UpdateGunTriggers(lt,rt,true,100);
        Check(lt==255 && *reinterpret_cast<int16_t*>(laraMemory+6)==1,"physical draw requests selected native gun in hold and toggle styles");
        status=2;lt=rt=0;UpdateGunTriggers(lt,rt,true,110);
        Check(lt==(style?0:255) && !g_gunTriggers.WantsShot(),"native draw acknowledges grip request without firing");
        status=4;g_holsters.Poll(1,false,true,1,1,.40f,4,1);
        lt=rt=0;UpdateGunTriggers(lt,rt,true,120);
        lt=rt=255;UpdateGunTriggers(lt,rt,true,130);
        Check(!g_gunTriggers.pending[0] && g_gunTriggers.pending[1],"two triggers fire only the physically drawn right gun");
        const int calls=nativeShotCalls;const int16_t aim[2]={};
        g_gunTriggers.pending[0]=true;
        Check(FireWeaponForHand(0,1,nullptr,nullptr,aim)==0 && nativeShotCalls==calls,"native fire guard independently rejects holstered hand even with queued shot");
        g_holsters.Poll(1,true,true,1,1,.02f,4,1);
        g_holsters.Poll(1,false,true,1,1,.02f,4,1);
        lt=rt=0;UpdateGunTriggers(lt,rt,true,140);
        Check(lt==(style?255:0) && !g_gunTriggers.WantsShot(),"returning last gun requests native undraw in either style");
    }
    ResetPhysicalHolsters();status=4;gun=1;
    static uint8_t object[3792]{};
    dll.objects=rva(object);dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeHolsterJoint));
    *reinterpret_cast<int16_t*>(itemMemory+16)=0;
    *reinterpret_cast<uint32_t*>(itemMemory+12)=0x3fff;
    float palette[15*12]{};
    for(int joint=0;joint<15;++joint) {
        auto* bind=reinterpret_cast<float*>(object+1676)+joint*16;
        bind[0]=bind[5]=bind[10]=bind[15]=1;
        motiongun::WriteRows({firstperson::IdentityBasis(),{100,200,300}},palette+joint*12);
    }
    {
        HolsterRenderScope scope(itemMemory,appMemory);g_bodySkinScope=true;
        motiongun::Vec offset{};
        Check(HolsterCameraOffset(itemMemory,palette,15,offset) && offset.x==900 && offset.y==1800 && offset.z==2700,
              "holster world conversion recovers original camera translation before body fitting");
        for(int joint=0;joint<15;++joint) { palette[joint*12+3]+=50;palette[joint*12+7]-=20; }
        CaptureHolsterAnchors(itemMemory,palette,15,offset);
        Check(g_holsterAnchor[0].valid && g_holsterAnchor[0].point.x==960 &&
              g_holsterAnchor[1].point.x==1140 && g_holsterAnchor[0].point.y==1995,
              "interactive hips include the visible body fit and preserve left/right positions");
        *reinterpret_cast<uint32_t*>(itemMemory+12)=2;
        Check(!HolsterCameraOffset(itemMemory,palette,15,offset),"attachment pass cannot overwrite body anchor capture");
        // Body meshes may use a reordered HD palette while holsters retain
        // canonical TRM skinning. Rotated IK hips must still match the latter.
        int32_t mapping[15]{};float poses[15*12]{};
        *reinterpret_cast<int32_t*>(object+off::object_geom+28)=15;
        *reinterpret_cast<const int32_t**>(object+off::object_geom+48)=mapping;
        *reinterpret_cast<const float**>(object+off::object_geom+72)=poses;
        const motiongun::Frame hdPose{firstperson::IdentityBasis(),{}};
        const motiongun::Frame turned{{{{0,0,1},{0,1,0},{-1,0,0}}},{50,300,70}};
        for(int index=0;index<15;++index) {
            mapping[index]=14-index;
            reinterpret_cast<float*>(object+1676)[index*16+13]=-100;
            motiongun::WriteRows(hdPose,poses+index*12);
            motiongun::WriteRows(motiongun::Multiply(turned,motiongun::HdInverseBind(hdPose)),palette+index*12);
        }
        CaptureHolsterAnchors(itemMemory,palette,15,offset);
        Check(g_holsterAnchor[0].point.x==930 && g_holsterAnchor[0].point.y==2015 &&
              g_holsterAnchor[0].point.z==2860 && g_holsterAnchor[1].point.z==2680,
              "rotated reordered HD body palettes place canonical holsters on the correct fitted hips");
        std::memset(object+off::object_geom,0,120);
    }
    // Controller samples can be located exactly at the body anchors without
    // assuming HMD offsets or world heading; withdrawal changes tracked pose.
    ResetPhysicalHolsters();status=0;gun=1;
    g_holsterItem=itemMemory;g_holsterLevel=AppState(drva::app_off::level);
    g_holsterSeen=1000;g_holsterKinds[0]=g_holsterKinds[1]=1;
    g_holsterVisible[0]=g_holsterVisible[1]=true;
    for(int hand=0;hand<2;++hand) {
        float r,d,f;Check(VR().FirstPersonControllerOffset(hand,r,d,f),"holster input has tracked controller offsets");
        g_holsterAnchor[hand]={motiongun::HandInWorld({float(g_scenePose.x_pos),float(g_scenePose.y_pos),float(g_scenePose.z_pos)},
            r,d,f,g_headingBase,LiveWorldUnitsPerMetre()),true};
    }
    Check(UpdatePhysicalHolsterGrips(false,false,true,true,true,1000)==0 &&
          VR().m_gunHaptics.hoverUntil[0] && VR().m_gunHaptics.hoverUntil[1] && !g_holsters.active,
          "hovering with both empty hands queues feedback without consuming Grip or equipping guns");
    UpdatePhysicalHolsterGrips(false,false,false,true,true,1000);
    Check(!VR().m_gunHaptics.hoverUntil[0] && VR().m_gunHaptics.hoverUntil[1],
          "lost controller tracking stops only that hand's hover feedback");
    Check(UpdatePhysicalHolsterGrips(true,true,true,true,true,1001)==3,"both spatial grabs consume grips together without Action");
    const auto savedController=VR().m_rawControllerPose[1];
    VR().m_rawControllerPose[1].m[1][3]+=.40f;
    Check(UpdatePhysicalHolsterGrips(true,true,true,true,true,1010)==3 && g_holsters.mask==2,
          "moving right controller out draws right gun while left stays grabbed in holster");
    UpdatePhysicalHolsterGrips(true,true,true,true,true,1200);
    Check(!g_holsters.hand[0].claimed && !g_holsters.hand[1].claimed && g_holsters.mask==2,
          "stale rendered body cancels transfers while retaining already drawn gun");
    Check(!VR().m_gunHaptics.hoverUntil[0] && !VR().m_gunHaptics.hoverUntil[1],
          "stale holster anchors stop both hover effects");
    CancelHolsterTracking();
    Check(g_holsters.mask==2 && !g_holsterSeen,"recenter invalidates spatial samples without equipping the other hand");
    g_holsters.mask=0;g_holsters.request=false;gun=3;
    UpdatePhysicalHolsterGrips(false,false,true,true,true,1210);
    Check(!g_holsters.active,"inventory selection while holstered cannot retain the previous gun in grab zones");
    VR().m_rawControllerPose[1]=savedController;
    ResetPhysicalHolsters();g_gunTriggers.Reset();g_gunEquip.Reset();
    status=savedStatus;gun=savedGun;dll.objects=savedObjects;dll.getJointAbsPositionLerp=savedJoint;
    *reinterpret_cast<uint32_t*>(itemMemory+12)=savedBits;*reinterpret_cast<int16_t*>(itemMemory+16)=savedObject;
    *reinterpret_cast<int16_t*>(itemMemory+18)=oldState;g_bodySkinScope=savedScope;config=savedConfig;
    std::memcpy(appMemory+0x508,savedSelectors,5);appMemory[0x9ec]=0;
    *reinterpret_cast<uint8_t**>(appMemory+0x4f8)=savedOutfit;
}
} // namespace
