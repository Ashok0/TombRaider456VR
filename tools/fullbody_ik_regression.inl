// Real render-hook fixtures plus independent physical constraints for full-body IK.
namespace {
tr::motiongun::Vec bodyJointWorld[15]{};
void __cdecl FakeBodyJoint(uint8_t* item,tr::PHD_VECTOR* point,int joint,int) {
    Check(item==itemMemory && joint>=0 && joint<15,"body IK reads only native Lara joints");
    const auto p=bodyJointWorld[joint];
    point->x+=int(p.x);point->y+=int(p.y);point->z+=int(p.z);
}
int fullBodyDraws=0,fullBodyGunDraws=0;
void __cdecl ObserveFullBodyDraw(uint8_t* item,int32_t masked,int32_t) {
    using namespace tr;
    Check(masked==1,"full-body eye passes use an explicit visibility mask");
    const auto bits=*reinterpret_cast<uint32_t*>(item+off::item_mesh_bits);
    if(g_renderArm<0) {
        ++fullBodyDraws;
        Check((bits&0xff)==0xff && (bits&0x1b00)==0x1b00 && !(bits&0x6400),
              "armed body includes torso legs and arm chains, excludes head and duplicate gun hands");
        Check(g_bodySkinScope,"armed body captures its own complete physics skeleton");
    } else { ++fullBodyGunDraws;Check(bits==(g_renderArm ? 0x400u : 0x2000u),"gun draws retain independent tracked hand masks"); }
}
void TestFullBodyIK() {
    using namespace tr;using namespace tr::motiongun;
    const auto settings=config;const auto savedDll=dll;const auto calibration=testCalibration;
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
    config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=config.firstPersonInteractionAnchorZ=0;
    config.firstPersonJoint=14;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);auto& gun=*reinterpret_cast<int16_t*>(laraMemory+4);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    *reinterpret_cast<int16_t*>(itemMemory+off::item_object)=0;
    state=2;status=0;gun=1;bits=0x3fff;
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    g_renderArm=-1;g_bodySkinScope=true;g_shadowDll=nullptr;
    const auto identity=firstperson::IdentityBasis();
    Frame native[15]{};
    const Vec positions[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    for(int i=0;i<15;++i) native[i]={identity,positions[i]};
    auto nearVec=[](Vec a,Vec b) { return firstperson::Length(Sub(a,b))<.04f; };
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
    Frame crouched[15]{};
    Check(firstperson::SolveBody(native,{120,220,160},identity,{0,0,1},1000,crouched) &&
          crouched[0].origin.y>native[0].origin.y+100 && crouched[7].origin.z>native[7].origin.z,
          "headset crouch lowers pelvis and leaning moves the chest");
    Frame broken[15];std::memcpy(broken,native,sizeof(broken));broken[2]=broken[1];
    Check(!firstperson::SolveBody(broken,{},identity,{0,0,1},1000,crouched),"degenerate leg safely rejects full-body solve");
    Check(!firstperson::SolveBody(native,{NAN,0,0},identity,{0,0,1},1000,crouched),"invalid head cannot poison skeleton");
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
        for(int which:{0,1}) for(int stance:{0,1,2,3,9,15,25,26,27,71,73,10,30,31,19,54}) for(int armed:{0,1}) {
            dll.game=which;state=int16_t(stance);status=armed ? 4 : 0;g_bodyVisualOffset={55,-80};
            g_headingBase=.4f;testCalibration={.04f,-.06985f,.2032f,-30.f,8,-6};
            Check(FullBodyIKReady(),"full-body available in both games, grounded, airborne and native grip states");
            if(!armed && !FullBodyGripState(itemMemory)) Check(UnarmedIKReady(),"free jumps retain controller hands");
            config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=false;
            Detour_GetJoints(itemMemory,baseline,0);
            config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=true;
            // Call explicitly as well: fallback arms must not conceal a failed body solve.
            std::memcpy(palette,baseline,count*12*sizeof(float));
            Check(ApplyFullBodyIK(itemMemory,palette,count),"production body solver recovers all native and HD helper joints");
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
                if(joint==3 || joint==6) Check(nearVec(actual.origin,Sub(bodyJointWorld[joint],camera)),"real palette keeps animated feet fixed despite head/body-fit offsets");
                if(joint==10 || joint==13) {
                    Frame target{};Basis controller{};const int hand=joint==10 ? 1 : 0;
                    BuildControllerWrist(hand,target,controller);
                    const auto expected=Sub(FullBodyGripState(itemMemory) ? bodyJointWorld[joint] : target.origin,camera);
                    Check(nearVec(actual.origin,expected),"full-body hands match gun positions or preserve native ledge grips");
                    if(armed && !FullBodyGripState(itemMemory)) for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                        Check(std::fabs(actual.basis.r[r][c]-target.basis.r[r][c])<.002f,"armed body wrist matches exact gun orientation without unarmed wrist limits");
                }
            }
        }
        state=2;status=0;
        // Exercise the real HMD Y-up -> game Y-down path, not only a synthetic
        // solver delta. A physical crouch must lower the pelvis, keeping feet.
        const auto sceneBefore=g_scenePose;const float headY=VR().m_rawHeadPose.m[1][3];
        const bool flipBefore=config.flipViewY;config.flipViewY=true;
        g_scenePose.y_pos=int(bodyJointWorld[14].y);
        VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1];
        Detour_GetJoints(itemMemory,baseline,0);
        VR().m_rawHeadPose.m[1][3]-=.20f;
        Detour_GetJoints(itemMemory,palette,0);
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
            if(reason==6) status=2;
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
        Check(!std::memcmp(palette,fixture,count*12*sizeof(float)),"full-body never changes native shadow caster skeleton");
        g_shadowDll=nullptr;
        Detour_GetJoints(laraMemory,palette,0);
        Check(!std::memcmp(palette,fixture,count*12*sizeof(float)),"full-body cannot pose another creature");
    }
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveFullBodyDraw);
    status=4;state=2;bits=0x3fff;fullBodyDraws=fullBodyGunDraws=0;
    Detour_DrawCreatureHD(itemMemory,0,0);bits=0x3600;Detour_DrawCreatureHD(itemMemory,1,0);
    Check(fullBodyDraws==1 && fullBodyGunDraws==2 && bits==0x3600 && g_renderArm==-1,
          "armed renderer adds one body draw, preserves both gun draws and restores native masks");
    skinFixture=nullptr;skinFixtureCount=0;g_bodySkinScope=g_bodySkinReady=false;
    g_hGetJoints.m_trampoline=getJoints;g_hDrawCreatureHD.m_trampoline=draw;g_shadowDll=shadowDll;
    g_scenePose=savedPose;g_headingBase=savedHeading;g_bodyVisualOffset=savedOffset;
    VR().m_rawHeadPose=savedHead;std::memcpy(VR().m_rawControllerPose,controllers,sizeof(controllers));
    std::memcpy(itemMemory,savedItem.data(),sizeof(itemMemory));std::memcpy(laraMemory,savedLara.data(),sizeof(laraMemory));
    config=settings;dll=savedDll;testCalibration=calibration;g_unarmedTwist[0]=savedLeft;g_unarmedTwist[1]=savedRight;
}
}
