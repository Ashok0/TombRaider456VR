// Real render-hook fixtures plus independent physical constraints for full-body IK.
#include "flare_regression.inl"
namespace {
tr::motiongun::Vec bodyJointWorld[15]{};
tr::motiongun::Basis bodyJointRotation{{{1,0,0},{0,1,0},{0,0,1}}};
void __cdecl FakeBodyJoint(uint8_t* item,tr::PHD_VECTOR* point,int joint,int) {
    Check(item==itemMemory && joint>=0 && joint<15,"body IK reads only native Lara joints");
    const auto p=bodyJointWorld[joint];
    const auto offset=tr::motiongun::Transform(bodyJointRotation,{float(point->x),float(point->y),float(point->z)});
    point->x=int(p.x+offset.x);point->y=int(p.y+offset.y);point->z=int(p.z+offset.z);
}
int fullBodyDraws=0,fullBodyGunDraws=0;
float pistolBodyPalette[33*12]{},pistolForearmPalette[33*12]{};
void __cdecl ObservePistolArmDraw(uint8_t* item,int32_t,int32_t pass) {
    using namespace tr;
    float palette[33*12]{};
    const int count=Detour_GetJoints(item,palette,pass);
    if(g_renderArm>=0 && PistolBodyPoseMatches(item,pass)) {
        const auto* obj=Ptr<uint8_t>(g_boundDll->objects)+
            *reinterpret_cast<int16_t*>(item+off::item_object)*off::object_stride;
        const auto* geom=obj+off::object_geom;
        const int bones=*reinterpret_cast<const int32_t*>(geom+28);
        const auto* mapping=bones>0 ? *reinterpret_cast<const int32_t* const*>(geom+48) : nullptr;
        const auto* poses=bones>0 ? *reinterpret_cast<const float* const*>(geom+72) : nullptr;
        motiongun::Frame wrist{};
        Check(ReadIKJoint(obj,palette,count,bones,mapping,poses,g_renderArm ? 10 : 13,wrist),"pistol draw exposes its wrist");
        Check(firstperson::Length(motiongun::Sub(wrist.origin,g_pistolArmPose.joint[g_renderArm ? 2 : 5].origin))<.04f,
              "both pistol hands stay on solved forearms despite independent native wrist interpolation");
    }
    if(g_pistolBodyScope) std::memcpy(pistolBodyPalette,palette,count*12*sizeof(float));
    if(g_pistolForearmScope) {
        std::memcpy(pistolForearmPalette,palette,count*12*sizeof(float));
        uint64_t mask=0;int captured=0;
        const auto* physics=FirstPersonBodyPalette(mask,captured);
        Check(physics && captured==count,"pistol attachment retains its native physics palette");
        float nativePalette[33*12]{};std::memcpy(nativePalette,skinFixture,count*12*sizeof(float));
        if(BodyVisualOffsetApplies(item)) stabilization::OffsetBodyPalette(nativePalette,count,BodyRenderOffset());
        Check(!std::memcmp(physics,nativePalette,count*12*sizeof(float)),
              "sharing pistol render arms cannot feed controller motion into native physics");
    }
}
uint32_t restrictedBodyMask=UINT32_MAX;
const int32_t* restrictedMapping=nullptr;
int restrictedBodyDraws=0;
void __cdecl ObserveRestrictedBodyDraw(uint8_t* item,int32_t masked,int32_t) {
    using namespace tr;
    const auto bits=*reinterpret_cast<uint32_t*>(item+off::item_mesh_bits);
    Check(masked==1 && (bits&~restrictedBodyMask)==0,
          "actual HD draw cannot restore hidden chest or lower body through IK or guns");
    if(g_bodySkinScope) {
        ++restrictedBodyDraws;
        float palette[33*12]{};Detour_GetJoints(item,palette,0);
        uint64_t visible=0;int count=0;
        Check(FirstPersonBodyPalette(visible,count) && count==33 &&
              visible==firstperson::SkinJointMask(bits,restrictedMapping,count),
              "uploaded HD skin mask follows draw restriction while retaining complete solver skeleton");
    }
}
void __cdecl ObserveFullBodyDraw(uint8_t* item,int32_t masked,int32_t) {
    using namespace tr;
    Check(masked==1,"full-body eye passes use an explicit visibility mask");
    const auto bits=*reinterpret_cast<uint32_t*>(item+off::item_mesh_bits);
    if(g_renderArm<0) {
        if(bits && !(bits&~0x1200u)) {
            Check(g_bodySkinScope,"separate weapon forearms retain their own IK skin palette");
            return;
        }
        ++fullBodyDraws;
        Check((bits&0xff)==0xff && (bits&0x1b00)==0x1b00 && !(bits&0x6400),
              "armed body includes torso legs and arm chains, excludes head and duplicate gun hands");
        Check(g_bodySkinScope,"armed body captures its own complete physics skeleton");
    } else { ++fullBodyGunDraws;Check(bits==(g_renderArm ? 0x400u : 0x2000u),"gun draws retain independent tracked hand masks"); }
}
void TestFullBodyIK() {
    using namespace tr;using namespace tr::motiongun;
    const auto settings=config;const auto savedDll=dll;const auto calibration=testCalibration;
    const auto savedPistolRoll=g_pistolRoll;g_pistolRoll={};
    const auto savedFollower=g_bodyFollower;const auto savedSerial=VR().m_poseSerial;
    const auto savedTorso=g_torsoPose;const auto savedMount=g_mountBodyTransition;
    const bool savedLanding=g_bodyLanding;g_bodyLanding=false;
    const auto savedPistolAnimation=g_pistolAnimation;g_pistolAnimation.Reset();
    const auto savedVisibility=g_bodyVisibility;g_bodyVisibility={};
    const auto savedAuto=g_autoBodyCalibration;g_autoBodyCalibration.pending=false;
    g_torsoPose.Reset();
    const auto savedBodySize=g_bodySize;const int savedSizeObject=g_bodySizeObject;
    const float savedNeckHeight=VR().m_firstPersonNeutralEyeNeckY;
    const auto savedHeightCrouch=g_heightCrouch;const bool savedPhysicalCamera=g_physicalCrouchCamera;
    g_bodyFollower.Reset();
    const auto savedItem=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
    const auto savedLara=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
    const auto savedPose=g_scenePose;const auto savedHeading=g_headingBase;const auto savedOffset=g_bodyVisualOffset;
    const auto getJoints=g_hGetJoints.m_trampoline,draw=g_hDrawCreatureHD.m_trampoline;
    const auto shadowDll=g_shadowDll;const auto savedHead=VR().m_rawHeadPose;
    const auto savedLeft=g_unarmedTwist[0],savedRight=g_unarmedTwist[1];
    vr::HmdMatrix34_t controllers[2];std::memcpy(controllers,VR().m_rawControllerPose,sizeof(controllers));
    auto rva=[](const void* p) { return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase); };
    alignas(16) static uint8_t objectInfo[off::object_stride]{};
    dll.objects=rva(objectInfo);dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeBodyJoint));dll.gLaraHeads=0;
    config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=true;
    config.firstPersonBodyScale=1; // Existing geometry invariants use native size.
    config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=config.firstPersonInteractionAnchorZ=0;
    config.firstPersonJoint=14;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    *reinterpret_cast<int16_t*>(itemMemory+off::item_object)=0;
    state=2;status=0;gun=1;bits=0x3fff;
    {
        const float height=VR().m_rawHeadPose.m[1][3];
        config.firstPersonPhysicalCrouch=true;
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-.36f;
        Check(FirstPersonPhysicalCrouch(true),"calibrated HMD height can request native crouch");
        VRSystem::HandState hands[2]{};XState input{};bool shifted=false,action=false;
        BuildStateFromHands(hands,input,shifted,true,action);
        Check((input.Gamepad.wButtons&XB_LEFT_SHOULDER)!=0,"physical crouch uses the game's existing Duck binding");
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-.28f;
        Check(FirstPersonPhysicalCrouch(true),"head height inside hysteresis band keeps native crouch held");
        Check(!FirstPersonPhysicalCrouch(false),"menus release physical crouch input");
        config.firstPersonPhysicalCrouch=false;VR().m_rawHeadPose.m[1][3]=height;
        g_heightCrouch={};g_physicalCrouchCamera=false;
    }
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    g_renderArm=-1;g_bodySkinScope=true;g_shadowDll=nullptr;
    const auto identity=firstperson::IdentityBasis();
    Frame native[15]{};
    const Vec positions[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    for(int i=0;i<15;++i) native[i]={identity,positions[i]};
    auto nearVec=[](Vec a,Vec b) { return firstperson::Length(Sub(a,b))<.04f; };
    // A fixed controller near the shoulder used to magnify a .057-degree
    // torso change into roughly 20 degrees of material roll. Test both sides,
    // reversal of the head glance, repeated draws, and exact wrist tracking.
    for(int hand=0;hand<2;++hand) {
        const float side=hand ? 1.f : -1.f;
        const Frame armBind[3]={{identity,{side*180,-700,0}},
            {identity,{side*240,-460,30}},{identity,{side*180,-250,70}}};
        const Frame torsoBind{identity,{0,-650,-60}};
        const Frame target{Multiply(firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},side*3)),motiongun::GunBasis(identity)),
            {side*180,-700,50}};
        firstperson::ArmRollContinuity history{};
        Frame previous[3]{};bool havePrevious=false;
        for(int sample=0;sample<=1400;++sample) {
            const int step=sample<=700 ? sample-350 : 1050-sample;
            Frame torso{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},step*.001f)),torsoBind.origin};
            Frame chain[3]{},changes[3]{};auto seed=history;
            Check(firstperson::PistolArmReference(torso,torsoBind,armBind,1,chain),"glance fixture builds pistol model reference");
            Check(firstperson::SolveTrackedArm(chain,target,{side*.6f,1,-.25f},armBind,changes,true,&history),"pistol glance roll solve succeeds");
            for(int j=0;j<3;++j) {
                const auto actual=Multiply(changes[j],chain[j]);
                if(havePrevious && j<2) {
                    const auto q=firstperson::RotationProduct(firstperson::RotationOf(actual.basis),
                        firstperson::RotationInverse(firstperson::RotationOf(previous[j].basis)));
                    const float angle=2*std::atan2(firstperson::Length(q.v),std::fabs(q.w));
                    Check(angle<.07f,"small shoulder glance cannot cause a pistol arm roll spike");
                }
                if(j==2) {
                    Check(nearVec(actual.origin,target.origin),"glance protection leaves pistol wrist position exact");
                    for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                        Check(std::fabs(actual.basis.r[r][c]-target.basis.r[r][c])<.001f,"glance protection leaves pistol aim and wrist roll exact");
                }
                previous[j]=actual;
            }
            Frame repeat[3]{};
            Check(firstperson::SolveTrackedArm(chain,target,{side*.6f,1,-.25f},armBind,repeat,true,&seed),"same-pose pistol redraw succeeds");
            Check(!std::memcmp(repeat,changes,sizeof(repeat)),"same-pose redraw cannot advance pistol roll history");
            havePrevious=true;
        }
    }
    // Model-proportion pistol arms must still obey the rigid-reach contract
    // through full wrist rolls, mirrored arms and calibrated body scales.
    for(int hand=0;hand<2;++hand) for(float size:{.8f,1.f,1.2f}) for(int roll=-8;roll<=8;++roll) {
        const int first=hand ? 8 : 11;
        const Frame armBind[3]={native[first],native[first+1],native[first+2]};
        Frame chain[3]{},changes[3]{};
        Check(firstperson::PistolArmReference(native[7],native[7],armBind,size,chain),"model-proportion pistol reference is valid");
        const Frame target{firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},roll*.4f)),
                           Add(chain[0].origin,{hand ? 100.f : -100.f,0,900.f})};
        Check(firstperson::SolveTrackedArm(chain,target,{hand ? .6f : -.6f,1,-.25f},armBind,changes),
              "pistol reaches beyond anatomical range with full controller roll");
        Frame output[3]{};for(int j=0;j<3;++j) output[j]=Multiply(changes[j],chain[j]);
        Check(nearVec(output[2].origin,target.origin),"pistol wrist follows controller immediately");
        for(int j=0;j<2;++j) {
            Check(std::fabs(firstperson::Length(Sub(output[j+1].origin,output[j].origin))-
                size*firstperson::Length(Sub(armBind[j+1].origin,armBind[j].origin)))<.04f,
                "pistol reaches by shoulder travel without changing model bone lengths");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
                float dot=0;for(int k=0;k<3;++k) dot+=output[j].basis.r[k][r]*output[j].basis.r[k][c];
                Check(std::fabs(dot-(r==c ? size*size : 0.f))<.003f,"pistol arm meshes keep calibrated scale without shear");
            }
        }
    }
    // Crouch/lean/yaw across a wide range, unequal legs, straight legs and
    // split strides: feet cannot drift and segment lengths cannot change.
    for(int stride=0;stride<4;++stride) for(int x=-6;x<=6;++x) for(int y=-4;y<=12;++y) {
        Frame input[15],output[15];std::memcpy(input,native,sizeof(input));
        input[3].origin.z+=stride*75.f;input[6].origin.z-=stride*40.f;
        if(stride==3) { input[2].origin={-100,-250,0};input[3].origin={-100,0,0}; }
        const auto rotate=firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},x*.2f));
        Check(firstperson::SolveBody(input,{x*60.f,y*50.f,120},rotate,{0,0,1},1000,output),"body solves crouch lean turn and split-stride poses");
        Check(nearVec(output[3].origin,input[3].origin) && nearVec(output[6].origin,input[6].origin),"both feet retain native world contact");
        for(int first:{1,4}) {
            for(int j=0;j<2;++j) {
                const auto before=Sub(input[first+j+1].origin,input[first+j].origin);
                const auto after=Sub(output[first+j+1].origin,output[first+j].origin);
                Check(std::fabs(firstperson::Length(before)-firstperson::Length(after))<.05f,"leg IK preserves bone lengths");
                Frame inv{};Inverse(input[first+j],inv);
                Check(nearVec(Transform(Multiply(output[first+j],inv),input[first+j+1].origin),output[first+j+1].origin),"leg skinning joins thigh knee and ankle without gaps");
            }
            Check(nearVec(Sub(output[first].origin,output[0].origin),Sub(input[first].origin,input[0].origin)),"hip sockets stay attached to pelvis");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                Check(std::fabs(output[first+2].basis.r[r][c]-input[first+2].basis.r[r][c])<.001f,"foot orientation retains native ground/stride alignment");
        }
        Check(std::fabs(firstperson::Length(Sub(output[14].origin,output[7].origin))-200)<.04f,"torso lean never stretches the spine");
    }
    for(float yaw:{-3.14f,-1.7f,0.f,.9f,3.14f}) for(float height:{-900.f,-200.f,0.f,200.f,450.f,900.f}) {
        Frame posed[15]{};const Vec offset{11,-32,144};const Vec anchor=Add(native[14].origin,offset);
        const Vec eye=Add(anchor,{620,height,-430});
        Check(firstperson::SolveBodyAtEye(native,14,anchor,eye,yaw,identity,{std::sin(yaw),0,std::cos(yaw)},posed),
              "centered solver accepts physical height and translation sweep");
        Check(nearVec(Transform(posed[14],offset),eye),"centered solver pins the eye even beyond leg reach");
        for(int first:{1,4}) for(int segment=0;segment<2;++segment) {
            const float originalLength=firstperson::Length(Sub(native[first+segment+1].origin,native[first+segment].origin));
            Check(std::fabs(firstperson::Length(Sub(posed[first+segment+1].origin,posed[first+segment].origin))-originalLength)<.04f,
                  "centering never stretches legs to reach the old footprint");
            Frame inv{};Inverse(native[first+segment],inv);
            Check(nearVec(Transform(Multiply(posed[first+segment],inv),native[first+segment+1].origin),posed[first+segment+1].origin),
                  "centered leg skinning remains connected at knee and ankle");
        }
        Frame consumed[15]{},afterConsume[15]{};
        const Vec acceptedStep{400,0,-650};
        for(int i=0;i<15;++i) { consumed[i]=native[i];consumed[i].origin=Add(consumed[i].origin,acceptedStep); }
        Check(firstperson::SolveBodyAtEye(consumed,14,Add(anchor,acceptedStep),eye,yaw,identity,
              {std::sin(yaw),0,std::cos(yaw)},afterConsume),"body solves after native root consumes roomscale displacement");
        for(int i=0;i<15;++i) Check(nearVec(afterConsume[i].origin,posed[i].origin),
              "consuming roomscale motion cannot add a second rendered body offset");
        if(height==0 || height==200) for(int foot:{3,6})
            Check(Near(posed[foot].origin.y,native[foot].origin.y),"reachable standing/crouched legs retain floor height");
    }
    Frame crouched[15]{};
    Check(firstperson::SolveBody(native,{120,220,160},identity,{0,0,1},1000,crouched) &&
          crouched[0].origin.y>native[0].origin.y+100 && crouched[7].origin.z>native[7].origin.z,
          "headset crouch lowers pelvis and leaning moves the chest");
    Frame broken[15];std::memcpy(broken,native,sizeof(broken));broken[2]=broken[1];
    Check(!firstperson::SolveBody(broken,{},identity,{0,0,1},1000,crouched),"degenerate leg safely rejects full-body solve");
    Check(!firstperson::SolveBody(native,{NAN,0,0},identity,{0,0,1},1000,crouched),"invalid head cannot poison skeleton");
    // Authored spines are not a straight arm chain: retain this curvature in
    // the actual render-hook fixtures as well as the standalone solver tests.
    native[7].origin.z=-60;
    const auto& rootPose=*reinterpret_cast<const PHD_3DPOS*>(itemMemory+off::item_pos);
    const Vec root{float(rootPose.x_pos),float(rootPose.y_pos),float(rootPose.z_pos)},camera=Add(root,{300,-200,-500});
    for(int j=0;j<15;++j) bodyJointWorld[j]=Add(root,native[j].origin);
    g_scenePose.x_pos=int(root.x+80);g_scenePose.y_pos=int(root.y-700);g_scenePose.z_pos=int(root.z+90);
    for(int hand=0;hand<2;++hand) {
        auto& m=VR().m_rawControllerPose[hand];m={};m.m[0][0]=m.m[1][1]=m.m[2][2]=1;
        m.m[0][3]=VR().m_rawHeadPose.m[0][3]+(hand ? .25f : -.25f);
        m.m[1][3]=VR().m_firstPersonNeutral[1]-.30f;m.m[2][3]=VR().m_rawHeadPose.m[2][3]-.30f;
    }
    for(int count:{15,33}) {
        float fixture[33*12]{},poses[33*12]{},palette[33*12]{},baseline[33*12]{};
        int32_t mapping[33]{};Frame bind[33]{};
        auto* geom=objectInfo+off::object_geom;
        *reinterpret_cast<int32_t*>(geom+28)=count==15 ? 0 : count;
        *reinterpret_cast<int32_t**>(geom+48)=mapping;*reinterpret_cast<float**>(geom+72)=poses;
        for(int i=0;i<count;++i) {
            const int joint=count==15 ? i : (i*7)%15;mapping[i]=joint;
            const Frame frame{identity,Sub(bodyJointWorld[joint],camera)};
            const Frame ib{CalibratedController(identity,{0,0,0,float(i*3),float(i*2),float(-i)}),{float(i*4),float(i*7),float(-i*2)}};
            Check(Inverse(ib,bind[i]),"body fixture inverse bind is invertible");
            if(count==15) {
                auto* m=reinterpret_cast<float*>(objectInfo+1676)+i*16;
                for(int r=0;r<3;++r) for(int c=0;c<3;++c) m[c*4+r]=ib.basis.r[r][c];
                m[12]=ib.origin.x;m[13]=ib.origin.y;m[14]=ib.origin.z;m[15]=1;
            } else {
                auto raw=ib;for(int r=0;r<3;++r) { raw.basis.r[r][1]=ib.basis.r[r][2];raw.basis.r[r][2]=-ib.basis.r[r][1]; }
                WriteRows(raw,poses+i*12);
            }
            WriteRows(Multiply(frame,ib),fixture+i*12);
        }
        skinFixture=fixture;skinFixtureCount=count;
        TestFlareTracking(count,bind,camera);
        {
            const bool cameraValid=worldCameraValid;
            float oldCamera[3];std::memcpy(oldCamera,worldCameraPos,sizeof(oldCamera));
            const auto oldHeadJoint=bodyJointWorld[14],oldLeftJoint=bodyJointWorld[13],oldRightJoint=bodyJointWorld[10];
            const auto oldScope=g_pistolBodyScope;
            worldCameraValid=true;worldCameraPos[0]=camera.x;worldCameraPos[1]=camera.y;worldCameraPos[2]=camera.z;
            // Real HD interpolation is floating point; the gameplay joint
            // query is integer and need not agree frame-for-frame. A mismatch
            // is NOT camera movement, even when every tracked pose is fixed.
            for(int which:{0,1}) for(bool armed:{false,true}) {
                dll.game=which;state=2;status=armed ? 4 : 0;gun=1;
                g_pistolBodyScope=armed;g_bodyVisualOffset={};
                worldCameraPos[0]=camera.x;worldCameraPos[1]=camera.y;worldCameraPos[2]=camera.z;
                g_torsoPose.Reset();g_bodyFollower.Reset();g_pistolRoll={};
                ++VR().m_poseSerial;Detour_GetJoints(itemMemory,baseline,0);
                for(int n=1;n<=120;++n) {
                    float animated[33*12]{};std::memcpy(animated,fixture,sizeof(animated));
                    const float noise=.45f*std::sin(n*1.7f);
                    const Vec cameraShift=n>60 ? Vec{(n-60)*2.4f,(n-60)*.3f,-(n-60)*1.7f} : Vec{};
                    worldCameraPos[0]=camera.x+cameraShift.x;
                    worldCameraPos[1]=camera.y+cameraShift.y;
                    worldCameraPos[2]=camera.z+cameraShift.z;
                    for(int i=0;i<count;++i) {
                        auto frame=Multiply(ReadRows(fixture+i*12),bind[i]);
                        frame.origin=Sub(frame.origin,cameraShift);
                        if(mapping[i]==14) frame.origin=Add(frame.origin,{noise,noise*.6f,-noise*.8f});
                        Frame inverse{};Inverse(bind[i],inverse);
                        WriteRows(Multiply(frame,inverse),animated+i*12);
                    }
                    bodyJointWorld[14]=Add(oldHeadJoint,{float(n%3-1),0,0});
                    bodyJointWorld[13]=Add(oldLeftJoint,{0,float(n%3-1),0});
                    bodyJointWorld[10]=Add(oldRightJoint,{0,0,float(n%3-1)});
                    skinFixture=animated;++VR().m_poseSerial;g_torsoPose.time=TurnTime()-1./60.;
                    Detour_GetJoints(itemMemory,palette,0);
                    for(int i=0;i<count;++i) {
                        const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                        const auto initial=Multiply(ReadRows(baseline+i*12),bind[i]);
                        Check(firstperson::Length(Sub(Add(actual.origin,cameraShift),initial.origin))<.025f,
                              "stationary full-body world positions ignore joint query noise and follow real camera rebasing exactly");
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                            Check(std::fabs(actual.basis.r[r][c]-initial.basis.r[r][c])<.001f,
                                  "stationary body and limb orientation stays stable through joint query mismatch");
                    }
                }
                bodyJointWorld[14]=oldHeadJoint;bodyJointWorld[13]=oldLeftJoint;bodyJointWorld[10]=oldRightJoint;
                skinFixture=fixture;
            }
            g_pistolBodyScope=oldScope;worldCameraValid=cameraValid;
            std::memcpy(worldCameraPos,oldCamera,sizeof(oldCamera));
            g_torsoPose.Reset();g_bodyFollower.Reset();
        }
        // Weapon status must not switch the torso back to native placement.
        // Native draw/holster arms remain attached to that same fitted chest.
        for(int which:{0,1}) {
            dll.game=game=which;state=2;gun=1;status=0;
            auto& anim=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
            const auto oldAnim=anim;anim=103;
            g_bodyFollower.Reset();g_torsoPose.Reset();g_bodyLanding=false;
            ++VR().m_poseSerial;Detour_GetJoints(itemMemory,baseline,0);
            for(int next:{2,2,4,3,3,0}) {
                status=int16_t(next);++VR().m_poseSerial;
                g_pistolAnimation.Reset(); // This fixture checks the settled native transition pose.
                Check(FullBodyIKReady(),"body solver stays available through draw and holster");
                Detour_GetJoints(itemMemory,palette,0);
                Frame nativeChest{},renderedChest{};
                for(int i=0;i<count;++i) if(mapping[i]==7) {
                    Inverse(Multiply(ReadRows(fixture+i*12),bind[i]),nativeChest);
                    Inverse(Multiply(ReadRows(palette+i*12),bind[i]),renderedChest);
                    break;
                }
                for(int i=0;i<count;++i) {
                    if(mapping[i]<8) Check(nearVec(ReadRows(palette+i*12).origin,ReadRows(baseline+i*12).origin),
                        "draw and holster cannot move the rendered torso or legs forward");
                    if((next==2 || next==3) && mapping[i]>=8 && mapping[i]<=13) {
                        const auto actual=Multiply(renderedChest,Multiply(ReadRows(palette+i*12),bind[i]));
                        const auto expected=Multiply(nativeChest,Multiply(ReadRows(fixture+i*12),bind[i]));
                        Check(nearVec(actual.origin,expected.origin),
                              "native weapon animation keeps its arm placement relative to fitted chest");
                    }
                }
                if(next==2 || next==3) Check(!MotionReady(),"rendering transition body does not enable weapon firing or tracked gun takeover");
            }
            const auto savedInventory=g_worldInventory;
            const bool ready=inventorySceneReady,drawing=inventorySceneDrawing;
            const auto cfg=config;config.worldInventory=true;config.monoTracking=false;config.gamepadEnabled=true;
            auto& menu=*reinterpret_cast<int32_t*>(appMemory+drva::app_off::InventoryActive);
            const int oldMenu=menu;menu=1;inventorySceneReady=true;
            g_worldInventory={};g_worldInventory.open=true;g_worldInventory.game=game;
            for(bool drawWorld:{false,true,false,true}) {
                inventorySceneDrawing=drawWorld;
                Check(!Gate() && !MotionTriggerMode(),"inventory keeps gameplay and weapon input suspended during world rendering");
                Check(FullBodyIKReady()==drawWorld,"only the inventory world draw retains the first-person body solver");
                if(drawWorld) {
                    ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                    for(int i=0;i<count;++i) if(mapping[i]<8)
                        Check(nearVec(ReadRows(palette+i*12).origin,ReadRows(baseline+i*12).origin),
                              "entering inventory ring cannot reveal the unfitted body position");
                }
            }
            menu=oldMenu;g_worldInventory=savedInventory;
            inventorySceneReady=ready;inventorySceneDrawing=drawing;config=cfg;
            // Landing camera/root changes must not carry a kneeling torso into
            // the first standing palette, even within the same tracking sample.
            const auto oldScene=g_scenePose;
            float landingFixture[33*12];std::memcpy(landingFixture,fixture,sizeof(fixture));
            for(int i=0;i<count;++i) landingFixture[i*12+11]+=240;
            skinFixture=landingFixture;anim=82;g_scenePose.z_pos+=240;
            ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
            Check(g_bodyLanding,"landing pose records its separate camera frame");
            skinFixture=fixture;anim=103;g_scenePose=oldScene;
            Detour_GetJoints(itemMemory,palette,0);
            for(int i=0;i<count;++i) if(mapping[i]<8)
                Check(nearVec(ReadRows(palette+i*12).origin,ReadRows(baseline+i*12).origin),
                      "landing completion immediately fits torso and legs to standing eye without a forward flash");
            Check(!g_bodyLanding,"landing frame is discarded after returning to standing");
            anim=oldAnim;
        }
        g_pistolAnimation.Reset();
        // Controller movement cannot override native pistol hands during draw
        // or holster, including equipment requested from physical holsters.
        {
            const auto savedHolsters=g_holsters;const auto cfg=config;
            vr::HmdMatrix34_t oldControllers[2];std::memcpy(oldControllers,VR().m_rawControllerPose,sizeof(oldControllers));
            const auto oldAnim=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=103;
            state=2;gun=1;config.firstPersonBodyHandYawWeight=0;
            for(int next:{2,3}) for(bool physical:{false,true}) {
                status=int16_t(next);g_holsters.active=physical;g_holsters.weapon=1;
                g_bodyFollower.Reset();g_torsoPose.Reset();g_pistolAnimation.Reset();
                ++VR().m_poseSerial;Detour_GetJoints(itemMemory,baseline,0);
                Check(!MotionReady() && FullBodyIKReady(),"pistol animation disables tracked hands while retaining fitted body");
                for(int hand=0;hand<2;++hand) VR().m_rawControllerPose[hand].m[0][3]+=.3f;
                ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                for(int i=0;i<count;++i) if(mapping[i]>=8 && mapping[i]<=13)
                    Check(nearVec(ReadRows(palette+i*12).origin,ReadRows(baseline+i*12).origin),
                          "moving controllers during draw/holster cannot move animated hands");
                std::memcpy(VR().m_rawControllerPose,oldControllers,sizeof(oldControllers));
            }
            g_holsters=savedHolsters;config=cfg;g_pistolAnimation.Reset();
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=oldAnim;
        }
        for(int which:{0,1}) for(int stance:{0,1,2,3,9,15,25,26,27,71,73,10,30,31,19,54}) for(int armed:{0,1}) {
            g_torsoPose.Reset();++VR().m_poseSerial;
            dll.game=which;state=int16_t(stance);status=armed ? 4 : 0;g_bodyVisualOffset={55,-80};
            g_headingBase=.4f;testCalibration={.04f,-.06985f,.2032f,-30.f,8,-6};
            Check(FullBodyIKReady(),"full-body available in both games, grounded, airborne and native grip states");
            if(!armed && !FullBodyGripState(itemMemory)) Check(UnarmedIKReady(),"free jumps retain controller hands");
            config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=false;
            status=0;Detour_GetJoints(itemMemory,baseline,0);status=armed ? 4 : 0;
            config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=true;
            // Call explicitly as well: fallback arms must not conceal a failed body solve.
            std::memcpy(palette,baseline,count*12*sizeof(float));
            const bool airborne=IsNativeAirborneBodyState(itemMemory);
            Check(ApplyFullBodyIK(itemMemory,palette,count)==!airborne,
                  "grounded body uses full IK; airborne body falls through to arm-only IK");
            if(airborne) Check(!std::memcmp(palette,baseline,count*12*sizeof(float)) &&
                !g_bodyFollower.valid && !g_torsoPose.valid,
                "airborne bypass leaves animation untouched and clears standing/mount pose history");
            const auto itemSnapshot=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            const auto laraSnapshot=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
            Detour_GetJoints(itemMemory,palette,0);
            Check(itemSnapshot==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                  laraSnapshot==std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory)),"full-body cannot alter collision, root, velocity or animation state");
            uint64_t mask=0;int captured=0;const auto* physics=FirstPersonBodyPalette(mask,captured);
            Check(physics && captured==count && !std::memcmp(physics,baseline,count*12*sizeof(float)),"full-body keeps complete jiggle anchor palette native");
            const auto* render=FirstPersonRenderBodyPalette(mask,captured);
            Check(render && !std::memcmp(render,palette,count*12*sizeof(float)),"skinning uploads the full-body pose without physics overwriting it");
            for(int i=0;i<count;++i) {
                const int joint=mapping[i];const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                if(airborne && (joint<8 || joint==14))
                    Check(!std::memcmp(palette+i*12,baseline+i*12,12*sizeof(float)),
                          "uploaded airborne hips legs torso and head exactly match native animation");
                if(joint>=8 && joint<=13 && (!FullBodyGripState(itemMemory) || armed)) {
                    for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
                        float dot=0;
                        for(int k=0;k<3;++k) dot+=actual.basis.r[k][r]*actual.basis.r[k][c];
                        Check(std::fabs(dot-(r==c ? 1.f : 0.f))<.003f,
                              "full-body tracked arms retain rigid meshes with calibrated native-size body");
                    }
                }
                if((joint==3 || joint==6) && FullBodyGripState(itemMemory) && !armed) Check(nearVec(actual.origin,Sub(bodyJointWorld[joint],camera)),"constrained grip keeps native animated feet");
                if(joint==10 || joint==13) {
                    Frame target{};Basis controller{};const int hand=joint==10 ? 1 : 0;
                    BuildControllerWrist(hand,target,controller);
                    if (FullBodyGripState(itemMemory) && !armed) {
                        Check(nearVec(actual.origin,Sub(bodyJointWorld[joint],camera)),"full-body preserves native ledge grip positions");
                    } else {
                        const Vec grip=Scale(Vec{-testCalibration.rightMetres,testCalibration.gripForwardMetres,
                                                -testCalibration.raiseMetres},LiveWorldUnitsPerMetre());
                        Check(nearVec(Add(Transform(actual,grip),camera),Transform(target,grip)),
                              "full-body hands keep the same physical grip pivot as floating guns");
                        if (armed) Check(nearVec(Add(actual.origin,camera),target.origin),"armed full-body wrist position remains unchanged");
                    }
                    if(armed) for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                        Check(std::fabs(actual.basis.r[r][c]-target.basis.r[r][c])<.002f,"armed body wrist matches exact gun orientation without unarmed wrist limits");
                }
            }
        }
        // Rapid airborne animation must reach skinning in the same frame,
        // including mapped HD helper bones and a lingering mount recovery.
        for(int which:{0,1}) for(int jump:{3,15,26,27}) for(bool armed:{false,true}) {
            dll.game=which;state=int16_t(jump);status=armed ? 4 : 0;
            for(int frame=0;frame<8;++frame) {
                float animated[33*12]{},expected[33*12]{};
                std::memcpy(animated,fixture,sizeof(animated));
                for(int i=0;i<count;++i) if(mapping[i]<8) {
                    animated[i*12+7]+=(frame%2 ? -150.f : 90.f)*(mapping[i]+1);
                    animated[i*12+11]+=frame*31.f;
                }
                std::memcpy(expected,animated,sizeof(expected));
                if(BodyVisualOffsetApplies(itemMemory))
                    stabilization::OffsetBodyPalette(expected,count,BodyRenderOffset());
                skinFixture=animated;g_torsoPose.recovering=true;++VR().m_poseSerial;
                Detour_GetJoints(itemMemory,palette,0);
                for(int i=0;i<count;++i) if(mapping[i]<8)
                    Check(!std::memcmp(palette+i*12,expected+i*12,12*sizeof(float)),
                          "rapid jump leg animation reaches uploaded palette immediately without IK easing or floating feet");
                Check(!g_torsoPose.recovering,"airborne rendering discards lingering mount-recovery smoothing");
            }
            skinFixture=fixture;
        }
        // Real draw order: body, then independently animated pistol meshes.
        // The attachment may omit torso/shoulders and has different bindings
        // for helper bones. Its forearms must use the body's SAME solved pose.
        {
            const auto savedDraw=g_hDrawCreatureHD.m_trampoline;
            const auto oldSettings=config;const int oldGame=dll.game;
            const bool oldBodyScope=g_bodySkinScope;
            const auto oldBits=bits;
            g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObservePistolArmDraw);
            // Hold tracking and torso still; vary BOTH native arm palettes,
            // including shoulder locations, lengths, roll and nonorthogonal
            // matrix blends. The previous wrist-only fix misses these inputs.
            for(int which:{0,1}) for(bool full:{false,true}) for(bool omitHands:{false,true}) {
                if(count==15 && omitHands) continue;
                g_pistolRoll={};
                dll.game=which;state=2;status=4;gun=1;
                config.firstPersonFullBodyIK=full;config.firstPersonBodyScale=1.15f;
                Frame referenceArms[6]{};
                const auto originalWrist=bodyJointWorld[10];
                const auto originalLeftWrist=bodyJointWorld[13];
                int32_t bodyMapping[33]{};
                for(int i=0;i<count;++i) bodyMapping[i]=omitHands && (mapping[i]==10 || mapping[i]==13) ? -1 : mapping[i];
                *reinterpret_cast<int32_t**>(geom+48)=bodyMapping;
                for(int sample=0;sample<8;++sample) {
                    bodyJointWorld[10]=Add(originalWrist,{sample*2.f,-sample*1.f,sample*3.f});
                    bodyJointWorld[13]=Add(originalLeftWrist,{-sample*3.f,sample*2.f,sample*1.f});
                    float animated[33*12]{};std::memcpy(animated,fixture,sizeof(animated));
                    for(int i=0;i<count;++i) if(mapping[i]>=8 && mapping[i]<=13) {
                        auto frame=Multiply(ReadRows(fixture+i*12),bind[i]);
                        const float phase=sample*.19f*(mapping[i]<11 ? 1.f : -1.f);
                        const auto rotation=firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},phase));
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                            frame.basis.r[r][c]=.5f*(frame.basis.r[r][c]+rotation.r[r][c]);
                        frame.origin=Add(frame.origin,{sample*(mapping[i]-7.f),sample*2.f,-sample*3.f});
                        Frame inverse{};Inverse(bind[i],inverse);
                        WriteRows(Multiply(frame,inverse),animated+i*12);
                    }
                    skinFixture=animated;
                    g_torsoPose.Reset();g_bodyFollower.Reset();++VR().m_poseSerial;
                    bits=0x3fff;Detour_DrawCreatureHD(itemMemory,0,0);
                    Check(g_pistolArmPose.valid,"animated bilateral pistol fixture captures body arms");
                    Frame firstEye[6]{};std::memcpy(firstEye,g_pistolArmPose.joint,sizeof(firstEye));
                    Detour_DrawCreatureHD(itemMemory,0,0);
                    Check(!std::memcmp(firstEye,g_pistolArmPose.joint,sizeof(firstEye)),
                          "second body draw in one tracking sample cannot advance either pistol roll");
                    if(sample==0) std::memcpy(referenceArms,g_pistolArmPose.joint,sizeof(referenceArms));
                    for(int j=0;j<6;++j) {
                        Check(nearVec(referenceArms[j].origin,g_pistolArmPose.joint[j].origin),
                              "native pistol animation cannot move either tracked shoulder elbow or wrist");
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                            Check(std::fabs(referenceArms[j].basis.r[r][c]-g_pistolArmPose.joint[j].basis.r[r][c])<.003f,
                                  "native pistol matrix interpolation cannot wobble or deform either arm mesh");
                    }
                    *reinterpret_cast<int32_t**>(geom+48)=mapping;
                    bits=0x3600;Detour_DrawCreatureHD(itemMemory,1,0);
                    *reinterpret_cast<int32_t**>(geom+48)=bodyMapping;
                }
                bodyJointWorld[10]=originalWrist;
                bodyJointWorld[13]=originalLeftWrist;
                skinFixture=fixture;*reinterpret_cast<int32_t**>(geom+48)=mapping;
            }
            for(int which:{0,1}) for(int renderPass:{0,2}) for(bool full:{false,true}) for(bool partial:{false,true}) for(bool omitBodyHands:{false,true}) {
                if(count==15 && (partial || omitBodyHands)) continue;
                dll.game=which;state=2;status=4;gun=1;
                config.firstPersonFullBodyIK=full;config.firstPersonBodyScale=1.15f;
                g_torsoPose.Reset();g_bodyFollower.Reset();++VR().m_poseSerial;
                skinFixture=fixture;
                int32_t bodyMapping[33]{};
                for(int i=0;i<count;++i) bodyMapping[i]=omitBodyHands && (mapping[i]==10 || mapping[i]==13) ? -1 : mapping[i];
                *reinterpret_cast<int32_t**>(geom+48)=bodyMapping;
                *reinterpret_cast<float**>(geom+72)=poses;
                bits=0x3fff;Detour_DrawCreatureHD(itemMemory,0,renderPass);
                Check(g_pistolArmPose.valid,"main pistol body captures its complete arm solve");
                Frame expected[6]{};
                for(int i=0;i<count;++i) if(bodyMapping[i]>=8 && bodyMapping[i]<=13)
                    expected[mapping[i]-8]=Multiply(ReadRows(pistolBodyPalette+i*12),bind[i]);
                if(omitBodyHands) for(int joint:{10,13}) {
                    expected[joint-8]=g_pistolArmPose.joint[joint-8];
                    Frame target{};Basis controller{};
                    Check(BuildControllerWrist(joint==10 ? 1 : 0,target,controller) &&
                          nearVec(Add(expected[joint-8].origin,camera),target.origin),
                          "body with omitted hands still publishes the exact solved controller wrist");
                }
                for(int sample=0;sample<12;++sample) {
                    float attachment[33*12]{},attachmentPoses[33*12]{};
                    int32_t attachmentMapping[33]{};Frame attachmentBind[33]{};
                    for(int i=0;i<count;++i) {
                        const int joint=mapping[i];attachmentMapping[i]=joint;
                        if(partial && joint!=9 && joint!=10 && joint!=12 && joint!=13) attachmentMapping[i]=-1;
                        Frame inverseBind{};Inverse(bind[i],inverseBind);
                        if(count==33) {
                            inverseBind.basis=Multiply(firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},i*.03f)),inverseBind.basis);
                            inverseBind.origin=Add(inverseBind.origin,{float(i*7),float(-i*3),float(i*2)});
                        }
                        Inverse(inverseBind,attachmentBind[i]);
                        auto raw=inverseBind;
                        for(int r=0;r<3;++r) { raw.basis.r[r][1]=inverseBind.basis.r[r][2];raw.basis.r[r][2]=-inverseBind.basis.r[r][1]; }
                        WriteRows(raw,attachmentPoses+i*12);
                        Frame animated{identity,Sub(bodyJointWorld[joint],camera)};
                        if(joint>=8 && joint<=13) {
                            const float phase=(joint<11 ? 1.f : -1.f)*sample*.11f;
                            animated.basis=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},phase));
                            if(joint==9 || joint==12) animated.origin=Add(animated.origin,{sample*1.5f,0,-sample*.7f});
                            if(joint==10 || joint==13) animated.origin=Add(animated.origin,{sample*2.f,-sample*1.f,sample*3.f});
                        }
                        WriteRows(Multiply(animated,inverseBind),attachment+i*12);
                    }
                    skinFixture=attachment;
                    *reinterpret_cast<int32_t**>(geom+48)=attachmentMapping;
                    *reinterpret_cast<float**>(geom+72)=attachmentPoses;
                    bits=0x3600;
                    const auto itemBefore=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
                    const auto laraBefore=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
                    const auto torsoBefore=std::string(reinterpret_cast<char*>(&g_torsoPose),sizeof(g_torsoPose));
                    GunPose gunsBefore[2]{};
                    for(int hand=0;hand<2;++hand) Check(BuildGunPose(hand,gunsBefore[hand]),"pistol fixture has a tracked gun pose");
                    Detour_DrawCreatureHD(itemMemory,1,renderPass);
                    for(int i=0;i<count;++i) if(attachmentMapping[i]>=8 && attachmentMapping[i]<=13) {
                        const auto actual=Multiply(ReadRows(pistolForearmPalette+i*12),attachmentBind[i]);
                        const auto& wanted=expected[attachmentMapping[i]-8];
                        Check(nearVec(actual.origin,wanted.origin),"animated pistol attachment cannot move an IK elbow or wrist away from the body");
                        for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                            Check(std::fabs(actual.basis.r[r][c]-wanted.basis.r[r][c])<.003f,
                                  "pistol forearm uses body roll and calibrated scale despite native weapon animation");
                    }
                    Check(itemBefore==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                          laraBefore==std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory)),
                          "pistol arm rendering never changes native aim recoil or firing state");
                    Check(torsoBefore==std::string(reinterpret_cast<char*>(&g_torsoPose),sizeof(g_torsoPose)),
                          "pistol attachment cannot run another body solve or advance torso filtering");
                    for(int hand=0;hand<2;++hand) {
                        GunPose after{};
                        Check(BuildGunPose(hand,after) && !std::memcmp(&gunsBefore[hand],&after,offsetof(GunPose,meshAligned)) &&
                              gunsBefore[hand].meshAligned==after.meshAligned,
                              "shared pistol arms retain exact tracked gun pose muzzle and aim");
                    }
                }
                const int oldFraction=fraction;
                for(int reason=0;reason<4;++reason) {
                    fraction=oldFraction;gun=1;
                    if(reason==0) ++fraction;
                    if(reason==1) gun=3;
                    float unchanged[33*12]{};std::memcpy(unchanged,fixture,count*12*sizeof(float));
                    Check(!PistolBodyArmPose(reason==2 ? laraMemory : itemMemory,unchanged,count,reason==3 ? renderPass^1 : renderPass,false) &&
                          !std::memcmp(unchanged,fixture,count*12*sizeof(float)),
                          "pistol pose rejects different interpolation weapon creature and render pass atomically");
                }
                fraction=oldFraction;gun=1;
                ++VR().m_poseSerial;
                float untouched[33*12]{};std::memcpy(untouched,fixture,count*12*sizeof(float));
                Check(!PistolBodyArmPose(itemMemory,untouched,count,renderPass,false) &&
                      !std::memcmp(untouched,fixture,count*12*sizeof(float)),"next tracking sample cannot reuse stale pistol arms");
            }
            *reinterpret_cast<int32_t**>(geom+48)=mapping;*reinterpret_cast<float**>(geom+72)=poses;
            skinFixture=fixture;config=oldSettings;dll.game=oldGame;
            g_hDrawCreatureHD.m_trampoline=savedDraw;g_pistolArmPose={};
            g_bodySkinScope=oldBodyScope;
            bits=oldBits;
        }
        state=2;status=0;
        // Reproduce the reported drift: the actual rendered anchor must
        // follow roomscale translation/height and stick + physical yaw exactly.
        {
            const auto savedSettings=config;const auto oldHead=VR().m_rawHeadPose;
            const auto oldView=VR().m_headFromTracking;const auto oldScene=g_scenePose;
            const auto oldBase=g_headingBase;const auto oldFit=g_bodyVisualOffset;
            const auto oldPos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
            const auto oldPrev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
            const int oldFraction=fraction;
            const float oldNeckHeight=VR().m_firstPersonNeutralEyeNeckY;
            float unturned[33*12]{};std::memcpy(unturned,fixture,sizeof(fixture));
            float neutral[3],neck[2];std::memcpy(neutral,VR().m_firstPersonNeutral,sizeof(neutral));
            std::memcpy(neck,VR().m_firstPersonNeutralNeck,sizeof(neck));
            vr::HmdMatrix34_t oldHands[2];std::memcpy(oldHands,VR().m_rawControllerPose,sizeof(oldHands));
            config.firstPersonMovementStabilization=config.flipViewY=true;
            config.firstPersonAnchorX=11;config.firstPersonAnchorY=-32;config.firstPersonAnchorZ=144;
            auto& current=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
            auto& previous=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
            current.y_rot=previous.y_rot=0;
            Head(0);VR().RecenterFirstPersonHead();
            {
                const auto beforeScene=g_scenePose;const auto beforeFit=g_bodyVisualOffset;
                const bool beforeManual=g_haveManualInput;
                const auto beforeStance=g_stanceEye;
                g_haveManualInput=false;g_bodyVisualOffset={};g_headingBase=0;g_bodyFollower.Reset();
                config.firstPersonBodyScale=0;g_bodySize.Reset();g_stanceEye={};
                g_torsoPose.Reset();g_mountBodyTransition.Reset();
                g_scenePose.x_pos=int(bodyJointWorld[14].x+config.firstPersonAnchorX);
                g_scenePose.y_pos=int(bodyJointWorld[14].y+config.firstPersonAnchorY+160);
                g_scenePose.z_pos=int(bodyJointWorld[14].z+config.firstPersonAnchorZ);
                for(int hand=0;hand<2;++hand) {
                    auto& m=VR().m_rawControllerPose[hand];m=VR().m_rawHeadPose;
                    m.m[0][3]+=(hand ? .25f : -.25f);m.m[1][3]-=.3f;m.m[2][3]-=.4f;
                }
                state=3;++VR().m_poseSerial;Detour_GetJoints(itemMemory,baseline,0);
                Check(!g_bodySize.valid,"jump poses cannot become standing size calibration");
                state=2;++VR().m_poseSerial;Detour_GetJoints(itemMemory,baseline,0);
                Check(g_bodySize.valid && g_bodySize.factor<.85f,
                      "real render hook automatically sizes an oversized body to the comfortable eye height");
                const float calibratedSize=g_bodySize.factor;
                Frame fitted[15]{};
                for(int i=0;i<count;++i) fitted[mapping[i]]=Multiply(ReadRows(baseline+i*12),bind[i]);
                for(int first:{1,4}) {
                    const float length=firstperson::Length(Sub(fitted[first+1].origin,fitted[first].origin))+
                        firstperson::Length(Sub(fitted[first+2].origin,fitted[first+1].origin));
                    Check(firstperson::Length(Sub(fitted[first+2].origin,fitted[first].origin))/length>.999f &&
                          firstperson::KneeBendDegrees(fitted,first)<2,
                          "actual rendered upright knees are within two degrees of straight");
                    Check(std::fabs(fitted[first+2].origin.y-(bodyJointWorld[first+2].y-camera.y))<.1f,
                          "rendered calibrated feet stay at the original floor height");
                }
                for(float yaw:{-.35f,-.15f,.15f,.35f}) for(float pitch:{-.2f,0.f,.2f}) {
                    Head(yaw,pitch);++VR().m_poseSerial;
                    Detour_GetJoints(itemMemory,palette,0);
                    for(int i=0;i<count;++i) {
                        const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                        const auto neutralJoint=Multiply(ReadRows(baseline+i*12),bind[i]);
                        if(mapping[i]==0 || mapping[i]==7) {
                            Check(nearVec(actual.origin,neutralJoint.origin),
                                  "real render hook holds chest and hips fixed during small head turns with stationary controllers");
                            for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                                Check(std::fabs(actual.basis.r[r][c]-neutralJoint.basis.r[r][c])<.001f,
                                      "real render hook never follows small head rotations with the torso");
                        }
                        if(mapping[i]==14) {
                            const Vec eye{float(g_scenePose.x_pos),float(g_scenePose.y_pos),float(g_scenePose.z_pos)};
                            Check(nearVec(Add(Transform(actual,{11,-32,144}),camera),eye),
                                  "real render hook keeps configured eye on camera while chest stays fixed");
                        }
                        if(mapping[i]==10 || mapping[i]==13) {
                            Frame target{};Basis controller{};BuildControllerWrist(mapping[i]==10 ? 1 : 0,target,controller);
                            const Vec grip=Scale(Vec{-testCalibration.rightMetres,testCalibration.gripForwardMetres,
                                                    -testCalibration.raiseMetres},LiveWorldUnitsPerMetre());
                            Check(nearVec(Add(Transform(actual,grip),camera),Transform(target,grip)),
                                  "automatic body scaling preserves physical controller grip alignment");
                        }
                    }
                }
                // A real neck need not follow our estimated pivot. Check final
                // leg frames during sustained down-gaze with imperfect height
                // compensation, translation and small orientation tremor.
                for(int frame=0;frame<180;++frame) {
                    const float wave=std::sin(frame*.37f);
                    Head(.03f*wave,-.7f+.025f*wave);
                    const auto& h=VR().m_rawHeadPose;
                    VR().m_rawHeadPose.m[1][3]+=.08f*h.m[1][1]-.12f*h.m[1][2]-VR().m_firstPersonNeutralEyeNeckY;
                    VR().m_rawHeadPose.m[1][3]-=.065f+.018f*wave;
                    VR().m_rawHeadPose.m[0][3]+=.008f*wave;
                    VR().m_rawHeadPose.m[2][3]+=.012f*wave;
                    ++VR().m_poseSerial;g_torsoPose.time=TurnTime()-1./90.;
                    const auto cameraBefore=g_scenePose;
                    Detour_GetJoints(itemMemory,palette,0);
                    Check(g_torsoPose.planted,"standing contact hold does not depend on hand-facing detection");
                    for(int i=0;i<count;++i) {
                        const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                        const auto baseJoint=Multiply(ReadRows(baseline+i*12),bind[i]);
                        if(mapping[i]<=6) {
                            Check(nearVec(actual.origin,baseJoint.origin),"head tremor cannot jerk uploaded thighs, knees or feet");
                            for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                                Check(std::fabs(actual.basis.r[r][c]-baseJoint.basis.r[r][c])<.001f,
                                      "stationary rendered legs keep their orientation under head tremor");
                        }
                        if(mapping[i]==14) {
                            locomotion::Vec view{};VR().FirstPersonViewOffset(view.x,view.z);
                            view=locomotion::Rotate(view,g_headingBase)*LiveWorldUnitsPerMetre();
                            const Vec eye{g_scenePose.x_pos+view.x,g_scenePose.y_pos+VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre(),g_scenePose.z_pos+view.z};
                            Check(nearVec(Add(Transform(actual,{11,-32,144}),camera),eye),"planted legs preserve the exact rendered eye");
                        }
                        if(mapping[i]==10 || mapping[i]==13) {
                            Frame target{};Basis controller{};BuildControllerWrist(mapping[i]==10 ? 1 : 0,target,controller);
                            const Vec grip=Scale(Vec{-testCalibration.rightMetres,testCalibration.gripForwardMetres,-testCalibration.raiseMetres},LiveWorldUnitsPerMetre());
                            Check(nearVec(Add(Transform(actual,grip),camera),Transform(target,grip)),"leg planting preserves physical controller grips");
                        }
                    }
                    Check(!std::memcmp(&cameraBefore,&g_scenePose,sizeof(g_scenePose)),"leg planting cannot write the camera pose");
                }
                for(float pitch:{-.35f,-.7f,-1.1f}) {
                    Head(0,pitch);
                    const auto& h=VR().m_rawHeadPose;
                    VR().m_rawHeadPose.m[1][3]+=.08f*h.m[1][1]-.12f*h.m[1][2]-VR().m_firstPersonNeutralEyeNeckY;
                    Check(VR().FirstPersonHeightDrop()>.03f && std::fabs(VR().FirstPersonPostureDrop())<.0001f,
                          "looking down around a stationary neck is not a physical crouch");
                    ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                    Frame looking[15]{};
                    for(int i=0;i<count;++i) looking[mapping[i]]=Multiply(ReadRows(palette+i*12),bind[i]);
                    Check(firstperson::KneeBendDegrees(looking,1)<2 && firstperson::KneeBendDegrees(looking,4)<2,
                          "looking down at the legs retains straight upright knees in the real renderer");
                    const Vec eye{float(g_scenePose.x_pos),float(g_scenePose.y_pos)+VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre(),float(g_scenePose.z_pos)};
                    Check(nearVec(Add(Transform(looking[14],{11,-32,144}),camera),eye),
                          "neck-arc posture compensation does not modify the rendered eye target");
                }
                // Real tracked room movement remains native idle. Do not set
                // a locomotion flag just to make the standing lock release.
                {
                    const auto oldGround=g_groundEye;
                    const auto beforeFollow=g_bodyFollower;const auto beforeFilter=g_torsoPose;
                    g_groundEye.valid=true;
                    g_groundEye.local={float(g_scenePose.x_pos)-root.x,float(g_scenePose.y_pos)-root.y,float(g_scenePose.z_pos)-root.z};
                    g_groundEye.bodyLocal={g_groundEye.local.x,g_groundEye.local.z};
                    for(Vec travel:{Vec{.35f,0,0},Vec{-.35f,0,0},Vec{0,0,.35f},Vec{0,0,-.35f},Vec{.10f,0,0}}) {
                        Head(0);g_bodyFollower.Reset();g_torsoPose.Reset();
                        ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                        Frame start[15]{};
                        for(int i=0;i<count;++i) start[mapping[i]]=Multiply(ReadRows(palette+i*12),bind[i]);
                        for(int frame=1;frame<=180;++frame) {
                            const float progress=std::min(1.f,frame/90.f);
                            Head(0);VR().m_rawHeadPose.m[0][3]+=travel.x*progress;
                            VR().m_rawHeadPose.m[2][3]-=travel.z*progress;
                            for(int hand=0;hand<2;++hand) {
                                auto& m=VR().m_rawControllerPose[hand];m=VR().m_rawHeadPose;
                                m.m[0][3]+=(hand ? .25f : -.25f);m.m[1][3]-=.3f;m.m[2][3]-=.4f;
                            }
                            ++VR().m_poseSerial;g_bodyFollower.time=g_torsoPose.time=TurnTime()-1./90.;
                            Detour_GetJoints(itemMemory,palette,0);
                            Frame walkingRoom[15]{};
                            for(int i=0;i<count;++i) walkingRoom[mapping[i]]=Multiply(ReadRows(palette+i*12),bind[i]);
                            Check(firstperson::KneeBendDegrees(walkingRoom,1)<8 && firstperson::KneeBendDegrees(walkingRoom,4)<8,
                                  "visual roomscale following cannot switch standing legs into synthetic crouching");
                        }
                        for(int i=0;i<count;++i) if(mapping[i]==0 || mapping[i]==7) {
                            const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                            const Vec wanted=Add(start[mapping[i]].origin,Scale(travel,LiveWorldUnitsPerMetre()));
                            const float error=firstperson::Length(firstperson::Flat(Sub(actual.origin,wanted)))/LiveWorldUnitsPerMetre();
                            if(error>.065f) std::printf("room follow error %.3f joint %d travel %.2f %.2f\n",error,mapping[i],travel.x,travel.z);
                            Check(error<.065f,"tracked steps in every direction keep pelvis and chest centered within neck/noise slack");
                        }
                        Frame settled[15]{};
                        for(int i=0;i<count;++i) settled[mapping[i]]=Multiply(ReadRows(palette+i*12),bind[i]);
                        for(int frame=0;frame<30;++frame) {
                            Head(0);VR().m_rawHeadPose.m[0][3]+=travel.x+(frame%2 ? .002f : -.002f);
                            VR().m_rawHeadPose.m[2][3]-=travel.z;
                            ++VR().m_poseSerial;g_bodyFollower.time=g_torsoPose.time=TurnTime()-1./90.;
                            Detour_GetJoints(itemMemory,palette,0);
                            for(int i=0;i<count;++i) if(mapping[i]<=6) {
                                const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                                Check(firstperson::Length(Sub(actual.origin,settled[mapping[i]].origin))<.001f*LiveWorldUnitsPerMetre(),
                                      "after a real step, subtle head noise leaves the new standing leg pose steady");
                            }
                        }
                    }
                    for(float turn:{1.570796327f,-1.570796327f,3.14159265f}) {
                        Head(0);g_bodyFollower.Reset();g_torsoPose.Reset();
                        ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                        Frame start[15]{};
                        for(int i=0;i<count;++i) start[mapping[i]]=Multiply(ReadRows(palette+i*12),bind[i]);
                        const Vec eyePalette=Sub({float(g_scenePose.x_pos),float(g_scenePose.y_pos),float(g_scenePose.z_pos)},camera);
                        for(int frame=1;frame<=270;++frame) {
                            const float angle=turn*std::min(1.f,frame/90.f);
                            Head(angle);
                            for(int hand=0;hand<2;++hand) {
                                auto& m=VR().m_rawControllerPose[hand];m=VR().m_rawHeadPose;
                                const Vec offset=firstperson::YawVector({hand ? .25f : -.25f,0,.4f},angle);
                                m.m[0][3]+=offset.x;m.m[1][3]-=.3f;m.m[2][3]-=offset.z;
                            }
                            const float nativeYaw=g_bodyFollower.yaw;
                            current.y_rot=previous.y_rot=Angle(nativeYaw);
                            bodyJointRotation=firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},nativeYaw));
                            const Vec pivot=Sub(root,camera);
                            const Frame rotate{bodyJointRotation,Sub(pivot,Transform(bodyJointRotation,pivot))};
                            for(int i=0;i<count;++i) WriteRows(Multiply(rotate,ReadRows(unturned+i*12)),fixture+i*12);
                            for(int i=0;i<15;++i) bodyJointWorld[i]=Add(root,Transform(bodyJointRotation,native[i].origin));
                            ++VR().m_poseSerial;g_bodyFollower.time=g_torsoPose.time=TurnTime()-1./90.;
                            Detour_GetJoints(itemMemory,palette,0);
                            Check(std::fabs(Wrap(g_bodyFollower.yaw-angle))<=BodyFollowSettings().yawSlack+.001f,
                                  "rendered body resolves physical turn beyond head slack in this tracking sample");
                        }
                        Check(std::fabs(Wrap(g_bodyFollower.yaw-turn))<=BodyFollowSettings().yawSlack+.001f,"real body rotation stays within neck yaw slack");
                        for(int i=0;i<count;++i) if(mapping[i]==0 || mapping[i]==7) {
                            const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                            const Vec wanted=Add(eyePalette,firstperson::YawVector(Sub(start[mapping[i]].origin,eyePalette),g_bodyFollower.yaw));
                            const float error=firstperson::Length(firstperson::Flat(Sub(actual.origin,wanted)))/LiveWorldUnitsPerMetre();
                            if(error>.065f) std::printf("turn centering error %.3f joint %d turn %.2f\n",error,mapping[i],turn);
                            Check(error<.065f,"physical quarter and half turns retain torso centering around tracked eye");
                        }
                        current.y_rot=previous.y_rot=0;bodyJointRotation=identity;
                        std::memcpy(fixture,unturned,sizeof(fixture));
                        for(int i=0;i<15;++i) bodyJointWorld[i]=Add(root,native[i].origin);
                    }
                    Head(0);g_groundEye=oldGround;g_bodyFollower=beforeFollow;g_torsoPose=beforeFilter;
                }
                {
                    const auto beforeGround=g_groundEye;const auto beforeWall=g_bodyWallOffset;
                    const auto beforeCamera=g_scenePose;const auto beforeBase=g_headingBase;
                    const auto beforeFollower=g_bodyFollower;const auto beforeTorso=g_torsoPose;
                    const bool beforeInput=g_haveManualInput;const auto beforeLocal=g_manualLocal;
                    g_groundEye.valid=true;
                    g_groundEye.bodyLocal={float(g_scenePose.x_pos)-root.x,float(g_scenePose.z_pos)-root.z};
                    for(float base:{0.f,-1.13f,.75f}) {
                        g_headingBase=base;Head(-base);
                        const auto local=locomotion::Rotate(g_groundEye.bodyLocal,-base);
                        g_groundEye.local={local.x,float(g_scenePose.y_pos)-root.y,local.z};
                        g_bodyFollower.Reset();g_torsoPose.Reset();g_bodyWallOffset={};
                        g_haveManualInput=false;state=2;
                        ++VR().m_poseSerial;Detour_GetJoints(itemMemory,baseline,0);
                        const Vec standingRoot=g_bodyFollower.root;
                        Check(firstperson::Length(firstperson::Flat(Sub(standingRoot,Scale(root,1/LiveWorldUnitsPerMetre()))))<.0001f,
                              "calibrated body offset uses body coordinates after tracking yaw recenter");
                        // The same calibrated eye expressed in another tracking
                        // yaw must not create a translation goal or a fake step.
                        for(int frame=0;frame<24;++frame) {
                            ++VR().m_poseSerial;g_bodyFollower.time=g_torsoPose.time=TurnTime()-1./90.;
                            Detour_GetJoints(itemMemory,palette,0);
                            Check(firstperson::Length(Sub(g_bodyFollower.root,standingRoot))<.0001f && !g_bodyFollower.stepping,
                                  "recentered tracking yaw cannot create a body-follow step");
                        }
                        g_haveManualInput=true;g_manualLocal={0,1};
                        for(int wallState:{2,12,2,12,2}) for(float retract:{-100.f,-101.f,-99.f,0.f}) {
                            state=int16_t(wallState);g_bodyWallOffset={0,retract};
                            g_scenePose=beforeCamera;g_scenePose.z_pos+=int(retract);
                            ++VR().m_poseSerial;g_bodyFollower.time=g_torsoPose.time=TurnTime()-1./90.;
                            Check(FullBodyIKReady(),"wall impact retains full-body rendering while input is held");
                            Detour_GetJoints(itemMemory,palette,0);
                            for(int i=0;i<count;++i) if(mapping[i]==0 || mapping[i]==7 || mapping[i]==10 || mapping[i]==13) {
                                const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                                const auto start=Multiply(ReadRows(baseline+i*12),bind[i]);
                                Check(nearVec(Sub(actual.origin,{0,0,retract}),start.origin),
                                      "wall contact/release carries torso and hands with the eye without a catch-up pop");
                            }
                            Check(firstperson::Length(Sub(g_bodyFollower.root,standingRoot))<.0001f,
                                  "camera wall retraction never feeds back into roomscale body motion");
                        }
                    }
                    g_groundEye=beforeGround;g_bodyWallOffset=beforeWall;g_scenePose=beforeCamera;g_headingBase=beforeBase;
                    g_bodyFollower=beforeFollower;g_torsoPose=beforeTorso;g_haveManualInput=beforeInput;g_manualLocal=beforeLocal;
                }
                Head(0);VR().m_rawHeadPose.m[1][3]-=.2f;state=1;
                ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                Check(!g_torsoPose.planted,"walking releases planted contacts in the actual render hook");
                Check(g_bodySize.factor==calibratedSize,"walking and physical crouch cannot rescale the rendered body");
                state=2;
                ++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
                Frame ducked[15]{};
                for(int i=0;i<count;++i) ducked[mapping[i]]=Multiply(ReadRows(palette+i*12),bind[i]);
                Check(firstperson::KneeBendDegrees(ducked,1)>20 && firstperson::KneeBendDegrees(ducked,4)>20,
                      "real physical crouch releases upright legs even while native animation remains standing idle");
                Head(0);g_scenePose=beforeScene;g_bodyVisualOffset=beforeFit;g_haveManualInput=beforeManual;
                g_stanceEye=beforeStance;config.firstPersonBodyScale=1;g_bodySize.Reset();
                g_bodyFollower.Reset();
            }
            for(int which:{0,1}) for(int armed:{0,1}) for(int step=0;step<48;++step) {
                g_torsoPose.Reset(); // Independent geometry samples, not a temporal sequence.
                ++VR().m_poseSerial;
                dll.game=which;state=2;status=armed ? 4 : 0;
                const float phase=step*6.2831853f/47;
                Head(phase,.35f*std::sin(phase));g_headingBase=-phase*.65f;
                // The native skeleton lags the tracked heading and interpolates
                // across +/-pi. Its palette and joint queries share that pose.
                current.y_rot=Angle(phase*.7f+.3f);previous.y_rot=Angle(phase*.7f-.6f);
                fraction=(step%5)*64;
                const float animatedYaw=Radians(previous.y_rot)+Wrap(Radians(current.y_rot)-Radians(previous.y_rot))*(fraction/256.f);
                bodyJointRotation=firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},animatedYaw));
                const Vec relativeRoot=Sub(root,camera);
                const Frame rotate{bodyJointRotation,Sub(relativeRoot,Transform(bodyJointRotation,relativeRoot))};
                for(int i=0;i<count;++i) WriteRows(Multiply(rotate,ReadRows(unturned+i*12)),fixture+i*12);
                for(int i=0;i<15;++i) bodyJointWorld[i]=Add(root,Transform(bodyJointRotation,native[i].origin));
                VR().m_rawHeadPose.m[0][3]+=.65f*std::sin(phase);
                VR().m_rawHeadPose.m[1][3]+=.3f*std::cos(phase);
                VR().m_rawHeadPose.m[2][3]+=.5f*std::sin(phase*2);
                g_bodyVisualOffset={80*std::cos(phase),-120*std::sin(phase)};
                for(int hand=0;hand<2;++hand) {
                    auto& m=VR().m_rawControllerPose[hand];m=VR().m_rawHeadPose;
                    m.m[0][3]+=(hand ? .25f : -.25f);m.m[1][3]-=.3f;m.m[2][3]-=.35f;
                }
                const auto itemSnapshot=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
                Detour_GetJoints(itemMemory,palette,0);
                locomotion::Vec displacement{};VR().FirstPersonViewOffset(displacement.x,displacement.z);
                displacement=locomotion::Rotate(displacement,g_headingBase)*LiveWorldUnitsPerMetre();
                const Vec eye{float(g_scenePose.x_pos)+displacement.x,
                    float(g_scenePose.y_pos)+VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre(),
                    float(g_scenePose.z_pos)+displacement.z};
                for(int i=0;i<count;++i) {
                    const auto joint=Multiply(ReadRows(palette+i*12),bind[i]);
                    if(mapping[i]==14) {
                        const Vec anchor=Transform(joint,{11,-32,144});
                        Check(firstperson::Length(Sub(Add(anchor,camera),eye))<2,
                              "full-body eye anchor stays within native integer-joint precision through roomscale and interpolated turns");
                    }
                    if(mapping[i]==0) {
                        const Vec forward=Transform(joint.basis,{0,0,1});
                        Check(std::fabs(Wrap(std::atan2(forward.x,forward.z)-g_bodyFollower.yaw))<.003f,
                              "pelvis retains persistent body yaw while torso and neck absorb the head twist");
                    }
                    if(mapping[i]==10 || mapping[i]==13) {
                        Frame target{};Basis controller{};Check(BuildControllerWrist(mapping[i]==10 ? 1 : 0,target,controller),"centered body wrist target available");
                        const Vec grip=Scale(Vec{-testCalibration.rightMetres,testCalibration.gripForwardMetres,
                                                -testCalibration.raiseMetres},LiveWorldUnitsPerMetre());
                        Check(firstperson::Length(Sub(Add(Transform(joint,grip),camera),Transform(target,grip)))<2,
                              "centering body keeps physical controller grip within native integer-joint precision");
                    }
                }
                Check(itemSnapshot==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),"centering changes only render skeleton");
                float repeated[33*12]{};Detour_GetJoints(itemMemory,repeated,0);
                for(int i=0;i<count*12;++i) Check(std::fabs(repeated[i]-palette[i])<.002f,
                    "repeated eye draws do not accumulate full-body offsets or rotations");
            }
            fraction=oldFraction;std::memcpy(fixture,unturned,sizeof(fixture));bodyJointRotation=identity;
            VR().m_firstPersonNeutralEyeNeckY=oldNeckHeight;
            for(int i=0;i<15;++i) bodyJointWorld[i]=Add(root,native[i].origin);
            config=savedSettings;VR().m_rawHeadPose=oldHead;VR().m_headFromTracking=oldView;g_scenePose=oldScene;
            g_headingBase=oldBase;g_bodyVisualOffset=oldFit;current=oldPos;previous=oldPrev;
            std::memcpy(VR().m_firstPersonNeutral,neutral,sizeof(neutral));std::memcpy(VR().m_firstPersonNeutralNeck,neck,sizeof(neck));
            std::memcpy(VR().m_rawControllerPose,oldHands,sizeof(oldHands));status=0;
        }
        // Native STOP arrives before the last sprint/directional-jump palette.
        // Exercise the actual uploaded skin, including the same tracking sample.
        for(int transition:{73,25,26,27}) {
            float originalFixture[33*12]{};std::memcpy(originalFixture,fixture,sizeof(fixture));
            Vec originalWorld[15];std::copy(std::begin(bodyJointWorld),std::end(bodyJointWorld),originalWorld);
            g_torsoPose.Reset();g_bodyFollower.Reset();state=2;status=0;++VR().m_poseSerial;
            Detour_GetJoints(itemMemory,baseline,0);
            const auto bend=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},.8f));
            for(int i=0;i<count;++i) if(mapping[i]>=1 && mapping[i]<=6) {
                const int hip=mapping[i]<=3 ? 1 : 4;
                const Vec pivot=Sub(bodyJointWorld[hip],camera);
                const Frame move{bend,Sub(pivot,Transform(bend,pivot))};
                WriteRows(Multiply(move,ReadRows(fixture+i*12)),fixture+i*12);
            }
            state=int16_t(transition);++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
            state=2;++VR().m_poseSerial;Detour_GetJoints(itemMemory,palette,0);
            std::memcpy(fixture,originalFixture,sizeof(fixture));
            for(int frame=0;frame<180;++frame) {
                if(frame) ++VR().m_poseSerial;
                g_torsoPose.time=TurnTime()-1./90.;Detour_GetJoints(itemMemory,palette,0);
            }
            for(int i=0;i<count;++i) if(mapping[i]==3 || mapping[i]==6) {
                const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                const auto standing=Multiply(ReadRows(baseline+i*12),bind[i]);
                Check(std::fabs(actual.origin.y-standing.origin.y)<1.f,
                      "uploaded feet return to standing floor after sprint and left/right/back jump recovery");
            }
            std::copy(std::begin(originalWorld),std::end(originalWorld),bodyJointWorld);
        }
        // Mount completion changes the incoming chest pose abruptly. Check
        // the final uploaded palette, where body IK used to undo the handoff.
        {
            g_torsoPose.Reset();g_bodyFollower.Reset();
            state=19;status=1;++VR().m_poseSerial;
            Check(FullBodyIKReady(),"native hands-busy mount keeps the full-body solver active");
            Detour_GetJoints(itemMemory,baseline,0);g_torsoPose.Mount();
            Frame previousPose[15]{};
            for(int i=0;i<count;++i) previousPose[mapping[i]]=Multiply(ReadRows(baseline+i*12),bind[i]);
            float originalFixture[33*12]{};std::memcpy(originalFixture,fixture,sizeof(fixture));
            const Vec originalChest=bodyJointWorld[7];
            const auto oldScene=g_scenePose;
            auto& current=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
            auto& previous=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
            const auto oldCurrent=current,oldPrevious=previous;
            const Vec relocation{120,-512,200};
            current.x_pos+=120;previous.x_pos+=120;g_scenePose.x_pos+=120;
            current.y_pos-=512;previous.y_pos-=512;g_scenePose.y_pos-=512;
            current.z_pos+=200;previous.z_pos+=200;g_scenePose.z_pos+=200;
            for(int j=0;j<15;++j) { bodyJointWorld[j]=Add(bodyJointWorld[j],relocation);previousPose[j].origin=Add(previousPose[j].origin,relocation); }
            for(int i=0;i<count;++i) {
                auto frame=ReadRows(fixture+i*12);frame.origin=Add(frame.origin,relocation);WriteRows(frame,fixture+i*12);
            }
            for(int i=0;i<count;++i) if(mapping[i]==7) {
                auto frame=ReadRows(fixture+i*12);frame.origin.z+=180;WriteRows(frame,fixture+i*12);
            }
            bodyJointWorld[7].z+=180;state=2;status=0;
            for(int frame=0;frame<24;++frame) {
                if(frame) ++VR().m_poseSerial; // First standing draw shares the mount's HMD sample.
                g_torsoPose.time=TurnTime()-1./90.;
                Detour_GetJoints(itemMemory,palette,0);
                for(int i=0;i<count;++i) if(mapping[i]==0 || mapping[i]==7) {
                    const auto actual=Multiply(ReadRows(palette+i*12),bind[i]);
                    Check(firstperson::Length(Sub(actual.origin,previousPose[mapping[i]].origin))<=
                          LiveWorldUnitsPerMetre()*1.2f*std::min(g_torsoPose.cadence,.05f)+.02f,
                          "uploaded mount-to-standing pelvis and chest cannot pop");
                    previousPose[mapping[i]]=actual;
                }
            }
            for(int j=0;j<15;++j) bodyJointWorld[j]=Sub(bodyJointWorld[j],relocation);
            std::memcpy(fixture,originalFixture,sizeof(fixture));bodyJointWorld[7]=originalChest;
            current=oldCurrent;previous=oldPrevious;g_scenePose=oldScene;
            g_torsoPose.Reset();
        }
        // Exercise the real HMD Y-up -> game Y-down path, not only a synthetic
        // solver delta. A physical crouch must lower the pelvis, keeping feet.
        const auto sceneBefore=g_scenePose;const float headY=VR().m_rawHeadPose.m[1][3];
        const bool flipBefore=config.flipViewY;config.flipViewY=true;
        g_bodyFollower.Reset();g_torsoPose.Reset();++VR().m_poseSerial;
        g_scenePose.y_pos=int(bodyJointWorld[14].y);
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1];
        Detour_GetJoints(itemMemory,baseline,0);
        VR().m_rawHeadPose.m[1][3]-=.20f;
        for(int frame=0;frame<30;++frame) {
            ++VR().m_poseSerial;g_torsoPose.time=TurnTime()-1./90.;
            Detour_GetJoints(itemMemory,palette,0);
        }
        for(int i=0;i<count;++i) {
            const auto before=Multiply(ReadRows(baseline+i*12),bind[i]);
            const auto after=Multiply(ReadRows(palette+i*12),bind[i]);
            if(mapping[i]==0) Check(after.origin.y>before.origin.y+20,"physical headset crouch lowers the rendered pelvis");
            if(mapping[i]==3 || mapping[i]==6) Check(nearVec(before.origin,after.origin),"physical headset crouch cannot move feet");
        }
        g_scenePose=sceneBefore;VR().m_rawHeadPose.m[1][3]=headY;config.flipViewY=flipBefore;
        Detour_GetJoints(itemMemory,palette,0);TestDynamicBonesVisibility(palette);
        for(int reason=0;reason<10;++reason) {
            state=2;status=0;gun=1;g_active=true;appMemory[0x9e4]=1;config.firstPersonFullBodyIK=true;VR().m_controllerPoseValid[0]=true;
            if(reason==0) config.firstPersonFullBodyIK=false;
            if(reason==1) VR().m_controllerPoseValid[0]=false;
            if(reason==2) g_active=false;
            if(reason==3) appMemory[0x9e4]=0;
            if(reason==4) state=23;
            if(reason==5) state=12;
            if(reason==6) status=1; // Busy hands outside native ledge grips.
            if(reason==7) water=1;
            if(reason==8) gun=7;
            if(reason==9) state=40; // Scripted/use interaction not in the allowed set.
            Check(!FullBodyIKReady(),"full-body gate rejects unsupported/native interactions and tracking loss");
            std::memcpy(palette,fixture,count*12*sizeof(float));
            Check(!ApplyFullBodyIK(itemMemory,palette,count) && !std::memcmp(palette,fixture,count*12*sizeof(float)),"rejected solve is atomic and leaves no stale body pose");
            water=0;
        }
        state=2;status=0;gun=1;g_active=true;appMemory[0x9e4]=1;config.firstPersonFullBodyIK=true;VR().m_controllerPoseValid[0]=true;
        ShadowDll shadow{};static int pass=4;shadow.renderPass=rva(&pass);g_shadowDll=&shadow;
        Detour_GetJoints(itemMemory,palette,0);
        for(int i=0;i<count;++i) if(mapping[i]<8 || mapping[i]>13)
            Check(!std::memcmp(palette+i*12,fixture+i*12,12*sizeof(float)),
                  "full-body fitting never moves the shadow root torso head or legs; only free arms track controllers");
        g_shadowDll=nullptr;
        Detour_GetJoints(laraMemory,palette,0);
        Check(!std::memcmp(palette,fixture,count*12*sizeof(float)),"full-body cannot pose another creature");
    }
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveFullBodyDraw);
    status=4;state=2;bits=0x3fff;fullBodyDraws=fullBodyGunDraws=0;
    Detour_DrawCreatureHD(itemMemory,0,0);bits=0x3600;Detour_DrawCreatureHD(itemMemory,1,0);
    Check(fullBodyDraws==1 && fullBodyGunDraws==2 && bits==0x3600 && g_renderArm==-1,
          "armed renderer adds one body draw, preserves both gun draws and restores native masks");
    for (bool full:{false,true}) for (int stance:{1,2,3,45,71,80,86}) {
        config.firstPersonFullBodyIK=full;config.firstPersonUnarmedIK=false;
        state=int16_t(stance);status=4;fullBodyDraws=fullBodyGunDraws=0;
        bits=0x3fff;Detour_DrawCreatureHD(itemMemory,0,0);
        bits=0x3600;Detour_DrawCreatureHD(itemMemory,1,0);
        Check(fullBodyDraws==(IsCrouchState(itemMemory) ? 0 : 1) && fullBodyGunDraws==2 && bits==0x3600 && g_renderArm==-1,
              "crouched tracked guns show hands only; other ordinary stances retain the body");
    }
    // Exercise real draw and palette hooks with both IK settings and weapon
    // paths; native simulation may change state without a new tracking sample.
    float visibilityFixture[33*12]{},visibilityBinds[33*12]{};int32_t visibilityMapping[33]{};
    for(int i=0;i<33;++i) {
        visibilityMapping[i]=(i*7)%15;
        WriteRows({identity,positions[visibilityMapping[i]]},visibilityFixture+i*12);
        Frame rawBind{identity,{}};
        for(int r=0;r<3;++r) { rawBind.basis.r[r][1]=identity.r[r][2];rawBind.basis.r[r][2]=-identity.r[r][1]; }
        WriteRows(rawBind,visibilityBinds+i*12);
    }
    auto* visibilityGeom=objectInfo+off::object_geom;
    *reinterpret_cast<int32_t*>(visibilityGeom+28)=33;
    *reinterpret_cast<int32_t**>(visibilityGeom+48)=visibilityMapping;
    *reinterpret_cast<float**>(visibilityGeom+72)=visibilityBinds;
    skinFixture=visibilityFixture;skinFixtureCount=33;restrictedMapping=visibilityMapping;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveRestrictedBodyDraw);
    auto& visibilityAnim=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
    for(bool full:{false,true}) for(bool armed:{false,true}) for(int stance:{10,30,31,75,82,83,139,19,54,2,71,80,81,84,85,86,105,106}) {
        g_bodyVisibility={};g_mountBodyTransition.Reset();g_torsoPose.Reset();
        config.firstPersonFullBodyIK=full;config.firstPersonUnarmedIK=true;
        state=int16_t(stance);status=armed ? 4 : (stance==2 ? 0 : 1);gun=1;
        visibilityAnim=stance==2 ? 24 : 103;
        const bool crouched=IsCrouchState(itemMemory);
        if(crouched && !armed) status=0;
        restrictedBodyMask=crouched ? firstperson::HandMeshBits : stance==2 ? ~(1u<<7) : firstperson::ArmMeshBits;
        restrictedBodyDraws=0;bits=UINT32_MAX;
        Detour_DrawCreatureHD(itemMemory,0,0);
        Check(restrictedBodyDraws==(crouched && armed ? 0 : 1) && bits==UINT32_MAX,
              "transition body draw keeps restriction and restores native item mask");
        if(armed) { bits=0x3600;Detour_DrawCreatureHD(itemMemory,1,0); }
        if(armed && stance!=2) {
            const int before=restrictedBodyDraws;bits=0x92;Detour_DrawCreatureHD(itemMemory,1,0);
            Check(restrictedBodyDraws==before,"arms-only ledge view suppresses floating hip and back holster meshes");
        }
        state=2;visibilityAnim=103;status=armed ? 4 : 0;bits=UINT32_MAX;
        if(crouched) restrictedBodyMask=UINT32_MAX;
        Detour_DrawCreatureHD(itemMemory,0,0);
        Check(g_bodyVisibility.mask==restrictedBodyMask,
              "first standing draw stays hidden with unchanged VR pose serial");
        g_bodyVisibility.until=0;g_torsoPose.Reset();g_mountBodyTransition.Reset();restrictedBodyMask=UINT32_MAX;
        Detour_DrawCreatureHD(itemMemory,0,0);
        Check(g_bodyVisibility.mask==UINT32_MAX,"settled standing restores normal visibility after recovery");
    }
    skinFixture=nullptr;skinFixtureCount=0;g_bodySkinScope=g_bodySkinReady=false;
    g_hGetJoints.m_trampoline=getJoints;g_hDrawCreatureHD.m_trampoline=draw;g_shadowDll=shadowDll;
    g_scenePose=savedPose;g_headingBase=savedHeading;g_bodyVisualOffset=savedOffset;
    VR().m_rawHeadPose=savedHead;std::memcpy(VR().m_rawControllerPose,controllers,sizeof(controllers));
    std::memcpy(itemMemory,savedItem.data(),sizeof(itemMemory));std::memcpy(laraMemory,savedLara.data(),sizeof(laraMemory));
    config=settings;dll=savedDll;testCalibration=calibration;g_unarmedTwist[0]=savedLeft;g_unarmedTwist[1]=savedRight;
    g_bodyFollower=savedFollower;VR().m_poseSerial=savedSerial;
    g_pistolRoll=savedPistolRoll;
    g_torsoPose=savedTorso;g_mountBodyTransition=savedMount;
    g_bodyLanding=savedLanding;
    g_pistolAnimation=savedPistolAnimation;
    g_bodyVisibility=savedVisibility;
    g_autoBodyCalibration=savedAuto;
    g_bodySize=savedBodySize;g_bodySizeObject=savedSizeObject;
    VR().m_firstPersonNeutralEyeNeckY=savedNeckHeight;
    g_heightCrouch=savedHeightCrouch;g_physicalCrouchCamera=savedPhysicalCamera;
}
}
