// TR4 CROWBAR_SWITCH: keep native inventory/alignment, insertion, effects and
// removal. Only the inserted tool's pry interval waits for the right hand.
// No synthetic Action, trigger, mesh swap or animation-frame writes.
struct CrowbarDll { uint32_t timestamp,collision,control; };
constexpr CrowbarDll kCrowbarDlls[]={
    {0x696B4999,0x8CEA0,0x8B760}, {0x68C12FDA,0x8D8B0,0x8C170},
};
const CrowbarDll* g_crowbarDll=nullptr;
hook::InlineHook g_hCrowbarCollision,g_hCrowbarAnimate;
constexpr int CrowbarInserted=46,CrowbarPried=58;
struct PhysicalCrowbar {
    SwitchCandidate identity{};
    uint8_t* lara=nullptr;
    PHD_3DPOS body{},socket{},eye{};
    int16_t first=0,targetAnimation=0,targetFirst=0,lastFrame=0;
    float progress=0,gripProgress=0,gripHeight=0;
    bool active=false,claimed=false,down=false,actionReleased=false,finishing=false;
    uint64_t trackingLost=0;
} g_crowbar;

void ResetPhysicalCrowbar() { g_crowbar={}; }

bool CrowbarObject(const uint8_t* item) {
    if(!item || !g_crowbarDll || !g_switchDll || !g_boundDll || g_boundDll->game!=0 ||
       !g_boundDll->objects || *reinterpret_cast<const int16_t*>(item+16)!=320) return false;
    const auto* o=Ptr<uint8_t>(g_boundDll->objects)+320*off::object_stride;
    const auto* name=*reinterpret_cast<const char* const*>(o+off::object_geom);
    return *reinterpret_cast<const int16_t*>(o)==1 &&
        *reinterpret_cast<const uint64_t*>(o+56)==g_boundBase+g_crowbarDll->collision &&
        *reinterpret_cast<const uint64_t*>(o+24)==g_boundBase+g_crowbarDll->control &&
        !*reinterpret_cast<const int32_t*>(o+off::object_geom+28) && name &&
        (!std::strcmp(name,"CROWBAR_SWITCH") || !std::strcmp(name,"CROWBAR_SWITCH_HIGHSTRT"));
}

// Structural ownership only: Gate and controller readiness call this too.
bool PhysicalCrowbarOwns(const uint8_t* lara) {
    if(!g_crowbar.active || !g_boundDll || !g_crowbarDll ||
       GameDllBound()!=g_boundDll || GameDllBase()!=g_boundBase ||
       !lara || lara!=g_crowbar.lara || lara!=*Ptr<uint8_t*>(g_boundDll->laraItem)) return false;
    auto* target=SwitchItem(g_crowbar.identity);
    if(!CrowbarObject(target) || std::memcmp(target+off::item_pos,&g_crowbar.socket,sizeof(PHD_3DPOS)) ||
       std::memcmp(lara+off::item_pos,&g_crowbar.body,sizeof(PHD_3DPOS)) ||
       *reinterpret_cast<const int16_t*>(lara+18)!=40 || *reinterpret_cast<const int16_t*>(lara+24)!=420 ||
       *reinterpret_cast<const int16_t*>(lara+off::item_hit_points)<=0 ||
       (*reinterpret_cast<const uint32_t*>(lara+0x1820)&8) ||
       *Ptr<int16_t>(g_boundDll->lara+2)!=1 || LaraWaterStatus()!=0) return false;
    const int frame=*reinterpret_cast<const int16_t*>(lara+26)-g_crowbar.first;
    const int socketFrame=*reinterpret_cast<const int16_t*>(target+26)-g_crowbar.targetFirst;
    return frame>=g_crowbar.lastFrame && frame<=g_crowbar.lastFrame+1 && frame<=118 &&
        socketFrame>=0 && socketFrame<=118 && socketFrame<=frame+1 &&
        *reinterpret_cast<const int16_t*>(target+24)==g_crowbar.targetAnimation &&
        (*reinterpret_cast<const uint32_t*>(target+0x1820)&7)==3;
}

bool CrowbarProfile(uint8_t* target,uint8_t* lara) {
    if(!CrowbarObject(target) || *reinterpret_cast<int16_t*>(lara+24)!=420 ||
       *reinterpret_cast<int16_t*>(lara+18)!=40 || (*reinterpret_cast<uint32_t*>(lara+0x1820)&8) ||
       !*Ptr<uint8_t>(g_boundDll->lara+0x169)) return false;
    const auto* a=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
    const auto* commands=*Ptr<int16_t*>(g_switchDll->commands);
    if(!a || !commands) return false;
    const auto& use=a[420];
    // These exact commands are shared by TRAIN/HIGHSTRT/JOBY4A. In particular,
    // neither held endpoint repeats SwapCrowbar; the native tick executes each
    // effect once. Animation 436 (a failed pry) never enters this path.
    const int16_t expected[]={6,12487,8,6,12585,8,5,12516,16384,5,12569,16384,
        5,12549,16431,5,12542,16722,6,12516,16416,6,12569,16416,3};
    if(!use.frames || use.interpolation!=(38<<8|1) || use.state!=40 || use.first!=12475 ||
       use.last!=12593 || use.jump!=11 || use.jumpFrame!=184 || use.changeCount ||
       use.velocity || use.acceleration || use.xVelocity || use.xAcceleration ||
       use.commandCount!=9 || use.commandIndex<0 ||
       std::memcmp(commands+use.commandIndex,expected,sizeof(expected)) ||
       *reinterpret_cast<int16_t*>(lara+26)!=use.first) return false;
    const int base=*reinterpret_cast<const int16_t*>(Ptr<uint8_t>(g_boundDll->objects)+320*off::object_stride+66);
    const int anim=*reinterpret_cast<int16_t*>(target+24);
    if(base<0 || (anim!=base+2 && anim!=base+3)) return false;
    const auto& socket=a[anim];
    return socket.frames && socket.state==2 && socket.last-socket.first==118 &&
        socket.interpolation==(11<<8|4) && socket.first==(anim==base+2 ? 2 : 121) &&
        socket.jump==(anim==base+2 ? base+1 : base) && socket.jumpFrame==(anim==base+2 ? 1 : 0) &&
        !socket.velocity && !socket.acceleration && !socket.xVelocity && !socket.xAcceleration &&
        !socket.changeCount && socket.commandCount==1 && socket.commandIndex>=0 &&
        commands[socket.commandIndex]==4 && *reinterpret_cast<int16_t*>(target+26)==socket.first &&
        (*reinterpret_cast<uint32_t*>(target+0x1820)&7)==3;
}

bool CrowbarWrist(motiongun::Frame& wrist,motiongun::Vec& palm) {
    if(!PhysicalCrowbarOwns(g_crowbar.lara) || !SwitchPalmLocal(palm)) return false;
    const auto origin=JointPoint(g_crowbar.lara,10,0,0,0);
    const motiongun::Vec axes[]={Sub(JointPoint(g_crowbar.lara,10,1024,0,0),origin),
        Sub(JointPoint(g_crowbar.lara,10,0,1024,0),origin),Sub(JointPoint(g_crowbar.lara,10,0,0,1024),origin)};
    wrist.origin=origin;
    for(int col=0;col<3;++col) {
        wrist.basis.r[0][col]=axes[col].x/1024;
        wrist.basis.r[1][col]=axes[col].y/1024;
        wrist.basis.r[2][col]=axes[col].z/1024;
    }
    return firstperson::Finite(wrist);
}

void AttachPhysicalCrowbarHand(int hand,motiongun::Frame& wrist) {
    if(hand!=1 || !PhysicalCrowbarOwns(g_crowbar.lara)) return;
    motiongun::Frame native{};motiongun::Vec palm{};
    // The temporary hand mesh includes the tool. Keep its authored wrist and
    // orientation at the socket, rather than moving the crowbar out of it with
    // free-hand IK. The left hand can continue to follow its controller.
    if(CrowbarWrist(native,palm)) wrist=native;
}

void ReleasePhysicalCrowbarHand() {
    if(g_crowbar.claimed && PhysicalCrowbarOwns(g_crowbar.lara)) {
        const int frame=*reinterpret_cast<int16_t*>(g_crowbar.lara+26)-g_crowbar.first;
        g_crowbar.progress=std::clamp(float(frame-CrowbarInserted)/(CrowbarPried-CrowbarInserted),0.f,1.f);
    }
    g_crowbar.claimed=false;
}

bool UpdatePhysicalCrowbarGrip(bool down,bool allowed,uint64_t now) {
    if(!PhysicalCrowbarOwns(g_crowbar.lara)) { ResetPhysicalCrowbar();return false; }
    const bool rising=down && !g_crowbar.down;g_crowbar.down=down;
    const int frame=*reinterpret_cast<int16_t*>(g_crowbar.lara+26)-g_crowbar.first;
    motiongun::Vec hand{},palm{};motiongun::Frame wrist{};float handY=0,headY=0;
    if(!down || !allowed || !g_active || !Gate() || g_crowbar.finishing ||
       frame<CrowbarInserted || frame>=CrowbarPried || !SwitchHand(hand,handY,headY) || !CrowbarWrist(wrist,palm)) {
        ReleasePhysicalCrowbarHand();return false;
    }
    const auto grip=motiongun::Transform(wrist,palm);
    const float distance=firstperson::Length(Sub(hand,grip));
    if(rising && distance<std::min(100.f,LiveWorldUnitsPerMetre()*.20f)) {
        g_crowbar.claimed=true;g_crowbar.gripHeight=handY;g_crowbar.gripProgress=g_crowbar.progress;
        LogF("vr-crowbar: grabbed inserted tool item=%d",g_crowbar.identity.index);
    }
    if(g_crowbar.claimed) {
        if(distance>LiveWorldUnitsPerMetre()*.8f) ReleasePhysicalCrowbarHand();
        else {
            // Raw tracked height makes camera turns/movement unable to pry.
            // Eight centimetres is a comfortable downstroke for this short arc.
            const float wanted=g_crowbar.gripProgress+(g_crowbar.gripHeight-handY)/.08f;
            g_crowbar.progress=std::clamp(std::max(g_crowbar.progress,wanted),0.f,1.f);
        }
    }
    return g_crowbar.claimed;
}

bool HoldPhysicalCrowbarLara(uint8_t* item) {
    if(!PhysicalCrowbarOwns(item)) { if(g_crowbar.active) ResetPhysicalCrowbar();return false; }
    // Mode changes, scripts, tracking loss, or explicit native Action return
    // control to the original animation at its current frame. Removal/hand
    // cleanup then runs normally; never toggle the tool mesh ourselves.
    if(!Gate() || !Cfg().positionalTracking || !Cfg().firstPersonHeadTranslation) {
        ResetPhysicalCrowbar();return false;
    }
    if(!(*Ptr<uint64_t>(g_boundDll->input)&0x40)) g_crowbar.actionReleased=true;
    else if(g_crowbar.actionReleased) g_crowbar.finishing=true;
    motiongun::Vec hand{};float handY=0,headY=0;
    const uint64_t now=GetTickCount64();
    if(SwitchHand(hand,handY,headY)) g_crowbar.trackingLost=0;
    else {
        ReleasePhysicalCrowbarHand();
        if(!g_crowbar.trackingLost) g_crowbar.trackingLost=now;
        if(now-g_crowbar.trackingLost>=500) g_crowbar.finishing=true;
    }
    const int frame=*reinterpret_cast<int16_t*>(item+26)-g_crowbar.first;
    g_crowbar.lastFrame=int16_t(frame);
    if(frame>=CrowbarPried) { g_crowbar.finishing=true;g_crowbar.claimed=false; }
    if(g_crowbar.finishing || frame<CrowbarInserted) return false;
    const int wanted=CrowbarInserted+int(std::floor(g_crowbar.progress*(CrowbarPried-CrowbarInserted)+.0001f));
    // On release retain the actual pose, without spending buffered movement.
    return !g_crowbar.claimed || frame>=wanted;
}

void PhysicalCrowbarCamera(const uint8_t* item,PHD_3DPOS& pose) {
    if(!PhysicalCrowbarOwns(item)) return;
    // Keep the pre-insertion eye; the authored bending/prying animation must
    // not jerk the HMD. Headset position/rotation still apply in the VR view.
    pose.x_pos=g_crowbar.eye.x_pos;pose.y_pos=g_crowbar.eye.y_pos;pose.z_pos=g_crowbar.eye.z_pos;
}

void __cdecl Detour_CrowbarAnimateItem(uint8_t* item) {
    if(PhysicalCrowbarOwns(g_crowbar.lara) && item==SwitchItem(g_crowbar.identity)) {
        const int laraFrame=*reinterpret_cast<int16_t*>(g_crowbar.lara+26)-g_crowbar.first;
        const int socketFrame=*reinterpret_cast<int16_t*>(item+26)-g_crowbar.targetFirst;
        // SwitchControl still runs once every normal tick (including timers).
        // Only its animation waits for Lara, so command 4 cannot fire early.
        if(socketFrame>=laraFrame) return;
    }
    g_hCrowbarAnimate.Original<Fn_SwitchItem>()(item);
}

void __cdecl Detour_CrowbarCollision(int16_t index,uint8_t* lara,void* collision) {
    const bool eligible=g_crowbarDll && g_switchDll && g_active && g_scenePoseValid && Gate() &&
        Cfg().positionalTracking && Cfg().firstPersonHeadTranslation && lara && lara==g_headingItem &&
        GameDllBound()==g_boundDll && GameDllBase()==g_boundBase && LaraWaterStatus()==0 &&
        *reinterpret_cast<int16_t*>(lara+18)==2 && *reinterpret_cast<int16_t*>(lara+24)==103 &&
        (*Ptr<int16_t>(g_boundDll->lara+2)==0 ||
         ((*Ptr<uint32_t>(g_boundDll->lara+off::lara_movement_flags)&0x20) &&
          *Ptr<int16_t>(g_boundDll->lara+0x15C)==index)) &&
        !g_switchGripClaimed && !g_cogClaimed && !g_pulleyClaimed && !g_containerClaimed;
    SwitchCandidate identity{index,-1,AppState(drva::app_off::level),
        g_switchDll ? *Ptr<uint8_t*>(g_switchDll->items) : nullptr,0,0};
    motiongun::Vec hand{};float handY=0,headY=0;
    const bool tracked=eligible && SwitchHand(hand,handY,headY);
    // Never synthesize inventory selection, bypass alignment, or reroll the
    // native failure branch. Capture only an actual successful native start.
    g_hCrowbarCollision.Original<Fn_SwitchCollision>()(index,lara,collision);
    auto* target=tracked ? SwitchItem(identity) : nullptr;
    if(!target || !CrowbarProfile(target,lara)) return;
    ResetPhysicalCrowbar();g_crowbar.active=true;g_crowbar.identity=identity;g_crowbar.lara=lara;
    g_crowbar.body=*reinterpret_cast<PHD_3DPOS*>(lara+off::item_pos);
    g_crowbar.socket=*reinterpret_cast<PHD_3DPOS*>(target+off::item_pos);g_crowbar.eye=g_scenePose;
    const auto* a=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
    g_crowbar.first=a[420].first;g_crowbar.targetAnimation=*reinterpret_cast<int16_t*>(target+24);
    g_crowbar.targetFirst=a[g_crowbar.targetAnimation].first;
    LogF("vr-crowbar: native insertion captured item=%d; RG pry waits at frame %d",index,CrowbarInserted);
}

void RemovePhysicalCrowbar() {
    ResetPhysicalCrowbar();g_hCrowbarCollision.Remove();g_hCrowbarAnimate.Remove();g_crowbarDll=nullptr;
}
void InstallPhysicalCrowbar(const GameDllLayout& d,uint64_t base) {
    if(d.game!=0 || !g_switchDll) return;
    for(const auto& row:kCrowbarDlls) if(row.timestamp==d.timestamp) g_crowbarDll=&row;
    if(!g_crowbarDll) return;
    const uint8_t collision[]={0x48,0x89,0x5c,0x24,0x10};
    const uint8_t animate[]={0x48,0x8b,0xc4,0x53,0x55};
    if(!g_hCrowbarCollision.Install(reinterpret_cast<void*>(base+g_crowbarDll->collision),
           reinterpret_cast<void*>(&Detour_CrowbarCollision),sizeof(collision),collision,sizeof(collision),"VRCrowbarCollision") ||
       !g_hCrowbarAnimate.Install(reinterpret_cast<void*>(base+g_switchDll->animate),
           reinterpret_cast<void*>(&Detour_CrowbarAnimateItem),sizeof(animate),animate,sizeof(animate),"VRCrowbarAnimate")) {
        RemovePhysicalCrowbar();Log("vr-crowbar: signature mismatch; native use retained");return;
    }
    Log("vr-crowbar: TR4 native insertion followed by physical RG pry enabled");
}
