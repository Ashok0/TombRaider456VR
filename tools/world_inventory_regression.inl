namespace {
uint8_t* ammoTestApp=appMemory;
uint8_t ammoTestLevel=1;
int ammoHotkeyCalls=0,ammoExpectedKey=-1;
void __cdecl FakeAmmoHotkeys() {
    ++ammoHotkeyCalls;
    if(ammoExpectedKey==-2) Check(appMemory[0x89d]==0,"invalid queued request reaches native handler without a synthetic hotkey");
    if(ammoExpectedKey<0) return;
    for(int key=0;key<6;++key)
        Check(appMemory[0x89c+key]==(key==ammoExpectedKey),"native handler sees exactly the equipped weapon hotkey");
    Check(appMemory[0x8f2]==0,"pad modifier cannot select a different native weapon during ammo routing");
}
void TestArmedInventoryButton() {
    using namespace tr;
    const auto oldConfig=config;const auto oldVR=VR();const auto oldLayout=layout;
    const auto oldDll=dll;const auto* oldBound=g_boundDll;const auto oldBase=g_boundBase;const int oldGame=game;
    uint8_t savedApp[sizeof(appMemory)],savedLara[sizeof(laraMemory)];
    std::memcpy(savedApp,appMemory,sizeof(appMemory));std::memcpy(savedLara,laraMemory,sizeof(laraMemory));
    std::memset(appMemory,0,sizeof(appMemory));layout.app=0;
    config.enabled=config.gamepadEnabled=config.worldInventory=true;config.monoTracking=false;
    VR().m_system=reinterpret_cast<vr::IVRSystem*>(1);VR().m_poseValid=true;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));g_boundDll=&dll;
    auto rva=[](const void* p) { return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase); };
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);
    ammoinput::Build row{0,0,rva(&ammoTestApp),rva(&ammoTestLevel),0};
    ammoinput::dll=&dll;ammoinput::base=g_boundBase;ammoinput::build=&row;
    ammoinput::hotkeysHook.m_installed=true;
    ammoinput::hotkeysHook.m_trampoline=reinterpret_cast<void*>(&FakeAmmoHotkeys);
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    auto& weapon=*reinterpret_cast<int16_t*>(laraMemory+4);
    for(int which:{0,1}) for(int gun=1;gun<=6;++gun) for(int state:{0,2,3,4}) {
        game=dll.game=which;weapon=gun;status=state;
        ammoTestLevel=gun==5 ? 11 : 1;
        ammoinput::held=ammoinput::consumeHold=false;ammoinput::queuedGun=0;g_worldInventory.Reset();
        XGamepad pad{};pad.wButtons=XB_BACK|XB_X|XB_A;
        pad.bLeftTrigger=240;pad.bRightTrigger=250;pad.sThumbLX=17000;pad.sThumbRX=-13000;
        auto expected=pad;if(state) expected.wButtons &= uint16_t(~XB_BACK);
        ammoinput::Apply(pad);
        Check(!std::memcmp(&pad,&expected,sizeof(pad)),"armed X removes only menu input, retaining shooting movement Walk and jump");
        const int key=gun==4 ? 1 : which==0 && gun==5 ? 4 : which==0 && gun==6 ? 5 : which==1 && gun==5 ? 0 : -1;
        Check(ammoinput::queuedGun==(state==4 && key>=0 ? gun : 0),"only ready weapons with ammo variants queue a native hotkey");
        bool shifted=false;ApplyWorldInventory(pad,shifted);
        Check(g_worldInventory.pending==(state==0),"armed X never prepares or opens world inventory");
        for(int k=0;k<6;++k) appMemory[0x89c+k]=uint8_t(2+k);
        appMemory[0x8f2]=7;
        ammoExpectedKey=state==4 ? key : -1;const int calls=ammoHotkeyCalls;
        ammoinput::Hotkeys();
        Check(ammoHotkeyCalls==calls+1 && ammoinput::queuedGun==0,"one native hotkeys call consumes queued ammo press");
        for(int k=0;k<6;++k) Check(appMemory[0x89c+k]==2+k,"keyboard hotkeys restored after native ammo call");
        Check(appMemory[0x8f2]==7,"native pad modifier restored after ammo call");
        pad.wButtons=XB_BACK;ammoinput::Apply(pad);
        Check(!ammoinput::queuedGun,"holding X cannot repeatedly cycle ammo");
        if(state) {
            status=0;pad.wButtons=XB_BACK;ammoinput::Apply(pad);
            Check(!pad.wButtons,"same armed press cannot open inventory when holstering completes");
        }
        pad={};ammoinput::Apply(pad);
        status=0;pad.wButtons=XB_BACK;ammoinput::Apply(pad);
        Check(pad.wButtons==XB_BACK,"fresh unarmed X still opens native inventory");
    }
    game=dll.game=1;status=4;weapon=5;ammoTestLevel=1;
    ammoinput::held=false;ammoinput::queuedGun=0;
    XGamepad pad{};pad.wButtons=XB_BACK;ammoinput::Apply(pad);
    Check(!pad.wButtons && !ammoinput::queuedGun,"TR5 chapter-specific HK key cannot equip pistols in an earlier chapter");
    for(int guard=0;guard<4;++guard) {
        game=dll.game=0;weapon=4;status=4;ammoinput::held=false;
        pad.wButtons=XB_BACK;ammoinput::Apply(pad);
        Check(ammoinput::queuedGun==4,"guard test starts with a queued shotgun press");
        if(guard==0) weapon=1;
        if(guard==1) status=3;
        if(guard==2) ammoinput::queuedAt-=1000;
        if(guard==3) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=1;
        ammoExpectedKey=-2;appMemory[0x89d]=0;
        ammoinput::Hotkeys();
        Check(!ammoinput::queuedGun && appMemory[0x89d]==0,"stale ammo requests cannot act after weapon or gameplay state changes");
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=0;
    }
    ammoinput::Reset();ammoinput::hotkeysHook.m_installed=false;g_worldInventory.Reset();
    config=oldConfig;VR()=oldVR;layout=oldLayout;dll=oldDll;g_boundDll=oldBound;g_boundBase=oldBase;game=oldGame;
    std::memcpy(appMemory,savedApp,sizeof(appMemory));std::memcpy(laraMemory,savedLara,sizeof(laraMemory));
}
void TestWorldInventory() {
    using namespace tr;
    using inventory::Input;
    using inventory::Session;
    for(int which:{0,1}) for(bool moving:{false,true}) {
        Session s;
        bool owned=false;
        const Input opening{XB_BACK,moving ? .7f : 0,0};
        auto in=opening;
        Check(s.Update(in,true,false,which,100,.5f,owned) && !owned && in.buttons==XB_BACK && in.x==opening.x,
              "X/BACK passes unchanged to native menu/ammo handling while anchor is prepared");
        for(uint64_t now:{110,150,250,300}) {
            in=opening;
            Check(!s.Update(in,true,false,which,now,.5f,owned) && !owned &&
                  in.buttons==XB_BACK && in.x==opening.x,
                  "held native X keeps its original timing without synthetic menu pulses");
        }
        in=opening;s.Update(in,true,true,which,350,.5f,owned);
        Check(s.open && !s.pending && !in.buttons && !in.x && !in.y,
              "opening X press does not immediately close native inventory");
        in={};s.Update(in,true,true,which,360,.5f,owned);
        for(auto direction:{Input{0,1,.2f},Input{0,-1,.2f},Input{0,.2f,1},Input{0,.2f,-1}}) {
            in=direction;s.Update(in,true,true,which,370,.5f,owned);
            const uint16_t expected=direction.x==1 ? XB_DPAD_RIGHT : direction.x==-1 ? XB_DPAD_LEFT :
                direction.y==1 ? XB_DPAD_UP : XB_DPAD_DOWN;
            Check(in.buttons==expected && !in.x && !in.y && owned,
                  "unshifted LS navigates exactly one native inventory direction");
        }
        for(uint16_t button:{uint16_t(XB_A),uint16_t(XB_B),uint16_t(XB_BACK)}) {
            in={button};s.Update(in,true,true,which,380,.5f,owned);
            Check(in.buttons==button && owned,"A selects, B backs out and X/BACK retains native close");
        }
        in={XB_Y,0,0,255,255};s.Update(in,true,true,which,390,.5f,owned);
        Check(!in.lt && !in.rt && owned,"menu input bypasses gun, graphics and view chords");
        in={XB_START|XB_RIGHT_THUMB};s.Update(in,true,true,which,395,.5f,owned);
        Check(!in.buttons,"menu chord and zoom cannot leak into a spatial inventory session");
        in={XB_A|XB_B,1,0,255,255};s.Update(in,true,false,which,400,.5f,owned);
        Check(!s.open && owned && !in.buttons && !in.x && !in.lt && !in.rt,
              "held select/back/trigger/stick cannot jump roll shoot or move on close");
        in={};s.Update(in,true,false,which,410,.5f,owned);
        in={XB_A,1,0,0,255};s.Update(in,true,false,which,420,.5f,owned);
        Check(!owned && in.buttons==XB_A && in.x==1 && in.rt==255,
              "fresh gameplay input works after release drain");
        in=opening;Check(s.Update(in,true,false,which,430,.5f,owned),"fresh X can prepare another session");
        in=opening;s.Update(in,true,false,which,1500,.5f,owned);
        Check(!s.open && !s.pending && !owned && in.buttons==XB_BACK && in.x==opening.x,
              "ammo-only press times out without consuming input or starting an exit drain");
        in={};s.Update(in,true,false,which,1510,.5f,owned);
        in={XB_B};s.Update(in,true,false,which,1520,.5f,owned);
        Check(!owned && in.buttons==XB_B,"rejected open releases gameplay controls");
    }
    for(int guard=0;guard<7;++guard) {
        Session s;bool owned=false;
        Input in{XB_BACK};
        if(guard==2) in.buttons|=XB_LEFT_THUMB|XB_RIGHT_THUMB;
        if(guard==3) in={XB_DPAD_DOWN};
        if(guard==4) in={XB_RIGHT_THUMB,0,-1};
        if(guard==5) in={XB_DPAD_DOWN,0,0,0,0,true};
        if(guard==6) in={XB_X}; // XInput X is Walk, not the Touch X menu button.
        Check(!s.Update(in,guard!=0,guard==1,0,100,.5f,owned) && !owned,
              "disabled, existing menus, Photo chord, old R3 shortcuts and Walk do not prepare inventory");
    }
    for(int which:{0,1}) {
        Session s;bool owned=false;
        // Native game keeps inventory closed when X switches ammo. Check
        // repeated presses, held fire, movement and the pending timeout.
        for(uint64_t now:{100,110,120,140,150,160,1200,1300}) {
            Input in{uint16_t(now==120 || now==150 ? 0 : XB_BACK),.7f,-.3f,255,255};
            const auto before=in;
            s.Update(in,true,false,which,now,.5f,owned);
            Check(!owned && !s.open && !s.release && in.buttons==before.buttons &&
                  in.x==before.x && in.y==before.y && in.lt==255 && in.rt==255,
                  "ammo switching never opens spatial inventory or interrupts triggers and movement");
        }
        s.Sync(true,true,which,2400);
        Check(!s.open,"expired ammo press cannot claim an unrelated later menu");
    }

    const auto oldConfig=config;const auto oldVR=VR();const int oldGame=game;
    const auto oldLayout=layout;
    const bool oldActive=g_active;
    uint8_t savedApp[sizeof(appMemory)];std::memcpy(savedApp,appMemory,sizeof(appMemory));
    layout.app=0; // Base() in the test harness is appMemory.
    config.enabled=config.gamepadEnabled=config.worldInventory=true;
    config.monoTracking=false;config.flipViewY=true;config.swapEyes=false;
    VR().m_system=reinterpret_cast<vr::IVRSystem*>(1);VR().m_poseValid=true;
    std::memset(appMemory,0,sizeof(appMemory));game=0;g_active=false;
    VR().m_rawHeadPose={};for(int i=0;i<3;++i) VR().m_rawHeadPose.m[i][i]=1;
    VR().m_rawHeadPose.m[0][3]=2;VR().m_rawHeadPose.m[1][3]=1.7f;
    VR().m_eyeFromHead[0]=VR().m_eyeFromHead[1]=Affine::Identity();
    VR().m_eyeFromHead[0].r[0][3]=-.032f;VR().m_eyeFromHead[1].r[0][3]=.032f;
    g_worldInventory.Reset();
    const bool oldMenuUsesBack=config.gamepadMenuUsesBack;
    config.gamepadMenuUsesBack=true;
    VRSystem::HandState hands[2]{};hands[0].btnLower=true;
    XState touchState{};bool shifted=false,touchGripAction=false;
    BuildStateFromHands(hands,touchState,shifted,false,touchGripAction);
    Check((touchState.Gamepad.wButtons & XB_BACK)!=0,"actual Touch X maps to native System/BACK");
    XGamepad pad=touchState.Gamepad;
    const auto nativePad=pad;
    Check(!ApplyWorldInventory(pad,shifted) && !std::memcmp(&pad,&nativePad,sizeof(pad)) && g_worldInventory.pending,
          "actual merged adapter prepares anchor while preserving native X ammo/menu input");
    config.gamepadMenuUsesBack=oldMenuUsesBack;
    *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=1;
    Check(WorldInventoryActive(),"renderer observes native inventory entry even before next input poll");
    const auto savedScene=g_scenePose;
    const bool savedSceneValid=g_scenePoseValid;
    inventorySceneReady=true;g_active=true;g_scenePoseValid=true;
    g_scenePose={123,456,789,100,200,300};
    const auto frozen=g_scenePose;
    const auto neutral=VR().m_firstPersonNeutral[0];
    PHD_3DPOS menuCamera{};
    UpdateSceneCamera(menuCamera);
    Check(!std::memcmp(&menuCamera,&frozen,sizeof(menuCamera)) && g_active && g_scenePoseValid,
          "live inventory keeps the first-person camera instead of switching to chase view");
    Check(VR().m_firstPersonNeutral[0]==neutral,
          "paused inventory redraw does not recenter or consume room-scale movement");
    inventorySceneReady=false;g_active=false;g_scenePose=savedScene;g_scenePoseValid=savedSceneValid;
    const auto anchor=g_inventoryAnchor;
    const float scale=LiveWorldUnitsPerMetre();
    const float ipd=LiveIpdScale();
    for(Eye eye:{Eye::Left,Eye::Right}) {
        const auto view=WorldInventoryEyeView(eye);
        Check(Near(view.r[0][3],(eye==Eye::Left ? -.032f : .032f)*scale*ipd) &&
              Near(view.r[1][3],0) && Near(view.r[2][3],0),
              "opening anchor cancels absolute room position and retains eye separation");
    }
    VR().m_rawHeadPose.m[0][3]+=.1f;
    VR().m_rawHeadPose.m[1][3]+=.05f;
    auto view=WorldInventoryEyeView(Eye::Left);
    Check(Near(view.r[0][3],(-.1f-.032f*ipd)*scale) && Near(view.r[1][3],.05f*scale),
          "physical leaning gives anchored inventory real stereo parallax");
    VR().m_headFromTracking=RotY(70);VR().m_headFromTracking.r[0][3]=100;
    VR().m_firstPersonNeutral[0]=-100;
    const auto after=WorldInventoryEyeView(Eye::Left);
    Check(!std::memcmp(&view,&after,sizeof(view)),"pause camera/FP-neutral handoffs cannot move the inventory anchor");
    VR().m_rawHeadPose=anchor;
    const auto turn=RotY(35);
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) VR().m_rawHeadPose.m[i][j]=turn.r[i][j];
    view=WorldInventoryEyeView(Eye::Left);
    Check(std::fabs(view.r[0][2])>.5f,"turning the head looks away from ring instead of dragging it along");
    WorldInventoryRecenter();view=WorldInventoryEyeView(Eye::Left);
    Check(Near(view.r[0][2],0) && Near(view.r[0][0],1),"explicit recenter places ring in front again");
    // Open while looking up/down and with a tilted head. The ring must be
    // upright at eye height and face us once we level our gaze, including
    // vertical gaze where the forward vector cannot supply a reliable yaw.
    for(float yaw:{-2.f,.7f,2.8f}) for(float pitch:{-kPi/2,-1.1f,0.f,1.1f,kPi/2})
        for(float roll:{-.4f,0.f,.4f}) {
            Head(yaw);VR().HeadYawRadians();
            Head(yaw,pitch);
            auto tilt=Affine::Identity();
            tilt.r[0][0]=tilt.r[1][1]=std::cos(roll);
            tilt.r[0][1]=-std::sin(roll);tilt.r[1][0]=std::sin(roll);
            const auto tilted=Mul(FromHmd(VR().m_rawHeadPose),tilt);
            for(int i=0;i<3;++i) for(int j=0;j<4;++j) VR().m_rawHeadPose.m[i][j]=tilted.r[i][j];
            g_worldInventory.Reset();
            *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=0;
            pad={};pad.wButtons=XB_BACK;shifted=false;
            ApplyWorldInventory(pad,shifted);
            *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive)=1;
            Check(WorldInventoryActive(),"tilted opening uses the actual X inventory path");
            Check(Near(g_inventoryAnchor.m[1][3],1.7f) && Near(g_inventoryAnchor.m[1][1],1) &&
                  Near(g_inventoryAnchor.m[0][1],0) && Near(g_inventoryAnchor.m[2][1],0),
                  "inventory opening retains eye height and removes pitch and roll");
            const auto levelAnchor=g_inventoryAnchor;
            Head(yaw);
            for(Eye eye:{Eye::Left,Eye::Right}) {
                const auto straight=WorldInventoryEyeView(eye);
                for(int i=0;i<3;++i) for(int j=0;j<3;++j)
                    Check(Near(straight.r[i][j],i==j ? 1.f : 0.f),"level gaze sees straight inventory text dead on");
                Check(Near(straight.r[1][3],0) && Near(straight.r[2][3],0),"level gaze finds ring at eye height");
            }
            Check(!std::memcmp(&levelAnchor,&g_inventoryAnchor,sizeof(levelAnchor)),"leveling gaze leaves world anchor fixed");
        }
    for(int guard=0;guard<8;++guard) {
        config=oldConfig;config.enabled=config.gamepadEnabled=config.worldInventory=true;config.monoTracking=false;
        VR().m_poseValid=true;game=0;std::memset(appMemory,0,sizeof(appMemory));
        if(guard==0) config.worldInventory=false;
        if(guard==1) config.enabled=false;
        if(guard==2) config.monoTracking=true;
        if(guard==3) game=2;
        if(guard==4) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InTitle)=1;
        if(guard==5) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::InFMV)=1;
        if(guard==6) *reinterpret_cast<int32_t*>(appMemory+drva::app_off::photoMode)=1;
        if(guard==7) VR().m_poseValid=false;
        pad={};pad.wButtons=XB_BACK;shifted=false;
        Check(!ApplyWorldInventory(pad,shifted) && pad.wButtons==XB_BACK,
              "runtime gates preserve native controls outside eligible stereo gameplay");
    }
    g_worldInventory.Reset();config=oldConfig;VR()=oldVR;game=oldGame;g_active=oldActive;layout=oldLayout;
    std::memcpy(appMemory,savedApp,sizeof(appMemory));
}
}
