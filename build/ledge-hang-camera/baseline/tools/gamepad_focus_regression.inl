namespace {
int32_t padNativeActive=0;
int padMessageState=0,padMessageCalls=0,padPollCalls=0;
int padNextMessageState=-1;
tr::VRSystem::HandState padFocusHands[2]{};
tr::XState padFocusResult{};
bool padDoublePump=false;
void __cdecl FakePadMessages() {
    Check(padNativeActive==padMessageState,"native message pump sees actual desktop activation state");
    ++padMessageCalls;
    if(padNextMessageState>=0) {
        padNativeActive=padMessageState=padNextMessageState;
        padNextMessageState=-1;
    }
}
void __cdecl FakeNativePadUpdate() {
    tr::Detour_PadMessages();
    if(padDoublePump) {
        // Another pump must not see the temporary controller-only override.
        tr::Detour_PadMessages();
    }
    // Reproduce the verified native inputUpdate gate and controller path.
    if(!padNativeActive) return;
    ++padPollCalls;
    bool shifted=false,action=false;
    tr::BuildStateFromHands(padFocusHands,padFocusResult,shifted,false,action);
}
void TestGamepadFocus() {
    using namespace tr;
    const auto oldConfig=config;const auto oldVR=VR();const int oldGame=game;
    const bool oldActive=g_active;
    auto* oldFlag=g_padWindowActive;
    const auto oldUpdate=g_hPadUpdate.m_trampoline,oldMessages=g_hPadMessages.m_trampoline;
    g_padWindowActive=&padNativeActive;
    g_hPadUpdate.m_trampoline=reinterpret_cast<void*>(&FakeNativePadUpdate);
    g_hPadMessages.m_trampoline=reinterpret_cast<void*>(&FakePadMessages);
    g_active=false;config.firstPerson=false;config.gamepadMenuUsesBack=true;
    for(int which:{-1,0,1,2}) for(bool vr:{false,true}) for(bool enabled:{false,true})
    for(bool pad:{false,true}) for(int focused:{0,1}) {
        game=which;VR().m_system=vr ? reinterpret_cast<vr::IVRSystem*>(1) : nullptr;config.enabled=enabled;config.gamepadEnabled=pad;
        padNativeActive=padMessageState=focused;
        padFocusHands[0]={};padFocusHands[1]={};padFocusHands[1].btnLower=true;
        padPollCalls=padMessageCalls=0;padDoublePump=true;
        Detour_PadUpdate();
        const bool expected=focused || (vr && enabled && pad && (which==0 || which==1));
        Check(padPollCalls==int(expected) && padMessageCalls==2,
              "only active TR4/5 VR bypasses native background pad gate; disabled/non-VR/TR6 retain native behavior");
        Check(padNativeActive==focused && !g_padFocusScope,
              "native desktop activation state is restored after every controller poll");
    }
    config.enabled=config.gamepadEnabled=true;VR().m_system=reinterpret_cast<vr::IVRSystem*>(1);game=1;
    padNativeActive=padMessageState=0;padDoublePump=false;
    // Calling the message pump from anywhere else must never fake activation.
    Detour_PadMessages();Check(!padNativeActive,"ordinary window message processing retains background state");
    for(int activation:{1,0}) {
        padNextMessageState=activation;Detour_PadUpdate();
        Check(padNativeActive==activation,"activation changes delivered during the native pump survive scope restoration");
    }
    for(int button=0;button<8;++button) {
        const uint16_t expected[]={XB_A,XB_B,XB_Y,XB_BACK,XB_LEFT_SHOULDER,XB_X,XB_LEFT_THUMB,XB_RIGHT_THUMB};
        padFocusHands[0]={};padFocusHands[1]={};
        if(button==0) padFocusHands[1].btnLower=true;
        if(button==1) padFocusHands[1].btnUpper=true;
        if(button==2) padFocusHands[0].btnUpper=true;
        if(button==3) padFocusHands[0].btnLower=true;
        if(button==4) padFocusHands[0].grip=1;
        if(button==5) padFocusHands[1].grip=1;
        if(button==6) padFocusHands[0].stickClick=true;
        if(button==7) padFocusHands[1].stickClick=true;
        for(int frame=0;frame<3;++frame) {
            padNativeActive=padMessageState=frame==1 ? 1 : 0;
            Detour_PadUpdate();
            Check(padFocusResult.Gamepad.wButtons==expected[button],
                  "jump roll action menu grips and stick clicks survive loss and regain of desktop focus");
        }
        padFocusHands[0]={};padFocusHands[1]={};Detour_PadUpdate();
        Check(!padFocusResult.Gamepad.wButtons && !padNativeActive,
              "button release is polled while unfocused and cannot leave a held action stuck");
    }
    g_hPadUpdate.m_trampoline=oldUpdate;g_hPadMessages.m_trampoline=oldMessages;
    g_padWindowActive=oldFlag;config=oldConfig;VR()=oldVR;game=oldGame;g_active=oldActive;
}
}
