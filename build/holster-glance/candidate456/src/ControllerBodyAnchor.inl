firstperson::ControllerBodyAnchor g_controllerBodyAnchor;
void ResetControllerBodyAnchor() { g_controllerBodyAnchor.Reset(); }
void UpdateControllerBodyAnchor() {
    vr::HmdMatrix34_t head{},left{},right{};
    const bool headTracked=Cfg().firstPersonFullBodyIK && Cfg().firstPersonMovementStabilization &&
        g_active && g_haveHeading && VR().HeadPose(head);
    const bool leftTracked=VR().ControllerPose(0,left),rightTracked=VR().ControllerPose(1,right);
    const bool tracked=headTracked && leftTracked && rightTracked;
    const bool walking=g_haveManualInput && !g_shifted && locomotion::Length(g_manualLocal)>.05f;
    g_controllerBodyAnchor.Update(VR().PoseSerial(),TurnTime(),
        {head.m[0][3],head.m[1][3],-head.m[2][3]},
        {left.m[0][3],left.m[1][3],-left.m[2][3]},
        {right.m[0][3],right.m[1][3],-right.m[2][3]},g_headingBase,
        g_bodyFollower.valid ? firstperson::BodyAngle(g_bodyFollower.yaw-g_headingBase) : g_controllerBodyAnchor.valid ? g_controllerBodyAnchor.yaw : VR().HeadYawRadians(),headTracked,walking,VR().HeadYawRadians(),Cfg().firstPersonRoomscaleNeckMetres,leftTracked && rightTracked);
    if(Cfg().firstPersonDriftLog) {
        static uint64_t next=0;static bool lastHeld=false;static int lastTracking=-1;
        const auto now=GetTickCount64();
        const int tracking=int(headTracked)*4+int(leftTracked)*2+int(rightTracked);
        if(now>=next || lastHeld!=g_controllerBodyAnchor.held || lastTracking!=tracking) {
            lastTracking=tracking;
            next=now+1000;lastHeld=g_controllerBodyAnchor.held;
            LogF("body-anchor: held=%d rotating=%d tracked=%d walking=%d reason=%d hand-travel=(%.1f,%.1f)mm head-only=(%.1f,%.1f)mm yaw=(body %.1f head %.1f)deg turn=%.2fdeg pending=%.2fdeg recovery=%d target=%.1fdeg pair=%.0fmm tracking=%d",
                g_controllerBodyAnchor.held,g_controllerBodyAnchor.rotating,tracked,walking,g_controllerBodyAnchor.reason,
                g_controllerBodyAnchor.handTravel[0]*1000,g_controllerBodyAnchor.handTravel[1]*1000,
                g_controllerBodyAnchor.offset.x*1000,g_controllerBodyAnchor.offset.z*1000,
                firstperson::BodyAngle(g_bodyFollower.yaw-g_headingBase)*57.2957795f,VR().HeadYawRadians()*57.2957795f,
                g_controllerBodyAnchor.turnOutput*57.2957795f,g_bodyFollower.pendingPhysicalTurn*57.2957795f,
                g_controllerBodyAnchor.heading.active,g_controllerBodyAnchor.heading.target*57.2957795f,
                std::hypot(left.m[0][3]-right.m[0][3],left.m[2][3]-right.m[2][3])*1000,tracking);
        }
    }
}
bool BodyTrackingSuspended() { UpdateControllerBodyAnchor();return g_controllerBodyAnchor.valid && g_controllerBodyAnchor.suspended; }
bool BodyControllersStationary() { UpdateControllerBodyAnchor();return g_controllerBodyAnchor.held; }
bool HasControllerBodyAnchor() { UpdateControllerBodyAnchor();return g_controllerBodyAnchor.valid; }
bool BodyControllersPermitTravel() { UpdateControllerBodyAnchor();return !g_controllerBodyAnchor.valid || g_controllerBodyAnchor.translating; }
void BodyTrackingOffset(float& right,float& forward) {
    UpdateControllerBodyAnchor();VR().FirstPersonViewOffset(right,forward);
    right-=g_controllerBodyAnchor.offset.x;forward-=g_controllerBodyAnchor.offset.z;
}
