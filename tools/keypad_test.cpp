// Focused fixture only: the full first-person self-test is never executed.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
alignas(16) uint8_t keypadObjects[250*3792]{};
uint8_t kpCount=0,kpY=0,kpPause=0,kpX=0,kpLeft=0,kpRight=0,kpUp=0,kpDown=0,kpSelect=0,kpMenu=0,kpUse=0,kpLevel=12,kpRich=0,kpCut=0;
uint8_t kpDigits[4]{};
int16_t kpBirthday=0,kpBirthdayTimer=0;
uint32_t kpMesh=0;int32_t kpCombination=0;
uint16_t kpTriggerWords[]={0x8504,0x3e00,0x8069},kpOtherWords[]={0x8004,0,0x8000};
uint16_t* kpTriggerIndex=kpOtherWords;
bool kpNoTrigger=false,kpChangeLevel=false;
int kpCalls=0,kpPresses=0,kpTriggers=0,kpLastKey=-1,kpBirthdayCalls=0;
uint32_t kpDrawMask=0;
float kpRoot[3]={-1,-618,506};
void __cdecl FakeKeypadCollision(int16_t,uint8_t*,void*) { Check(false,"physical keypad must never open inventory through collision"); }
uint64_t kpDrawTime=0;float kpDrawLight[4]{};
void __cdecl FakeKeypadDraw(uint8_t* item,int32_t pass) {
    Check(pass==2,"static keypad draw preserves native render-pass argument");
    kpDrawMask=*reinterpret_cast<uint32_t*>(item+12);
    tr::KeypadLight(kpDrawLight,kpDrawTime);
}
void __cdecl FakeKeypadMatrices(uint8_t* item) {
    using namespace tr;
    const float a=Radians(reinterpret_cast<PHD_3DPOS*>(item+96)->y_rot),c=std::cos(a),s=std::sin(a);
    motiongun::Frame yaw{{{{c,0,s},{0,1,0},{-s,0,c}}},{}};
    // Actual authored frame, independently observed in the live static item.
    motiongun::Frame root{{{{-1,0,0},{0,0,1},{0,1,0}}},{kpRoot[0],kpRoot[1],kpRoot[2]}};
    motiongun::WriteRows(motiongun::Multiply(yaw,root),reinterpret_cast<float*>(item+7832));
}
void __cdecl FakeKeypadStep() {
    ++kpCalls;
    Check(!kpLeft && !kpRight && !kpUp && !kpDown && kpMenu==1 && !kpUse,"keypad isolates native menu controls");
    Check(kpCount<=4 && kpX<=2 && kpY<=3,"scoped native input is bounded");
    Check(*reinterpret_cast<int32_t*>(appMemory+2148)==0,"physical keys never enter inventory");
    kpMesh=0x1fff;for(int i=0;i<kpCount;++i) {const int n=kpDigits[i] ? kpDigits[i] : 11;kpMesh=(kpMesh&~(1u<<n))|(1u<<(n+12));}
    if(kpPause) { if(!--kpPause) {kpMenu=0;kpUse=1;}return; }
    if(!kpSelect) return;
    ++kpPresses;kpLastKey=kpY*3+kpX;
    if(kpLastKey==9) { kpCount=0;return; }
    if(kpLastKey==11) { kpPause=30;return; }
    const uint8_t digit=kpLastKey==10 ? 0 : uint8_t(kpLastKey+1);
    if(kpCount==4) return;
    for(int i=0;i<kpCount;++i) if(kpDigits[i]==digit) return;
    kpDigits[kpCount++]=digit;if(kpCount==4) {kpX=2;kpY=3;}
}
void* __cdecl FakeKeypadFloor(int32_t,int32_t,int32_t,int16_t*) { return kpTriggerWords; }
int32_t __cdecl FakeKeypadHeight(void*,int32_t x,int32_t y,int32_t z) {
    using namespace tr;
    const auto& p=*reinterpret_cast<PHD_3DPOS*>(switchMemory+96);
    Check(x==p.x_pos && y==p.y_pos && z==p.z_pos,"completion queries the keypad sector, not a neighbouring Lara sector");
    kpTriggerIndex=kpNoTrigger ? nullptr : kpTriggerWords;return y;
}
void __cdecl FakeKeypadTrigger(uint16_t* trigger,int32_t heavy,int32_t flags) {
    using namespace tr;
    ++kpTriggers;Check(trigger==kpTriggerWords && heavy==1 && flags==0,"keypad keeps native heavy trigger arguments");
    Check(actionInput==0 && *reinterpret_cast<int16_t*>(laraMemory+2)==0 && !kpCut,"trigger sees original Lara/input and native cutscene flag");
    if(kpChangeLevel) {*reinterpret_cast<int32_t*>(appMemory+1264)=99;kpCut=1;}
}
void __cdecl FakeBirthday(int32_t track,int32_t volume) {++kpBirthdayCalls;Check(track==110 && volume==100,"native keypad bonus preserved");}
}
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    auto rva=[](const void* p) {auto v=reinterpret_cast<uint64_t>(p)-g_boundBase;Check(v<=UINT32_MAX,"test RVA in module");return uint32_t(v);};
    KeypadDll table{};
    table.step=rva(reinterpret_cast<void*>(&FakeKeypadStep));table.collision=rva(reinterpret_cast<void*>(&FakeKeypadCollision));
    table.trigger=rva(reinterpret_cast<void*>(&FakeKeypadTrigger));table.triggerIndex=rva(&kpTriggerIndex);table.level=rva(&kpLevel);
    table.birthdayDisplay=rva(&kpBirthday);table.birthdayTimer=rva(&kpBirthdayTimer);table.richCut=rva(&kpRich);table.cutTrigger=rva(&kpCut);
    table.count=rva(&kpCount);table.y=rva(&kpY);table.pause=rva(&kpPause);table.x=rva(&kpX);table.digits=rva(kpDigits);
    table.combination=rva(&kpCombination);table.mesh=rva(&kpMesh);table.left=rva(&kpLeft);table.right=rva(&kpRight);
    table.up=rva(&kpUp);table.down=rva(&kpDown);table.select=rva(&kpSelect);table.menu=rva(&kpMenu);table.use=rva(&kpUse);
    g_keypadDll=&table;
    g_hKeypadDraw.m_trampoline=reinterpret_cast<void*>(&FakeKeypadDraw);
    // Nonzero external state must survive every native call, including Enter.
    kpCount=3;kpY=2;kpX=1;kpPause=17;kpLeft=1;kpRight=2;kpUp=3;kpDown=4;kpSelect=1;kpUse=1;kpMenu=0;
    kpDigits[0]=9;kpDigits[1]=8;kpDigits[2]=7;kpDigits[3]=6;kpMesh=0xaabbccdd;kpCombination=3210;
    keypad::Entry entry{};
    StepPhysicalKeypad(entry,7,8526);Check(entry.count==1 && entry.digits[0]==8,"press feeds only touched digit to native input");
    StepPhysicalKeypad(entry,7,8526);Check(entry.count==1,"native duplicate-digit rule retained");
    StepPhysicalKeypad(entry,9,8526);Check(!entry.count,"clear feeds native clear key");
    StepPhysicalKeypad(entry,11,8526);for(int i=0;i<29;++i) Check(!StepPhysicalKeypad(entry,-1,8526),"native result delay retained");
    Check(StepPhysicalKeypad(entry,-1,8526),"native result signals completion exactly once");
    Check(kpCount==3 && kpY==2 && kpX==1 && kpPause==17 && kpLeft==1 && kpRight==2 && kpUp==3 && kpDown==4 && kpSelect==1 && kpUse==1 && kpMenu==0 &&
          kpDigits[0]==9 && kpDigits[3]==6 && kpMesh==0xaabbccdd && kpCombination==3210,"all external native keypad state restored after completion");
    // Geometric rejection: no big palm box, side swipe, backwards press or
    // tracking teleport. Every actual authored key centre has a unique hit.
    for(int key=0;key<12;++key) {
        const float x=(keypad::ColumnLeft[key%3]+keypad::ColumnRight[key%3])/2;
        const float z=(keypad::RowTop[key/3]+keypad::RowBottom[key/3])/2;
        for(float scale:{300.f,500.f,800.f}) {
            motiongun::Vec a{x,-60,z},b{x,-26,z};
            Check(keypad::Touch(a,b,scale)==key,"unique key face swept contact");
            Check(keypad::Touch(b,a,scale)==-1,"withdrawal is not a press");
            Check(keypad::Touch({x,-26,z-20},b,scale)==-1,"lateral sliding cannot type");
            Check(keypad::Touch({x,-1000,z},b,scale)==-1,"tracking teleport cannot type");
            Check(keypad::Touch({x,-60,z},{x,100,z},scale)==-1,"back-of-panel penetration cannot type");
        }
    }
    Check(keypad::Touch({0,-60,90},{0,-26,90},500)==-1,"display area is not a key");
    Check(keypad::Touch({0,-60,0},{0,-26,0},NAN)==-1,"invalid scale rejected");
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);dll.input=rva(&actionInput);
    dll.objects=rva(keypadObjects);dll.getFloor=rva(reinterpret_cast<void*>(&FakeKeypadFloor));dll.getHeight=rva(reinterpret_cast<void*>(&FakeKeypadHeight));
    g_boundDll=&dll;game=dll.game=1;
    SwitchDll sw{};sw.items=rva(&switchItems);sw.count=rva(&switchCount);sw.calcMatrices=rva(reinterpret_cast<void*>(&FakeKeypadMatrices));g_switchDll=&sw;
    MotionDll motion{};motion.app=rva(&motionApp);motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;g_motionHooksReady=true;
    FILE* fixture=nullptr;fopen_s(&fixture,"build/keypad-poses.txt","r");Check(fixture!=nullptr,"offline authored keypad poses available");
    int poses[2][6]{};for(auto& row:poses) for(auto& n:row) Check(fscanf_s(fixture,"%d",&n)==1,"read asset fixture");std::fclose(fixture);
    for(int asset=0;asset<2;++asset) for(int yaw=0;yaw<4;++yaw) for(int hand=0;hand<2;++hand) {
        config=Config{};config.enabled=config.firstPerson=config.firstPersonMotionGuns=config.firstPersonUnarmedIK=true;
        config.positionalTracking=config.firstPersonHeadTranslation=true;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));std::memset(appMemory,0,sizeof(appMemory));
        std::memset(switchMemory,0,sizeof(switchMemory));std::memset(keypadObjects,0,sizeof(keypadObjects));
        *reinterpret_cast<int32_t*>(appMemory+1264)=11+asset;kpLevel=uint8_t(11+asset);appMemory[0x9e4]=1;
        *reinterpret_cast<int16_t*>(itemMemory+18)=2;*reinterpret_cast<int16_t*>(itemMemory+24)=103;*reinterpret_cast<int16_t*>(itemMemory+38)=1000;
        *reinterpret_cast<int16_t*>(keypadObjects)=15;
        for(int j=0;j<15;++j) {auto* b=reinterpret_cast<float*>(keypadObjects+1676+j*64);b[0]=b[5]=b[10]=b[15]=1;}
        auto* object=keypadObjects+249*3792;*reinterpret_cast<int16_t*>(object)=25;
        *reinterpret_cast<Fn_SwitchCollision*>(object+56)=&FakeKeypadCollision;*reinterpret_cast<const char**>(object+112)="PUZZLE_HOLE8_1";
        auto* b=reinterpret_cast<float*>(object+1676);b[0]=b[5]=b[10]=b[15]=1;
        *reinterpret_cast<int16_t*>(switchMemory+16)=249;*reinterpret_cast<int16_t*>(switchMemory+48)=8526;
        *reinterpret_cast<int16_t*>(switchMemory+24)=int16_t(poses[asset][0]);*reinterpret_cast<int16_t*>(switchMemory+26)=int16_t(poses[asset][1]);
        for(int j=0;j<3;++j) kpRoot[j]=float(poses[asset][j+2]);
        *reinterpret_cast<uint32_t*>(switchMemory+12)=0x1fff;
        g_active=g_runtimeEnabled=g_haveHeading=true;g_headingBase=g_lastHeadWorld=0;g_headingItem=itemMemory;water=0;actionInput=0;
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(0);FirstPersonRecenter();
        g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        VR().m_rawControllerPose[0]=VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
        VR().m_rawControllerPose[hand].m[1][3]-=.2f;VR().m_rawControllerPose[1-hand].m[0][3]-=1.f;
        Check(UnarmedIKReady(),"rendered controller hands ready");
        motiongun::Vec initialHand{};Check(KeypadFinger(hand,initialHand),"rendered fingertip contact available");
        auto& pos=*reinterpret_cast<PHD_3DPOS*>(switchMemory+96);pos.y_rot=int16_t(yaw*16384);
        motiongun::Frame inverse{},world{};Check(KeypadFrame(switchMemory,object,inverse) && motiongun::Inverse(inverse,world),"authored keypad world transform");
        const auto initial=motiongun::Transform(world,{28.5f,-60,-40.8f});
        pos.x_pos=int32_t(std::lround(initialHand.x-initial.x));pos.y_pos=int32_t(std::lround(initialHand.y-initial.y));pos.z_pos=int32_t(std::lround(initialHand.z-initial.z));
        *reinterpret_cast<PHD_3DPOS*>(itemMemory+96)=pos;
        Check(KeypadFrame(switchMemory,object,inverse)&&motiongun::Inverse(inverse,world),"rotated and translated keypad transform");
        const auto originalController=VR().m_rawControllerPose[hand];const float scale=LiveWorldUnitsPerMetre();
        auto setHand=[&](int key,float y) {
            const float x=(keypad::ColumnLeft[key%3]+keypad::ColumnRight[key%3])/2,z=(keypad::RowTop[key/3]+keypad::RowBottom[key/3])/2;
            const auto delta=Sub(motiongun::Transform(world,{x,y,z}),initialHand);VR().m_rawControllerPose[hand]=originalController;
            VR().m_rawControllerPose[hand].m[0][3]+=delta.x/scale;VR().m_rawControllerPose[hand].m[1][3]-=delta.y/scale;VR().m_rawControllerPose[hand].m[2][3]-=delta.z/scale;
        };
        uint64_t now=1000;auto tick=[&](int key,float y) {setHand(key,y);return AdvancePhysicalKeypad(itemMemory,now+=34);};
        auto press=[&](int key) {tick(key,-60);tick(key,-60);tick(key,-26);};
        kpPresses=kpTriggers=0;kpNoTrigger=kpChangeLevel=false;switchSightBlocked=false;
        tick(0,-26);Check(!kpPresses,"tracking starts on face without pressing");
        tick(0,-60);switchSightBlocked=true;tick(0,-26);switchSightBlocked=false;Check(!kpPresses,"wall blocks finger contact");
        for(int key=0;key<12;++key) {
            ResetPhysicalKeypad();press(key);Check(kpLastKey==key,"rendered finger selects exact key for either hand and any panel yaw");
            kpDrawTime=now;Detour_KeypadDraw(switchMemory,2);
            Check(kpDrawLight[0]==key+1 && kpDrawLight[1]==key+13 && kpDrawLight[2]==1,"actual touch lights the correct raised and depressed key, including Clear/Enter");
            float outside[4]={1,1,1,1};Check(!KeypadLight(outside,now) && outside[2]==0,"key light cannot escape the panel draw");
            kpDrawTime=now+250;Detour_KeypadDraw(switchMemory,2);Check(kpDrawLight[2]==.5f,"pressed-key illumination fades smoothly");
            kpDrawTime=now+350;Detour_KeypadDraw(switchMemory,2);Check(kpDrawLight[2]==0,"pressed-key illumination expires");
            kpDrawTime=now-1;Detour_KeypadDraw(switchMemory,2);Check(kpDrawLight[2]==0,"clock rollback cannot leave a key lit");
            kpDrawTime=now;Detour_KeypadDraw(itemMemory,2);Check(kpDrawLight[2]==0,"another item cannot inherit keypad illumination");
            {int32_t use=0;KeypadMeshScope outer(switchMemory,use);
             Check(KeypadLight(outside,now),"creature path receives the same button light");
             {KeypadMeshScope inner(itemMemory,use);Check(!KeypadLight(outside,now),"nested unrelated drawing clears the light");}
             Check(KeypadLight(outside,now),"nested draw restores the outer keypad light");}
            const int before=kpPresses;
            for(int i=0;i<5;++i) tick(key,-26);
            Check(kpPresses==before,"held finger does not repeat");tick((key+1)%12,-26);Check(kpPresses==before,"side swipe cannot press adjacent key");
        }
        ResetPhysicalKeypad();press(0);Check(g_keypad.entry.count==1,"physical entry contains first digit");
        tick(0,-60); {int32_t use=0;KeypadMeshScope scope(switchMemory,use);Check(use==1 && *reinterpret_cast<uint32_t*>(switchMemory+12)!=0x1fff,"pressed key visible in world only");}
        Check(*reinterpret_cast<uint32_t*>(switchMemory+12)==0x1fff,"render scope restores simulation mesh bits");
        Detour_KeypadDraw(switchMemory,2);Check(kpDrawMask==g_keypad.entry.mesh && kpDrawMask!=0x1fff,"static keypad draw receives pressed-key mask");
        Check(*reinterpret_cast<uint32_t*>(switchMemory+12)==0x1fff,"static draw leaves native item mask unchanged");
        *reinterpret_cast<int32_t*>(appMemory+2148)=1;Detour_KeypadDraw(switchMemory,2);Check(kpDrawMask==0x1fff && kpDrawLight[2]==0,"ordinary inventory preserves native draw mask and has no added light");
        *reinterpret_cast<int32_t*>(appMemory+2148)=0;
        for(int mode=0;mode<6;++mode) {
            ResetPhysicalKeypad();tick(0,-60);const int before=kpPresses;
            if(mode==0)g_active=false;if(mode==1)appMemory[0x9e4]=0;if(mode==2)*reinterpret_cast<int16_t*>(laraMemory+2)=4;
            if(mode==3)g_cogClaimed=true;if(mode==4)VR().m_controllerPoseValid[hand]=false;if(mode==5)*reinterpret_cast<int32_t*>(appMemory+2148)=1;
            tick(0,-26);Check(kpPresses==before,"inactive view/graphics/weapon/ownership/tracking/menu gate prevents press");
            g_active=true;appMemory[0x9e4]=1;*reinterpret_cast<int16_t*>(laraMemory+2)=0;g_cogClaimed=false;VR().m_controllerPoseValid[hand]=true;*reinterpret_cast<int32_t*>(appMemory+2148)=0;
        }
        ResetPhysicalKeypad();tick(0,-60);const int before=kpPresses;now+=300;tick(0,-26);Check(kpPresses==before,"stale fingertip sample cannot press");
        ResetPhysicalKeypad();press(0);press(1);press(2);press(3);press(11);
        for(int i=0;i<30;++i)tick(11,-60);
        Check(kpTriggers==0 && g_keypad.entry.count==0 && !g_keypad.submitted,"wrong code resets without triggering door");
        press(7);press(4);press(1);press(5);press(11);
        kpRich=1;kpCut=0;kpChangeLevel=(asset==1);kpTriggerIndex=kpOtherWords;
        for(int i=0;i<30;++i)tick(11,-60);
        Check(kpTriggers==1,"correct four-digit entry dispatches trigger once");
        Check(kpTriggerIndex==kpOtherWords,"floor query does not leak native trigger pointer");
        Check(kpChangeLevel ? kpRich==1 : kpRich==0,"native cutscene bookkeeping preserved");
        for(int i=0;i<8;++i)tick(11,-26);Check(kpTriggers==1,"held Enter cannot repeat successful trigger");
        *reinterpret_cast<int32_t*>(appMemory+1264)=11+asset;ResetPhysicalKeypad();kpChangeLevel=false;
        keypad::Entry complete{};complete.count=4;complete.digits[0]=8;complete.digits[1]=5;complete.digits[2]=2;complete.digits[3]=6;
        kpNoTrigger=true;FinishPhysicalKeypad(switchMemory,complete,8526);Check(kpTriggers==1,"missing floor cannot activate any trigger");kpNoTrigger=false;
        kpTriggerWords[0]=0x8004;FinishPhysicalKeypad(switchMemory,complete,8526);Check(kpTriggers==1,"ordinary adjacent trigger cannot be activated");kpTriggerWords[0]=0x8504;
        ResetPhysicalKeypad();press(0);*reinterpret_cast<int16_t*>(switchMemory+48)=1234;kpDrawTime=now;Detour_KeypadDraw(switchMemory,2);Check(kpDrawLight[2]==0,"reused item identity cannot inherit illumination");tick(0,-60);Check(g_keypad.index==-1,"changed item identity drops old session");
    }
    kpLevel=12;kpBirthday=0;kpBirthdayTimer=99;kpBirthdayCalls=0;
    *reinterpret_cast<void (__cdecl**)(int32_t,int32_t)>(appMemory+0x4b0)=&FakeBirthday;
    keypad::Entry birthday{};birthday.count=3;birthday.digits[0]=1;birthday.digits[1]=4;birthday.digits[2]=2;
    FinishPhysicalKeypad(switchMemory,birthday,8526);Check(kpBirthdayCalls==1 && kpBirthday==1 && !kpBirthdayTimer,"native optional keypad code remains functional");
    std::printf("PASS: %d focused physical keypad checks\n",checks);
}
