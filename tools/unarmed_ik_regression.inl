// Included by first_person_tests.cpp: exercises real palette and controller paths.
namespace {
tr::motiongun::Vec ikWrists[2]{};
tr::motiongun::Vec ikArmJoints[15]{};
float observedForearmPalette[33*12]{},observedGunPalette[33*12]{};
int observedForearmPasses=0,observedGunPasses=0;
void __cdecl ObserveIKWeaponDraw(uint8_t* item,int32_t,int32_t pass) {
    using namespace tr;
    float palette[33*12]{};
    const int count=Detour_GetJoints(item,palette,pass);
    const auto bits=*reinterpret_cast<const uint32_t*>(item+off::item_mesh_bits);
    if (g_renderArm<0) {
        ++observedForearmPasses;
        Check(bits && !(bits&~0x1200u),"attachment IK draw contains native forearm vertices only");
        uint64_t visible=0;int captured=0;
        const auto* render=FirstPersonRenderBodyPalette(visible,captured);
        Check(render && captured==count && !std::memcmp(render,palette,count*12*sizeof(float)),
              "forearm skin upload retains the solved IK palette before native masking");
        std::memcpy(observedForearmPalette,palette,count*12*sizeof(float));
    } else {
        ++observedGunPasses;
        Check(bits==(g_renderArm ? 0x400u : 0x2000u),"gun draw keeps original rigid hand-only mask");
        std::memcpy(observedGunPalette,palette,count*12*sizeof(float));
    }
}
void __cdecl FakeIKJoint(uint8_t* item,tr::PHD_VECTOR* p,int joint,int) {
    Check(item==itemMemory && joint>=8 && joint<=13,"IK queries native arm chain only");
    const auto v=joint==10 || joint==13 ? ikWrists[joint==10 ? 1 : 0] : ikArmJoints[joint];
    *p={int(v.x),int(v.y),int(v.z)};
}
uint8_t observedHandGeometry[tr::off::geom_stride]{};
int16_t observedHandState=-1;
void __cdecl ObserveUnarmedHandDraw(uint8_t* item,int32_t,int32_t) {
    using namespace tr;
    const int object=*reinterpret_cast<const int16_t*>(item+off::item_object);
    std::memcpy(observedHandGeometry,Ptr<uint8_t>(dll.objects)+object*off::object_stride+off::object_geom,
                sizeof(observedHandGeometry));
    observedHandState=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
}
void TestUnarmedHandMeshes() {
    using namespace tr;
    const auto savedHandDll=g_unarmedHandDll;const auto savedShadow=g_shadowDll;
    const auto savedDraw=g_hDrawCreatureHD.m_trampoline;
    const auto savedHeads=dll.gLaraHeads;const auto savedGame=dll.game;
    const auto savedScope=g_bodySkinScope;const auto savedSettings=config;
    auto* current=Ptr<uint8_t>(dll.objects)+off::object_geom;
    uint8_t original[off::geom_stride];std::memcpy(original,current,sizeof(original));
    alignas(16) static uint8_t hands[63*off::geom_stride]{};
    const auto rva=uint32_t(reinterpret_cast<uint64_t>(hands)-g_boundBase);
    UnarmedHandDll handDll{dll.timestamp,rva};g_unarmedHandDll=&handDll;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveUnarmedHandDraw);
    dll.gLaraHeads=0;
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    const auto savedBits=bits;const auto savedState=state;const auto savedStatus=status;
    for(int i=0;i<63;++i) {
        auto* geom=hands+i*off::geom_stride;std::memset(geom,i,off::geom_stride);
        *reinterpret_cast<const void**>(geom+off::geom_mesh)=geom; // Unique live mesh identity.
    }
    // Different material banks may share mesh data. Match the full native
    // descriptor so a glove/X-ray pass never borrows the bare-hand material.
    for(int bank:{0,30,60})
        *reinterpret_cast<const void**>(hands+(bank+2)*off::geom_stride+off::geom_mesh)=hands+2*off::geom_stride;
    for(int which:{0,1}) for(int bank:{0,30,60}) for(int gait:{0,1,2})
    for(uint32_t mask:{0x600u,0x3000u,0x3600u}) {
        dll.game=which;state=int16_t(gait);status=0;bits=mask;
        const auto* run=hands+(bank+2)*off::geom_stride;
        const auto* rest=hands+(bank+1)*off::geom_stride;
        std::memcpy(current,run,off::geom_stride);
        Check(UnarmedIKReady(),"mesh regression runs with active controller arm IK");
        Detour_DrawCreatureHD(itemMemory,1,0);
        Check(!std::memcmp(observedHandGeometry,rest,off::geom_stride),
              "VRIK draws resting hands with matching bare/glove/X-ray geometry and bindings");
        Check(!std::memcmp(current,run,off::geom_stride) && bits==mask && observedHandState==gait && state==gait,
              "hand mesh replacement restores full descriptor and preserves native gait and mask");
        std::memcpy(current,rest,off::geom_stride);
        Detour_DrawCreatureHD(itemMemory,1,0);
        Check(!std::memcmp(observedHandGeometry,rest,off::geom_stride),"already resting hands remain unchanged");
    }
    for(int reason=0;reason<12;++reason) {
        config=savedSettings;status=0;state=1;bits=0x3600;g_active=true;
        VR().m_controllerPoseValid[0]=true;appMemory[0x9e4]=1;g_shadowDll=nullptr;
        g_unarmedHandDll=&handDll;
        int index=2,useBits=1;
        ShadowDll shadow{};static int pass=4;
        shadow.renderPass=uint32_t(reinterpret_cast<uint64_t>(&pass)-g_boundBase);
        if(reason==0) status=4;
        if(reason==1) status=2;
        if(reason==2) config.firstPersonUnarmedIK=false;
        if(reason==3) g_active=false;
        if(reason==4) VR().m_controllerPoseValid[0]=false;
        if(reason==5) appMemory[0x9e4]=0;
        if(reason==6) g_shadowDll=&shadow;
        if(reason==7) { bits=0x3fff;useBits=0; }
        if(reason==8) index=5; // Weapon grip is never substituted.
        if(reason==9) *reinterpret_cast<void**>(hands+off::geom_stride+off::geom_mesh)=nullptr;
        if(reason==10) state=10; // Ledge grip remains native.
        if(reason==11) g_unarmedHandDll=nullptr;
        const auto* selected=hands+index*off::geom_stride;
        std::memcpy(current,selected,off::geom_stride);
        Detour_DrawCreatureHD(itemMemory,useBits,0);
        Check(!std::memcmp(observedHandGeometry,selected,off::geom_stride) &&
              !std::memcmp(current,selected,off::geom_stride),
              "armed/disabled/native/shadow/unloaded/interaction hand meshes remain untouched");
        *reinterpret_cast<void**>(hands+off::geom_stride+off::geom_mesh)=hands+off::geom_stride;
    }
    g_active=true;VR().m_controllerPoseValid[0]=true;appMemory[0x9e4]=1;
    config=savedSettings;bits=savedBits;state=savedState;status=savedStatus;
    std::memcpy(current,original,sizeof(original));
    g_hDrawCreatureHD.m_trampoline=savedDraw;g_unarmedHandDll=savedHandDll;g_shadowDll=savedShadow;
    dll.gLaraHeads=savedHeads;dll.game=savedGame;g_bodySkinScope=savedScope;
}
void TestUnarmedIK() {
    using namespace tr;
    using namespace tr::motiongun;
    const auto settings=config; const auto savedDll=dll;
    config.firstPersonFullBodyIK=false; // Preserve coverage of the arm-only fallback.
    const auto savedCalibration=testCalibration;
    const auto savedItem=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
    const auto savedLara=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
    const auto savedPose=g_scenePose;const auto savedHeading=g_headingBase;
    const auto savedOffset=g_bodyVisualOffset;
    const auto savedWall=g_bodyWallOffset;
    const auto savedLeftTwist=g_unarmedTwist[0],savedRightTwist=g_unarmedTwist[1];
    const auto savedGetJoints=g_hGetJoints.m_trampoline;
    const auto savedShadow=g_shadowDll;
    vr::HmdMatrix34_t controllers[2];std::memcpy(controllers,VR().m_rawControllerPose,sizeof(controllers));
    auto rva=[](const void* p) { return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase); };
    alignas(16) static uint8_t objectInfo[off::object_stride]{};
    dll.objects=rva(objectInfo);dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeIKJoint));
    *reinterpret_cast<int16_t*>(itemMemory+off::item_object)=0;
    *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
    status=0;gun=1;
    *reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits)=0x3fff;
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    g_renderArm=-1;g_bodySkinScope=true;
    const auto identity=firstperson::IdentityBasis();
    auto nearVec=[](Vec a,Vec b) { return std::sqrt(Dot(Sub(a,b),Sub(a,b)))<.04f; };
    auto nearFrame=[&](const Frame& a,const Frame& b) {
        Check(nearVec(a.origin,b.origin),"IK wrist position equals calibrated gun wrist");
        for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
            float dot=0;
            for(int k=0;k<3;++k) dot+=a.basis.r[k][r]*a.basis.r[k][c];
            Check(std::fabs(dot-(r==c ? 1.f : 0.f))<.002f,"constrained wrist stays rigid without shear or collapse");
        }
    };
    // Independently check analytic lengths, connected endpoints, mirrored elbow
    // poles, straight/folded arms and targets outside the native reach.
    const Frame arm[3]={{identity,{0,0,0}},{identity,{0,300,0}},{identity,{0,500,0}}};
    for(Vec target:{Vec{100,200,250},Vec{-250,0,100},Vec{0,500,0},Vec{0,0,0},
                    Vec{0,-500,0},Vec{0,900,0},Vec{.00001f,0,0}}) {
        Frame delta[3]{}; Frame desired{identity,target};
        Check(firstperson::SolveArm(arm,desired,{1,1,-.25f},delta),"IK solves bent, extended and singular poses");
        const auto elbow=Transform(delta[1],arm[1].origin);
        Check(nearVec(Transform(delta[0],arm[0].origin),arm[0].origin),"IK shoulder remains anchored");
        Check(nearVec(Transform(delta[0],arm[1].origin),elbow),"upper arm connects to elbow");
        Check(nearVec(Transform(delta[1],arm[2].origin),target),"forearm connects exactly to tracked wrist");
        nearFrame(Multiply(delta[2],arm[2]),desired);
        const float distance=std::sqrt(Dot(target,target));
        if(distance>=100 && distance<=500) {
            Check(std::fabs(std::sqrt(Dot(elbow,elbow))-300)<.04f,"reachable upper arm retains native length");
            const Vec lower=Sub(target,elbow);
            Check(std::fabs(std::sqrt(Dot(lower,lower))-200)<.04f,"reachable forearm retains native length");
        }
    }
    // Model an already-bent arm with the forearm forward and neutral gun
    // wrist axes. This lets the test independently measure actual mesh twist.
    const Basis neutral=motiongun::GunBasis(identity);
    const Frame restArm[3]={{neutral,{0,0,0}},{neutral,{0,300,0}},{neutral,{0,300,200}}};
    // Natural roll: the controller and forearm must keep turning together,
    // including past the old 90-degree clamp and 135..180 return-to-neutral.
    // Independent animation rotations cannot change that rest-pose relation.
    for (float forearmDegrees:{-170.f,-90.f,0.f,90.f,170.f})
    for (float wristDegrees:{-180.f,-90.f,0.f,90.f,180.f})
    for (float controllerDegrees:{-720.f,-190.f,-180.f,-135.f,-90.f,-45.f,0.f,45.f,90.f,135.f,180.f,190.f,720.f}) {
        const auto turn=[](float degree) { return firstperson::RotationMatrix(
            firstperson::AxisRotation({0,0,1},degree*3.14159265358979323846f/180)); };
        Frame animated[3]={restArm[0],restArm[1],restArm[2]};
        animated[1].basis=Multiply(turn(forearmDegrees),neutral);
        animated[2].basis=Multiply(turn(wristDegrees),neutral);
        const Frame target{Multiply(turn(controllerDegrees),neutral),restArm[2].origin};
        Frame baseline[3]{},changes[3]{};
        Check(firstperson::SolveTrackedArm(restArm,target,{0,1,0},restArm,baseline),"neutral tracked arm roll reference solves");
        Check(firstperson::SolveTrackedArm(animated,target,{0,1,0},restArm,changes),"independent native forearm and wrist roll solves");
        const auto expected=Multiply(baseline[1],restArm[1]);
        const auto actual=Multiply(changes[1],animated[1]);
        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
            Check(std::fabs(actual.basis.r[r][c]-expected.basis.r[r][c])<.003f,
                  "native wrist pose cannot twist the independently animated forearm into a knot");
        const Vec handRight=Transform(target.basis,{1,0,0});
        const Vec armRight=Transform(actual.basis,{1,0,0});
        Check(Dot(handRight,armRight)>.999f,
              "forearm follows actual controller rotation without clamping or reversing at large angles");
        const auto actualHand=Multiply(changes[2],animated[2]);
        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
            Check(std::fabs(actualHand.basis.r[r][c]-target.basis.r[r][c])<.003f,
                  "natural arm roll preserves exact controller hand orientation");
        Check(nearVec(Transform(changes[1],animated[2].origin),target.origin),
              "natural forearm roll cannot move the wrist endpoint");
    }
    // Rest axes differ between forearm and hand meshes. Check continuous roll
    // through those real relationships, mirrored arms and body headings.
    for(float side:{-1.f,1.f}) for(float yaw:{-2.f,0.f,1.3f}) {
        const Frame bodyTurn{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},yaw)),{}};
        Frame rest[3]={restArm[0],restArm[1],restArm[2]};
        rest[0].origin.x=rest[1].origin.x=rest[2].origin.x=side*100;
        rest[1].basis=Multiply(firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},side*.7f)),neutral);
        rest[2].basis=Multiply(firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},-side*.4f)),neutral);
        Frame animated[3]{};
        for(int i=0;i<3;++i) animated[i]=Multiply(bodyTurn,rest[i]);
        const auto axis=Transform(bodyTurn.basis,{0,0,1});
        Frame previous[3]{};bool havePrevious=false;
        for(int degree=-720;degree<=720;degree+=2) {
            const auto roll=firstperson::RotationMatrix(firstperson::AxisRotation(axis,degree*3.14159265358979323846f/180));
            const Frame target{Multiply(roll,animated[2].basis),animated[2].origin};
            Frame changes[3]{};
            Check(firstperson::SolveTrackedArm(animated,target,Transform(bodyTurn.basis,{0,1,0}),rest,changes),
                  "mirrored tracked arms solve with independent rest joint axes");
            const auto forearm=Multiply(changes[1],animated[1]);
            const auto expected=Multiply(roll,animated[1].basis);
            for(int row=0;row<3;++row) for(int col=0;col<3;++col)
                Check(std::fabs(forearm.basis.r[row][col]-expected.r[row][col])<.003f,
                      "forearm follows controller through rest binding without accumulating or clamping roll");
            if(havePrevious) {
                const auto before=Multiply(previous[1],animated[1]);
                Check(Dot(Transform(before.basis,{1,0,0}),Transform(forearm.basis,{1,0,0}))>.999f,
                      "forearm has no reversal or snap at 90, 135 or 180 degrees");
            }
            std::memcpy(previous,changes,sizeof(previous));havePrevious=true;
        }
    }
    // Legacy constrained-solver math remains tested separately; tracked arms
    // now use SolveTrackedArm and never enter its clamp/neutral-return path.
    // Returning to the same physical pose after full turns must agree with
    // a fresh solver. The old accumulator stayed clamped until FP was toggled.
    for (int direction:{-1,1}) for (int turns:{1,3}) {
        firstperson::ArmTwistState history{};
        Basis wrist{},forearm{};
        for (int degree=0;degree<=360*turns;++degree) {
            const auto rotation=firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},
                direction*degree*3.14159265358979323846f/180));
            firstperson::ConstrainWrist(neutral,Multiply(rotation,neutral),{0,0,1},history,wrist,forearm);
        }
        for (int repeat=0;repeat<8;++repeat) {
            firstperson::ConstrainWrist(neutral,neutral,{0,0,1},history,wrist,forearm);
            Check(nearVec(Transform(wrist,{1,0,0}),Transform(neutral,{1,0,0})),
                  "full controller turns return to neutral without toggling first person");
            Check(nearVec(Transform(forearm,{1,0,0}),Vec{1,0,0}),
                  "forearm cannot retain accumulated turns after returning to neutral");
        }
    }
    firstperson::ArmTwistState twistState{};
    Basis previous=neutral;bool havePrevious=false;
    auto testRoll=[&](float degrees) {
        const float radians=degrees*3.14159265358979323846f/180;
        const Basis rotation{{{std::cos(radians),-std::sin(radians),0},
                               {std::sin(radians),std::cos(radians),0},{0,0,1}}};
        const Frame target{Multiply(rotation,neutral),restArm[2].origin};
        Frame changes[3]{};
        Check(firstperson::SolveArm(restArm,target,{0,1,0},changes,&twistState),"wrist-roll sweep solves through the 180-degree boundary");
        const auto wrist=Multiply(changes[2],restArm[2]);
        const auto forearm=Multiply(changes[1],restArm[1]);
        const float principal=std::remainder(degrees,360.f);
        const float limited=std::clamp(principal,-90.f,90.f)*3.14159265358979323846f/180;
        const Vec expectedRight{std::cos(limited),std::sin(limited),0};
        const Vec right=Transform(wrist.basis,{1,0,0});
        if (std::fabs(principal)<=135)
            Check(nearVec(right,expectedRight),"reachable wrist roll and adjacent limit plateau follow the current pose");
        Check(right.x>=-.001f,"wrist roll never exceeds its 90-degree bound");
        const Vec forearmRight=Transform(forearm.basis,{1,0,0});
        Check(Dot(right,forearmRight)>std::cos(18.1f*3.14159265358979323846f/180),
              "at most 18 degrees of roll remains across the wrist seam");
        const Vec blended=Scale(Add(right,forearmRight),.5f);
        Check(Dot(blended,blended)>.97f,"blended wrist cross-section cannot collapse into a knot");
        Check(nearVec(Transform(changes[1],restArm[2].origin),target.origin),"forearm roll preserves wrist attachment");
        Check(nearVec(wrist.origin,target.origin),"wrist rotation limits do not move tracked hands");
        if(havePrevious) Check(Dot(right,Transform(previous,{1,0,0}))>.999f,
                              "roll limit has no snap at +/-180 degrees or reversal");
        previous=wrist.basis;havePrevious=true;
    };
    for(int angle=0;angle<=270;++angle) testRoll(float(angle));
    for(int angle=269;angle>=-270;--angle) testRoll(float(angle));
    for(int angle=-269;angle<=0;++angle) testRoll(float(angle));
    // Moving Lara's facing/forearm reference must not create a persistent
    // history offset either. Repeated stereo/material draws are idempotent.
    for (float yaw:{0.f,.7f,-2.1f}) for (float bend:{-170.f,0.f,55.f,170.f}) {
        firstperson::ArmTwistState history{};
        const auto body=firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},yaw));
        const auto reference=Multiply(body,neutral);
        const auto axis=Transform(body,{0,0,1});
        const auto bendAxis=Transform(body,{1,0,0});
        const auto swing=firstperson::AxisRotation(bendAxis,bend*3.14159265358979323846f/180);
        for (int degree=-720;degree<=720;degree+=7) {
            const auto twist=firstperson::AxisRotation(axis,degree*3.14159265358979323846f/180);
            const auto target=Multiply(firstperson::RotationMatrix(firstperson::RotationProduct(swing,twist)),reference);
            firstperson::ArmTwistState fresh{};Basis expected{},expectedForearm{};
            firstperson::ConstrainWrist(reference,target,axis,fresh,expected,expectedForearm);
            for (int draw=0;draw<3;++draw) {
                Basis wrist{},forearm{};
                firstperson::ConstrainWrist(reference,target,axis,history,wrist,forearm);
                for (int row=0;row<3;++row) for (int col=0;col<3;++col)
                    Check(std::fabs(wrist.r[row][col]-expected.r[row][col])<.002f &&
                          std::fabs(forearm.r[row][col]-expectedForearm.r[row][col])<.002f,
                          "defined wrist pose is independent of rotation history and repeated render draws");
            }
        }
        // Undefined twist at an exact 180-degree swing may use the last
        // sample, but the next defined pose must immediately recover.
        Basis wrist{},forearm{};
        const auto folded=Multiply(firstperson::RotationMatrix(firstperson::AxisRotation(bendAxis,3.14159265358979323846f)),reference);
        firstperson::ConstrainWrist(reference,folded,axis,history,wrist,forearm);
        firstperson::ConstrainWrist(reference,reference,axis,history,wrist,forearm);
        for (int row=0;row<3;++row) for (int col=0;col<3;++col)
            Check(std::fabs(wrist.r[row][col]-reference.r[row][col])<.002f,
                  "singular arm pose cannot poison the next neutral wrist orientation");
    }
    for(float bend:{-170.f,-120.f,-60.f,0.f,60.f,120.f,170.f}) {
        const float a=bend*3.14159265358979323846f/180;
        const Basis rotation{{{1,0,0},{0,std::cos(a),-std::sin(a)},{0,std::sin(a),std::cos(a)}}};
        Frame changes[3]{};
        Check(firstperson::SolveArm(restArm,{Multiply(rotation,neutral),restArm[2].origin},{0,1,0},changes),"extreme wrist bend solves");
        const auto wrist=Multiply(changes[2],restArm[2]);
        const Vec forward=Transform(wrist.basis,{0,1,0});
        Check(forward.z>=std::cos(55.1f*3.14159265358979323846f/180),"wrist flexion stays within 55 degrees");
    }
    // Running can rotate the authored forearm/wrist around the bone. It must
    // not change the final hand orientation or reintroduce twist at the seam.
    Frame baselineChanges[3]{};
    const Frame fixedTarget{neutral,restArm[2].origin};
    Check(firstperson::SolveArm(restArm,fixedTarget,{0,1,0},baselineChanges),"neutral gait baseline solves");
    const auto baselineForearm=Multiply(baselineChanges[1],restArm[1]);
    for(int angle=-180;angle<=180;angle+=5) {
        const float a=angle*3.14159265358979323846f/180;
        const Basis animated{{{std::cos(a),-std::sin(a),0},{std::sin(a),std::cos(a),0},{0,0,1}}};
        Frame running[3]={restArm[0],restArm[1],restArm[2]};
        running[1].basis=running[2].basis=Multiply(animated,neutral);
        Frame changes[3]{};
        Check(firstperson::SolveArm(running,fixedTarget,{0,1,0},changes),"running animation roll can be removed");
        const auto wrist=Multiply(changes[2],running[2]);
        const auto forearm=Multiply(changes[1],running[1]);
        for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
            Check(std::fabs(wrist.basis.r[r][c]-neutral.r[r][c])<.002f,"running retains walking controller wrist orientation");
            Check(std::fabs(forearm.basis.r[r][c]-baselineForearm.basis.r[r][c])<.002f,"running cannot add authored roll to the IK forearm");
        }
    }
    // Rotate a controller about a fixed physical grip, not about the wrist.
    // Nonzero side/up/forward calibration exposes the old lever-arm drift.
    for (float scale:{.5f,1.f,2.f}) for (float side:{-1.f,1.f})
    for (float yaw:{-170.f,-80.f,0.f,80.f,170.f})
    for (float pitch:{-120.f,-45.f,0.f,45.f,120.f})
    for (float roll:{-170.f,-90.f,0.f,90.f,170.f}) {
        const Vec gripLocal=Scale(Vec{side*20,90,-35},scale);
        const Vec physicalGrip=Scale(Vec{side*170,250,200},scale);
        Frame chain[3]={restArm[0],restArm[1],restArm[2]};
        for (auto& joint:chain) joint.origin=Scale(joint.origin,scale);
        const auto orientation=motiongun::GunBasis(CalibratedController(identity,{0,0,0,pitch,yaw,roll}));
        const Frame target{orientation,Sub(physicalGrip,Transform(orientation,gripLocal))};
        Frame changes[3]{},oldChanges[3]{};
        firstperson::ArmTwistState history{};
        Check(firstperson::SolveArm(chain,target,{side,1,-.25f},oldChanges),"original wrist limit reference solves");
        Check(firstperson::SolveArm(chain,target,{side,1,-.25f},changes,&history,nullptr,true,gripLocal),
              "controller grip anchored IK solves across yaw/pitch/roll, hands and scales");
        const auto wrist=Multiply(changes[2],chain[2]);
        const auto bounded=Multiply(oldChanges[2],chain[2]);
        Check(nearVec(Transform(wrist,gripLocal),physicalGrip),"rotation keeps calibrated grip exactly on controller");
        Check(nearVec(Transform(changes[0],chain[0].origin),chain[0].origin),"pivot correction keeps shoulder anchored");
        Check(nearVec(Transform(changes[0],chain[1].origin),Transform(changes[1],chain[1].origin)),
              "pivot correction keeps elbow connected");
        Check(nearVec(Transform(changes[1],chain[2].origin),wrist.origin),"pivot correction reconnects forearm to hand");
        for (int r=0;r<3;++r) for (int c=0;c<3;++c)
            Check(std::fabs(wrist.basis.r[r][c]-bounded.basis.r[r][c])<.002f,
                  "grip correction preserves existing bounded hand orientation");
        const float reach=firstperson::Length(Sub(wrist.origin,chain[0].origin));
        const float upper=firstperson::Length(Sub(chain[1].origin,chain[0].origin));
        const float lower=firstperson::Length(Sub(chain[2].origin,chain[1].origin));
        if (reach>=std::fabs(upper-lower) && reach<=upper+lower) {
            const Vec elbow=Transform(changes[1],chain[1].origin);
            Check(std::fabs(firstperson::Length(Sub(elbow,chain[0].origin))-upper)<.04f &&
                  std::fabs(firstperson::Length(Sub(wrist.origin,elbow))-lower)<.04f,
                  "reachable grip correction preserves both bone lengths");
        }
        Frame repeated[3]{};
        Check(firstperson::SolveArm(chain,target,{side,1,-.25f},repeated,&history,nullptr,true,gripLocal),"repeated eye pose solves");
        for (int bone=0;bone<3;++bone) {
            Check(nearVec(repeated[bone].origin,changes[bone].origin),"pivot solve cannot accumulate positional drift");
            for (int r=0;r<3;++r) for (int c=0;c<3;++c)
                Check(std::fabs(repeated[bone].basis.r[r][c]-changes[bone].basis.r[r][c])<.002f,
                      "pivot solve cannot accumulate stereo wrist twist");
        }
        Check(firstperson::SolveArm(chain,target,{side,1,-.25f},changes,nullptr,nullptr,false,gripLocal),"armed wrist remains unconstrained");
        const auto armed=Multiply(changes[2],chain[2]);
        Check(nearVec(armed.origin,target.origin),"armed wrist position is unchanged by grip-aware solver");
        for (int r=0;r<3;++r) for (int c=0;c<3;++c)
            Check(std::fabs(armed.basis.r[r][c]-target.basis.r[r][c])<.002f,"armed rotation remains exact");
    }
    Frame corrections[3]{}; Frame invalid[3]={arm[0],arm[0],arm[2]};
    Check(!firstperson::SolveArm(invalid,{identity,{1,2,3}},{0,1,0},corrections),"zero-length arm rejected");
    Check(!firstperson::SolveArm(arm,{identity,{NAN,2,3}},{0,1,0},corrections),"nonfinite target rejected");
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(itemMemory+off::item_pos);
    const Vec root{float(body.x_pos),float(body.y_pos),float(body.z_pos)};
    const Vec cameraOrigin{root.x+300,root.y-200,root.z-500};
    g_scenePose.x_pos=int(root.x);g_scenePose.y_pos=int(root.y-600);g_scenePose.z_pos=int(root.z);
    for(int count:{15,33}) {
        float fixture[33*12]{},poses[33*12]{},palette[33*12]{},baseline[33*12]{};
        int32_t mapping[33]{};Frame frames[33]{},invBind[33]{},bind[33]{};
        auto* geom=objectInfo+off::object_geom;
        *reinterpret_cast<int32_t*>(geom+28)=count==15 ? 0 : count;
        *reinterpret_cast<int32_t**>(geom+48)=mapping;
        *reinterpret_cast<float**>(geom+72)=poses;
        for(int i=0;i<count;++i) {
            const int joint=count==15 ? i : (i*7)%15;mapping[i]=joint;
            Vec local{float(joint*30),float(-joint*20),float(joint*10)};
            if(joint>=8 && joint<=13) {
                const int segment=(joint-8)%3;const float side=joint<11 ? 1.f : -1.f;
                local={side*(segment==1 ? 260.f : 220.f),-400.f+210.f*segment,30.f*segment};
            }
            const Vec world=Add(root,local);
            ikArmJoints[joint]=world;
            if(joint==10 || joint==13) ikWrists[joint==10 ? 1 : 0]=world;
            frames[i]={identity,Sub(world,cameraOrigin)};
            invBind[i]={CalibratedController(identity,{0,0,0,float(i*3),float(i*2),float(-i)}),{float(i*4),float(i*7),float(-i*2)}};
            Check(Inverse(invBind[i],bind[i]),"IK fixture nontrivial inverse bind is invertible");
            if(count==15) {
                auto* m=reinterpret_cast<float*>(objectInfo+1676)+i*16;
                for(int row=0;row<3;++row) for(int col=0;col<3;++col) m[col*4+row]=invBind[i].basis.r[row][col];
                m[12]=invBind[i].origin.x;m[13]=invBind[i].origin.y;m[14]=invBind[i].origin.z;m[15]=1;
            } else {
                auto raw=invBind[i];
                for(int row=0;row<3;++row) { raw.basis.r[row][1]=invBind[i].basis.r[row][2];raw.basis.r[row][2]=-invBind[i].basis.r[row][1]; }
                WriteRows(raw,poses+i*12);
            }
            WriteRows(Multiply(frames[i],invBind[i]),fixture+i*12);
        }
        // Canonical rest frames used when an HD attachment omits a joint.
        bool bound[15]{};
        for(int i=0;i<count;++i) if(!bound[mapping[i]]) {
            bound[mapping[i]]=true;
            auto* m=reinterpret_cast<float*>(objectInfo+1676)+mapping[i]*16;
            for(int row=0;row<3;++row) for(int col=0;col<3;++col) m[col*4+row]=invBind[i].basis.r[row][col];
            m[12]=invBind[i].origin.x;m[13]=invBind[i].origin.y;m[14]=invBind[i].origin.z;m[15]=1;
        }
        skinFixture=fixture;skinFixtureCount=count;
        for(int which:{0,1}) for(int step=0;step<16;++step) {
            dll.game=which;g_headingBase=step*.3f;
            testCalibration={.04f,-.06985f,.2032f,-30.f,float(step*3),float(-step*4)};
            g_bodyVisualOffset={float(step*5-25),float(80-step*9)};
            g_bodyWallOffset={float(-step*11),float(-step*17)};
            for(int hand=0;hand<2;++hand) {
                auto& m=VR().m_rawControllerPose[hand];
                const float yaw=step*.2f*(hand ? 1.f : -1.f);
                m={};m.m[0][0]=m.m[2][2]=std::cos(yaw);m.m[0][2]=std::sin(yaw);m.m[2][0]=-std::sin(yaw);m.m[1][1]=1;
                m.m[0][3]=VR().m_rawHeadPose.m[0][3]+(hand ? .25f : -.25f);
                m.m[1][3]=VR().m_firstPersonNeutral[1]-.25f-step*.01f;
                m.m[2][3]=VR().m_rawHeadPose.m[2][3]-.25f-step*.01f;
            }
            status=4;GunPose guns[2]{};
            for(int hand=0;hand<2;++hand) Check(BuildGunPose(hand,guns[hand]),"armed reference pose available");
            status=0;Check(UnarmedIKReady() && !MotionReady(),"unarmed IK never enables armed firing readiness");
            config.firstPersonUnarmedIK=false;
            Detour_GetJoints(itemMemory,baseline,0);
            config.firstPersonUnarmedIK=true;
            Check(Detour_GetJoints(itemMemory,palette,0)==count,"production unarmed IK preserves palette size");
            uint64_t mask=0;int capturedCount=0;const auto* physics=FirstPersonBodyPalette(mask,capturedCount);
            Check(physics && capturedCount==count && !std::memcmp(physics,baseline,count*12*sizeof(float)),
                  "controller IK cannot change the captured jiggle-physics skeleton");
            const auto* render=FirstPersonRenderBodyPalette(mask,capturedCount);
            Check(render && render!=physics && !std::memcmp(render,palette,count*12*sizeof(float)),
                  "shader uploads IK palette instead of overwriting it with physics skeleton");
            Check(FirstPersonTrackedHandJoint()==-1,"IK body does not enable rigid gun skinning shader");
            for(int i=0;i<count;++i) {
                const int joint=mapping[i];
                if(joint==10 || joint==13) {
                    const auto& gunPose=guns[joint==10 ? 1 : 0];
                    const auto rendered=Multiply(ReadRows(palette+i*12),bind[i]);
                    const Vec grip=Scale(Vec{-testCalibration.rightMetres,testCalibration.gripForwardMetres,
                                            -testCalibration.raiseMetres},LiveWorldUnitsPerMetre());
                    const Frame floatingGun{gunPose.desired,Sub(gunPose.trackedHand,cameraOrigin)};
                    Check(nearVec(Transform(rendered,grip),Transform(floatingGun,grip)),
                          "IK and floating gun keep the same physical grip pivot after wrist constraints");
                } else if(joint<8 || joint>13) {
                    Check(!std::memcmp(palette+i*12,baseline+i*12,12*sizeof(float)),"IK preserves torso, head and legs exactly");
                }
            }
            const auto itemSnapshot=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            const auto laraSnapshot=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
            Detour_GetJoints(itemMemory,palette,0);
            Check(itemSnapshot==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                  laraSnapshot==std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory)),"IK never writes native animation or collision state");
        }
        // Armed body wrists must meet the UNMODIFIED floating-gun wrist, not a
        // clamped IK approximation. Exercise bind helpers, camera fitting,
        // arbitrary rotations, out-of-reach controllers and both game layouts.
        {
            const auto oldHeading=g_headingBase;const auto oldCalibration=testCalibration;
            const auto oldFit=g_bodyVisualOffset;
            const auto oldWall=g_bodyWallOffset;
            const bool oldLong=g_longGunHooksReady;
            vr::HmdMatrix34_t oldHands[2];std::memcpy(oldHands,VR().m_rawControllerPose,sizeof(oldHands));
            g_longGunHooksReady=true;config.firstPersonUnarmedIK=false;
            for(int which:{0,1}) for(int weapon=1;weapon<=6;++weapon)
            for(int stance:{1,2,3,71,80,86,45}) for(int step=0;step<5;++step) {
                dll.game=which;gun=int16_t(weapon);
                *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=int16_t(stance);
                g_headingBase=step*.75f;g_bodyVisualOffset={float(step*21-40),float(20-step*19)};
                g_bodyWallOffset={float(-step*53),float(-step*37)};
                testCalibration={.04f,-.06985f,.2032f,-30.f,13,-9};
                for(int hand=0;hand<2;++hand) {
                    const auto basis=CalibratedController(identity,{0,0,0,float(step*65),float(step*-83),float(step*97)});
                    auto& m=VR().m_rawControllerPose[hand];m=oldHands[hand];
                    for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.m[r][c]=basis.r[r][c];
                    m.m[0][3]+=(hand ? 1.f : -1.f)*step*.3f;
                    m.m[1][3]-=step*.13f;m.m[2][3]+=step*.25f;
                }
                status=0;float nativePalette[33*12]{};Detour_GetJoints(itemMemory,nativePalette,0);
                status=4;Check(MotionReady(),"armed IK fixture has tracked guns with both IK INI options off");
                GunPose before[2]{};for(int hand=0;hand<2;++hand) Check(BuildGunPose(hand,before[hand]),"capture original gun position and pivot");
                float armedPalette[33*12]{};Detour_GetJoints(itemMemory,armedPalette,0);
                uint64_t mask=0;int captured=0;const auto* physics=FirstPersonBodyPalette(mask,captured);
                Check(physics && captured==count && !std::memcmp(physics,nativePalette,count*12*sizeof(float)),
                      "armed IK never replaces jiggle physics with controller motion");
                for(int i=0;i<count;++i) {
                    const int joint=mapping[i];
                    if(joint<8 || joint>13) Check(!std::memcmp(armedPalette+i*12,nativePalette+i*12,12*sizeof(float)),
                                                "armed arm-only IK preserves animated torso legs and head");
                    if(joint==8 || joint==11) Check(nearVec(Multiply(ReadRows(armedPalette+i*12),bind[i]).origin,
                                                              Multiply(ReadRows(nativePalette+i*12),bind[i]).origin),
                                                   "IK shoulder stays anchored to animated body");
                }
                for(int hand=0;hand<2;++hand) {
                    g_renderArm=hand;float gunPalette[33*12]{};Detour_GetJoints(itemMemory,gunPalette,0);g_renderArm=-1;
                    // Go through the real draw split too. The old code solved
                    // wrist matrices but never drew the weapon-mesh forearms.
                    const auto savedDraw=g_hDrawCreatureHD.m_trampoline;
                    auto& meshBits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
                    const auto savedBits=meshBits;
                    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveIKWeaponDraw);
                    meshBits=hand ? 0x600u : 0x3000u;
                    observedForearmPasses=observedGunPasses=0;
                    Detour_DrawCreatureHD(itemMemory,1,0);
                    Check(observedForearmPasses==1 && observedGunPasses==1,
                          "armed rendering emits an IK forearm and unchanged gun hand from each native attachment");
                    Check(!std::memcmp(observedForearmPalette,armedPalette,count*12*sizeof(float)),
                          "visible forearm uses same articulated chain as the solved body");
                    Check(!std::memcmp(observedGunPalette,gunPalette,count*12*sizeof(float)),
                          "restoring forearm draw cannot change floating gun transform");
                    meshBits=savedBits;g_hDrawCreatureHD.m_trampoline=savedDraw;
                    g_bodySkinScope=true;
                    const int joint=hand ? 10 : 13;
                    for(int i=0;i<count;++i) if(mapping[i]==joint) {
                        const auto armWrist=Multiply(ReadRows(armedPalette+i*12),bind[i]);
                        const auto gunWrist=Multiply(ReadRows(gunPalette+i*12),bind[i]);
                        Check(nearVec(armWrist.origin,gunWrist.origin),"armed IK wrist meets exact floating hand position through rotation and reach");
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                            Check(std::fabs(armWrist.basis.r[r][c]-gunWrist.basis.r[r][c])<.003f,
                                  "armed IK cannot clamp or twist the gun-hand orientation");
                        const Vec grip=Scale(Vec{-testCalibration.rightMetres,testCalibration.gripForwardMetres,-testCalibration.raiseMetres},LiveWorldUnitsPerMetre());
                        Check(nearVec(Transform(armWrist,grip),Transform(gunWrist,grip)),"armed IK and floating hand share the identical grip pivot");
                    }
                    GunPose after{};Check(BuildGunPose(hand,after) && !std::memcmp(&before[hand],&after,sizeof(after)),
                                          "arm rendering cannot change weapon pose muzzle or aim");
                }
            }
            // A single weapon must keep its right arm connected if left tracking
            // drops out; the untracked left arm keeps its animation.
            gun=2;status=4;VR().m_controllerPoseValid[0]=false;
            float oneHand[33*12]{};Detour_GetJoints(itemMemory,oneHand,0);
            for(int i=0;i<count;++i) if(mapping[i]==10) {
                GunPose gunPose{};Check(BuildGunPose(1,gunPose),"right-only long gun stays tracked");
                Check(nearVec(Multiply(ReadRows(oneHand+i*12),bind[i]).origin,Sub(gunPose.trackedHand,cameraOrigin)),
                      "missing left controller does not detach right IK wrist");
            }
            VR().m_controllerPoseValid[0]=true;g_longGunHooksReady=oldLong;
            std::memcpy(VR().m_rawControllerPose,oldHands,sizeof(oldHands));
            g_headingBase=oldHeading;g_bodyVisualOffset=oldFit;testCalibration=oldCalibration;
            g_bodyWallOffset=oldWall;
            config.firstPersonUnarmedIK=true;status=0;gun=1;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
            Detour_GetJoints(itemMemory,palette,0);
        }
        if (count==33) {
            // Real attachments omit unrelated joints; a body mesh can omit
            // wrist bones owned by its separate hand meshes. Preserve the
            // existing frames/binds and remove only the absent mappings.
            int32_t savedMapping[33];std::memcpy(savedMapping,mapping,sizeof(mapping));
            status=4;float complete[33*12]{};Detour_GetJoints(itemMemory,complete,0);
            for(int hand=0;hand<2;++hand) for(bool attachment:{false,true}) {
                const int first=hand ? 8 : 11;
                for(int i=0;i<count;++i) {
                    const int joint=savedMapping[i];
                    mapping[i]=attachment ? (joint==first+1 || joint==first+2 ? joint : -1)
                                          : (joint==10 || joint==13 ? -1 : joint);
                }
                float partial[33*12]{};Detour_GetJoints(itemMemory,partial,0);
                for(int i=0;i<count;++i) if(mapping[i]>=first && mapping[i]<=first+2) {
                    const auto expected=Multiply(ReadRows(complete+i*12),bind[i]);
                    const auto actual=Multiply(ReadRows(partial+i*12),bind[i]);
                    // Native joint queries return integer world units; unlike
                    // the complete float palette, reconstruction is quantized.
                    Check(firstperson::Length(Sub(actual.origin,expected.origin))<1.5f,
                          "partial mesh IK retains the complete arm endpoint within native joint precision");
                    for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                        Check(std::fabs(actual.basis.r[r][c]-expected.basis.r[r][c])<.003f,
                              "partial body/weapon mesh bends its visible segments like the complete skeleton");
                }
            }
            std::memcpy(mapping,savedMapping,sizeof(mapping));status=0;
        }
        // The production gate and render palette must remain active through
        // repeated walk/run transitions with unchanged native shoulder poses.
        for (int weaponStatus:{4,0,4,0}) {
            status=int16_t(weaponStatus);
            Check(weaponStatus==4 ? MotionReady() : UnarmedIKReady(),
                  "equip and holster retain controller arm readiness with unarmed IK enabled");
            float transitionPalette[33*12]{};
            Detour_GetJoints(itemMemory,transitionPalette,0);
            Check(std::memcmp(transitionPalette,baseline,count*12*sizeof(float))!=0,
                  "holstering cannot revert tracked arms to the native animation");
        }
        for(int gait:{0,1,0,1,2}) {
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=int16_t(gait);
            Check(UnarmedIKReady(),"walking and running both retain controller IK");
            float gaitPalette[33*12]{};
            Detour_GetJoints(itemMemory,gaitPalette,0);
            for(int i=0;i<count*12;++i)
                Check(std::fabs(gaitPalette[i]-palette[i])<.003f,"walk/run state change cannot switch or twist tracked hands");
        }
        TestDynamicBonesVisibility(palette);
        // Missing input and native interactions cannot leave stale IK behind.
        const auto heldWall=g_bodyWallOffset;
        for(int reason=0;reason<10;++reason) {
            // Anchor clears clearance on entry to the native ledge camera.
            g_bodyWallOffset=reason==7 ? locomotion::Vec{} : heldWall;
            status=0;gun=1;g_active=true;appMemory[0x9e4]=1;config.firstPersonUnarmedIK=true;
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
            VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
            const float originalX=VR().m_rawControllerPose[0].m[0][3];
            if(reason==0) VR().m_controllerPoseValid[0]=false;
            if(reason==1) VR().m_controllerPoseValid[1]=false;
            if(reason==2) status=4;
            if(reason==3) status=2;
            if(reason==4) gun=7;
            if(reason==5) appMemory[0x9e4]=0;
            if(reason==6) config.firstPersonUnarmedIK=false;
            if(reason==7) *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=10;
            if(reason==8) g_active=false;
            if(reason==9) VR().m_rawControllerPose[0].m[0][3]=NAN;
            Check(!UnarmedIKReady(),"IK gate rejects loss, invalid pose, guns, classic, interactions and third person");
            Detour_GetJoints(itemMemory,palette,0);
            if (reason!=2) Check(!std::memcmp(palette,(reason==7 || reason==8) ? fixture : baseline,count*12*sizeof(float)),"rejected IK leaves original body pose without stale controllers");
            else Check(std::memcmp(palette,baseline,count*12*sizeof(float))!=0,"equipping changes to exact armed IK instead of disabling the arms");
            VR().m_rawControllerPose[0].m[0][3]=originalX;
        }
        status=0;gun=1;g_active=true;appMemory[0x9e4]=1;config.firstPersonUnarmedIK=true;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state)=2;
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        ShadowDll shadow{};static int pass=4;shadow.renderPass=rva(&pass);g_shadowDll=&shadow;
        Detour_GetJoints(itemMemory,palette,0);
        Check(!std::memcmp(palette,fixture,count*12*sizeof(float)),"active IK cannot move any native shadow bone");
        g_shadowDll=nullptr;
        Detour_GetJoints(laraMemory,palette,0);
        Check(!std::memcmp(palette,fixture,count*12*sizeof(float)),"active IK cannot affect another creature");
    }
    TestUnarmedHandMeshes();
    skinFixture=nullptr;skinFixtureCount=0;g_bodySkinScope=g_bodySkinReady=false;
    g_hGetJoints.m_trampoline=savedGetJoints;g_shadowDll=savedShadow;
    g_scenePose=savedPose;g_headingBase=savedHeading;g_bodyVisualOffset=savedOffset;
    g_bodyWallOffset=savedWall;
    std::memcpy(VR().m_rawControllerPose,controllers,sizeof(controllers));
    std::memcpy(itemMemory,savedItem.data(),sizeof(itemMemory));std::memcpy(laraMemory,savedLara.data(),sizeof(laraMemory));
    config=settings;dll=savedDll;testCalibration=savedCalibration;
    g_unarmedTwist[0]=savedLeftTwist;g_unarmedTwist[1]=savedRightTwist;
}
}
