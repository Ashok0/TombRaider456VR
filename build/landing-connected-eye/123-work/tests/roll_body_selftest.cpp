// Exercise the native roll boundary and the remastered body/arm renderer together.
#define main UnusedFullLocomotionSuite
#include "locomotion_selftest.cpp"
#undef main

void TestRollBodyFrame() {
    using namespace tr;
    using namespace tr::motiongun;
    using namespace tr::firstperson;
    for(int game:{1,2,3}) for(bool armed:{false,true})
    for(float base:{0.f,.7f,3.1f,-3.1f}) for(int recovery:{0,1,2}) {
        trackingtest::Clock clock;
        BodyRig(game,armed);testConfigFlags|=1;
        testConfig.firstPersonFullBodyIK=true;testConfig.firstPersonBodyScale=1;
        testConfig.firstPersonMovementStabilization=true;
        g_haveManualInput=false;g_heading.base=base;g_bodySkinScope=true;g_pistolBodyScope=armed;
        g_groundEye.valid=true;g_groundEye.bodyLocal={};
        auto* object=Ptr<uint8_t>(dll.objects);
        Frame reference[15]{};
        for(int j=0;j<15;++j) Check(ReadIKJoint(object,ikNative,15,0,nullptr,nullptr,j,reference[j]),"roll fixture reads body");
        for(int hand=0;hand<2;++hand) {
            testControllerPose[hand].m[0][3]=testControllerOffset[hand].x;
            testControllerPose[hand].m[1][3]=1.7f-testControllerOffset[hand].y;
            testControllerPose[hand].m[2][3]=-testControllerOffset[hand].z;
        }
        auto& pos=Field<PHD_3DPOS>(off::item_pos);pos.y_rot=locomotion::Angle(base);
        auto render=[&](float yaw) {
            Field<PHD_3DPOS>(off::item_pos_prev)=pos;
            *Ptr<int32_t>(dll.frameFrac)=256;
            float palette[384]{};
            const Frame turn{RotationMatrix(AxisRotation({0,1,0},yaw)),{}};
            for(int j=0;j<15;++j) {
                Frame bind{},inverse{};ReadIKBind(object,15,0,nullptr,nullptr,j,bind);Inverse(bind,inverse);
                WriteRows(Multiply(Multiply(turn,reference[j]),inverse),palette+j*12);
            }
            Check(ApplyFullBodyIK(item,palette,15),"roll recovery runs production remastered full-body IK");
            std::array<Frame,15> result{};
            for(int j=0;j<15;++j) ReadIKJoint(object,palette,15,0,nullptr,nullptr,j,result[j]);
            return result;
        };
        ++testVR.m_poseSerial;render(base);
        const float relative=BodyAngle(g_bodyFollower.yaw-base);
        for(int roll=0;roll<2;++roll) {
            Field<int16_t>(off::item_anim_state)=45;Field<int16_t>(off::item_anim_number)=146;
            nativeHalfTurn=true;nextState=23;Detour_AnimateLara(item);
            const float expected=locomotion::Wrap(base+(roll+1)*locomotion::Pi);
            Check(std::fabs(BodyAngle(g_heading.base-expected))<.0001f,"native roll turns the camera once");
            // The roll hides the body. Input/roomscale can sample the anchor
            // before any renderer has transported the cached body follower.
            UpdateControllerBodyAnchor();
            Check(std::fabs(BodyAngle(g_controllerBodyAnchor.yaw-relative))<.0001f,
                "roll must not seed a reversed tracking-space stance from stale render yaw");
            Field<int16_t>(off::item_anim_state)=2;Field<int16_t>(off::item_anim_number)=103;
            TurnBodyToHead(item,.05f);
            Check(std::fabs(BodyAngle(locomotion::Radians(pos.y_rot)-expected-relative))<.001f,
                "simulation before rendering must not undo the native half-turn");
            if(recovery==1) g_bodyFollower.Reset();
            clock.Advance(recovery==2 ? .3 : 1./90);++testVR.m_poseSerial;
            for(int frame=0;frame<3;++frame) {
                const auto shown=render(expected);
                Check(std::fabs(BodyAngle(g_bodyFollower.yaw-expected-relative))<.001f,
                    "first visible torso follows camera after cached, reset or expired roll recovery");
                const motiongun::Vec right{std::cos(expected),0,-std::sin(expected)};
                Check(Dot(Sub(shown[8].origin,shown[11].origin),right)>0,
                    "remastered shoulders retain the correct left/right order after the roll");
                for(int hand=0;hand<2;++hand) {
                    Frame wrist{};Basis controller{};Check(BuildControllerWrist(hand,wrist,controller),"roll keeps tracked wrists available");
                    Check(Length(Sub(shown[hand ? 10 : 13].origin,wrist.origin))<.05f,
                        "both arms end at their controller positions in the turned body frame");
                }
            }
        }
        g_bodySkinScope=g_pistolBodyScope=false;
    }
}

int main() {
    TestRollBodyFrame();TestJumpRoll();
    std::printf("PASS: %d roll/camera/body checks (TR1-3)\n",checks);
}
