firstperson::ControllerBodyAnchor g_controllerBodyAnchor;
void ResetControllerBodyAnchor() { g_controllerBodyAnchor.Reset(); }
void UpdateControllerBodyAnchor() {
    vr::HmdMatrix34_t head{},left{},right{};
    const bool tracked=Cfg().firstPersonFullBodyIK && Cfg().firstPersonMovementStabilization &&
        g_active && g_haveHeading && VR().HeadPose(head) &&
        VR().ControllerPose(0,left) && VR().ControllerPose(1,right);
    const bool walking=g_haveManualInput && !g_shifted && locomotion::Length(g_manualLocal)>.05f;
    g_controllerBodyAnchor.Update(VR().PoseSerial(),TurnTime(),
        {head.m[0][3],head.m[1][3],-head.m[2][3]},
        {left.m[0][3],left.m[1][3],-left.m[2][3]},
        {right.m[0][3],right.m[1][3],-right.m[2][3]},g_headingBase,
        g_bodyFollower.valid ? firstperson::BodyAngle(g_bodyFollower.yaw-g_headingBase) : VR().HeadYawRadians(),tracked,walking);
}
bool BodyControllersStationary() { UpdateControllerBodyAnchor();return g_controllerBodyAnchor.held; }
void BodyTrackingOffset(float& right,float& forward) {
    UpdateControllerBodyAnchor();VR().FirstPersonViewOffset(right,forward);
    right-=g_controllerBodyAnchor.offset.x;forward-=g_controllerBodyAnchor.offset.z;
}
