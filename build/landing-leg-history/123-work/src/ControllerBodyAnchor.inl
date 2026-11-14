firstperson::ControllerBodyAnchor g_controllerBodyAnchor;
void ResetControllerBodyAnchor() {
    if(g_controllerBodyAnchor.valid && Cfg().firstPersonDriftLog) Log("body-input-reset");
    g_controllerBodyAnchor.Reset();
}
void UpdateControllerBodyAnchor() {
    vr::HmdMatrix34_t head{},left{},right{};
    const bool tracked=Cfg().firstPersonFullBodyIK && Cfg().firstPersonMovementStabilization &&
        g_active && g_haveHeading && VR().HeadPose(head) &&
        VR().ControllerPose(0,left) && VR().ControllerPose(1,right);
    const bool walking=g_haveManualInput && !g_shifted && locomotion::Length(g_manualLocal)>.05f;
    const auto frame=VR().PoseSerial();const double time=TurnTime();
    const float headYaw=VR().HeadYawRadians();
    const float bodyYaw=g_bodyFollower.valid ? g_bodyFollower.TrackingYaw() :
        g_controllerBodyAnchor.valid ? g_controllerBodyAnchor.yaw : headYaw;
    const bool trace=Cfg().firstPersonDriftLog &&
        ((tracked && (!g_controllerBodyAnchor.valid || g_controllerBodyAnchor.serial!=frame || g_controllerBodyAnchor.base!=g_heading.base)) ||
         (!tracked && g_controllerBodyAnchor.valid));
    g_controllerBodyAnchor.Update(frame,time,
        {head.m[0][3],head.m[1][3],-head.m[2][3]},
        {left.m[0][3],left.m[1][3],-left.m[2][3]},
        {right.m[0][3],right.m[1][3],-right.m[2][3]},g_heading.base,
        bodyYaw,tracked,walking,headYaw,Cfg().firstPersonRoomscaleNeckMetres,{-head.m[0][2],-head.m[1][2],head.m[2][2]});
    if(trace) LogF("body-input: %llu %.17g %.9g %.9g %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g -> %.9g %.9g %d %d %d",
        frame,time,g_heading.base,bodyYaw,int(tracked),int(walking),headYaw,Cfg().firstPersonRoomscaleNeckMetres,
        head.m[0][3],head.m[1][3],-head.m[2][3],left.m[0][3],left.m[1][3],-left.m[2][3],
        right.m[0][3],right.m[1][3],-right.m[2][3],-head.m[0][2],-head.m[1][2],head.m[2][2],
        g_controllerBodyAnchor.head.x,g_controllerBodyAnchor.head.z,int(g_controllerBodyAnchor.held),
        int(g_controllerBodyAnchor.rotating),int(g_controllerBodyAnchor.translating));
    if(trace && tracked) {
        // Keep the existing replay input unchanged. These supplementary raw
        // tracking-space bases distinguish hand aim from rotation of the line
        // joining the controllers; positions alone cannot measure that difference.
        const auto basis=[frame](const char* device,const vr::HmdMatrix34_t& pose) {
            LogF("body-basis: %llu %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",
                frame,device,pose.m[0][0],pose.m[0][1],pose.m[0][2],
                pose.m[1][0],pose.m[1][1],pose.m[1][2],pose.m[2][0],pose.m[2][1],pose.m[2][2]);
        };
        basis("head",head);basis("left",left);basis("right",right);
        LogF("body-height: %llu %.9g %.9g",frame,
            head.m[1][3]+VR().FirstPersonHeightDrop(),VR().FirstPersonPostureDrop());
    }
    if(Cfg().firstPersonDriftLog) {
        static uint64_t next=0;static bool lastHeld=false;
        const auto now=GetTickCount64();
        if(now>=next || lastHeld!=g_controllerBodyAnchor.held) {
            next=now+1000;lastHeld=g_controllerBodyAnchor.held;
            LogF("body-anchor: held=%d rotating=%d tracked=%d walking=%d reason=%d hand-travel=(%.1f,%.1f)mm head-only=(%.1f,%.1f)mm yaw=(body %.1f head %.1f)deg turn=%.2fdeg pending=%.2fdeg recovery=%d target=%.1fdeg pair=%.0fmm",
                g_controllerBodyAnchor.held,g_controllerBodyAnchor.rotating,tracked,walking,g_controllerBodyAnchor.reason,
                g_controllerBodyAnchor.handTravel[0]*1000,g_controllerBodyAnchor.handTravel[1]*1000,
                g_controllerBodyAnchor.offset.x*1000,g_controllerBodyAnchor.offset.z*1000,
                g_bodyFollower.TrackingYaw()*57.2957795f,VR().HeadYawRadians()*57.2957795f,
                g_controllerBodyAnchor.turnOutput*57.2957795f,g_bodyFollower.pendingPhysicalTurn*57.2957795f,
                g_controllerBodyAnchor.heading.active,g_controllerBodyAnchor.heading.target*57.2957795f,
                std::hypot(left.m[0][3]-right.m[0][3],left.m[2][3]-right.m[2][3])*1000);
        }
    }
}
bool BodyControllersStationary() { UpdateControllerBodyAnchor();return g_controllerBodyAnchor.held; }
bool HasControllerBodyAnchor() { UpdateControllerBodyAnchor();return g_controllerBodyAnchor.valid; }
bool BodyControllersPermitTravel() { UpdateControllerBodyAnchor();return !g_controllerBodyAnchor.valid || g_controllerBodyAnchor.translating; }
void BodyTrackingOffset(float& right,float& forward) {
    UpdateControllerBodyAnchor();VR().FirstPersonViewOffset(right,forward);
    right-=g_controllerBodyAnchor.offset.x;forward-=g_controllerBodyAnchor.offset.z;
}
