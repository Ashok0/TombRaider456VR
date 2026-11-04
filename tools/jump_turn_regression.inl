// Production palette regression: turn after calibrating off-center, then jump
// while Lara's native facing still belongs to the takeoff direction.
void TestJumpTurnCentering(uint8_t* objectInfo) {
    using namespace tr;using namespace tr::motiongun;
    const Vec points[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    std::memset(objectInfo,0,off::object_stride);
    for(int j=0;j<15;++j) {
        auto* bind=reinterpret_cast<float*>(objectInfo+1676)+j*16;
        bind[0]=bind[5]=bind[10]=bind[15]=1;
        bind[12]=-points[j].x;bind[13]=-points[j].y;bind[14]=-points[j].z;
    }
    dll.getJointAbsPositionLerp=uint32_t(reinterpret_cast<uint64_t>(&FakeBodyJoint)-g_boundBase);
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    float fixture[180]{},palette[180]{},repeat[180]{};skinFixture=fixture;skinFixtureCount=15;
    worldCameraValid=true;g_bodySkinScope=true;g_shadowDll=nullptr;
    config.firstPersonBodyScale=1;config.firstPersonMovementStabilization=true;
    g_groundEye.Reset();g_groundEye.Apply({0,0,0},0,{35,-850,140},0);
    for(float turn:{0.f,.7f,1.5707963f,3.1415926f,-1.5707963f,6.2831853f})
    for(float physicalYaw:{0.f,.6f,-1.2f}) for(float cm:{0.f,11.f})
    for(int state:{3,9,15,25,26,27,28,29}) for(int fraction:{0,128,256}) {
        ResetBodyFollower();g_headingBase=Wrap(turn);Head(physicalYaw);
        VR().m_rawHeadPose.m[0][3]=.23f;VR().m_rawHeadPose.m[2][3]=.17f;
        VR().m_firstPersonNeutral[0]=VR().m_firstPersonNeutral[2]=0;
        // Stick pivot preserves the current world-space roomscale displacement.
        VR().PivotHeadFloorOffset(turn);
        for(int hand=0;hand<2;++hand) {
            VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[hand].m[0][3]+=(hand ? .2f : -.2f);
            VR().m_rawControllerPose[hand].m[1][3]-=.3f;
        }
        config.firstPersonBodyForwardCm=cm;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=int16_t(state);
        *reinterpret_cast<uint32_t*>(itemMemory+0x1820)=8;
        *reinterpret_cast<int32_t*>(g_boundBase+dll.frameFrac)=fraction;
        auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
        auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
        prev={600,-200,400};pos={640,-260,440}; // Native yaw deliberately stays at takeoff.
        const float t=fraction/256.f;
        const Vec root{600+40*t,-200-60*t,400+40*t};
        const auto anchor=locomotion::Rotate({35,140},g_headingBase);
        g_scenePose={int32_t(std::lround(root.x+anchor.x)),int32_t(root.y-850),int32_t(std::lround(root.z+anchor.z))};
        g_scenePoseValid=true;g_bodyVisualOffset=g_bodyWallOffset={};
        locomotion::Vec view{};VR().FirstPersonViewOffset(view.x,view.z);
        const auto physical=locomotion::Rotate(view,g_headingBase)*LiveWorldUnitsPerMetre();
        const auto behind=locomotion::Rotate({35,140-cm*10},Wrap(turn+physicalYaw));
        const Vec expected{g_scenePose.x_pos+physical.x-behind.x,0,g_scenePose.z_pos+physical.z-behind.z};
        const auto before=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
        ++VR().m_poseSerial;
        for(int eye:{0,1}) {
            const Vec camera{300.f+eye*64,-400,-200};
            worldCameraPos[0]=camera.x;worldCameraPos[1]=camera.y;worldCameraPos[2]=camera.z;
            for(int j=0;j<15;++j) {
                bodyJointWorld[j]=Add(root,Add(points[j],{25*t,0,-45*t}));
                Frame animated{firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},.15f*t)),Sub(bodyJointWorld[j],camera)};
                WriteRows(Multiply(animated,{firstperson::IdentityBasis(),Scale(points[j],-1)}),fixture+j*12);
            }
            Detour_GetJoints(itemMemory,palette,0);Detour_GetJoints(itemMemory,repeat,0);
            Frame hip{};ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,0,hip);hip.origin=Add(hip.origin,camera);
            Check(std::fabs(hip.origin.x-expected.x)<.15f && std::fabs(hip.origin.z-expected.z)<.15f,
                "partial stick turns and roomscale keep the jumping body centered at its configured offset");
            Check(!std::memcmp(palette,repeat,sizeof(palette)),"repeated stereo jump draws do not accumulate centering");
            for(int j:{0,1,2,3,4,5,6,7,14}) {
                const auto actual=ReadRows(palette+j*12),native=ReadRows(fixture+j*12);
                Check(std::fabs(actual.origin.y-native.origin.y)<.001f,"jump centering preserves native vertical animation");
                for(int r=0;r<3;++r) for(int c=0;c<3;++c) Check(std::fabs(actual.basis.r[r][c]-native.basis.r[r][c])<.001f,
                    "jump centering preserves authored limb rotations");
            }
            if(MotionReady() || UnarmedIKReady()) for(int hand:{0,1}) {
                Frame target{},wrist{};Basis controller{};
                Check(BuildControllerWrist(hand,target,controller) && ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,hand?10:13,wrist),"jump wrist fixture resolves");
                Check(firstperson::Length(Sub(Add(wrist.origin,camera),target.origin))<.2f,"turning jump placement preserves controller wrist targets");
            }
            Check(before==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),"jump centering leaves gameplay and collision untouched");
        }
    }
    skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;
}
