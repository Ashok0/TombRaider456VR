// Focused fixture; never runs the full first-person suite or a game DLL.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
void TestDynamicBonesMirrorArguments();
void TestDynamicBonesMirrorIsolation(const float* palette);
namespace {
#include "../build/mirror/face-fixture.h"
alignas(16) uint8_t mirrorObjects[3792]{},mirrorRooms[179*304]{},mirrorChunk[44]{};
int mirrorOuterCalls=0,mirrorNativeDraws=0;
bool mirrorTestFirstPerson=true;
uint32_t mirrorObservedMask=0;
float bodyBefore[15*12]{};
const uint8_t nativeSelections[5]={2,2,1,1,0};
void __cdecl MirrorNativeDraw(uint8_t* item,int32_t,int32_t pass) {
    ++mirrorNativeDraws;mirrorObservedMask=*reinterpret_cast<uint32_t*>(item+12);
    float palette[33*12]{};
    const int n=tr::Detour_GetJoints(item,palette,pass);
    Check(n==15,"reflection retains native skeleton size");
    Check(!std::memcmp(palette,bodyBefore,sizeof(bodyBefore)),"every reflected body, head and arm matrix retains native animation in both views");
    uint64_t mask=0;int count=0;
    const float* rendered=tr::FirstPersonRenderBodyPalette(mask,count);
    Check(rendered && count==15 && !std::memcmp(rendered,bodyBefore,sizeof(bodyBefore)),"mirror shader receives native animation without IK or room-scale offsets");
}
void __cdecl MirrorOuterDraw(uint8_t* item,int32_t) {
    ++mirrorOuterCalls;
    const bool modern=(*reinterpret_cast<uint32_t*>(appMemory+tr::drva::app_off::cfgFlags)&2)!=0;
    Check(modern==(!mirrorTestFirstPerson && !tr::MirrorReflectionCapturing()),"third-person normal draw keeps native proximity behavior; reflected draw bypasses it");
    Check(tr::g_holsterRenderScope==(mirrorTestFirstPerson && !tr::MirrorReflectionCapturing()),"physical holster overrides are restricted to the ordinary first-person draw");
    if(tr::MirrorReflectionCapturing()) {
        Check(*reinterpret_cast<uint32_t*>(item+12)==(mirrorTestFirstPerson ? 0x7fffu : 0x36cfu),"reflection restores FP mesh visibility but retains third-person native mask");
        Check(!std::memcmp(appMemory+0x508,nativeSelections,5),"reflected hands and weapons use restored native selections");
        tr::Detour_DrawCreatureHD(item,0,0);tr::Detour_DrawHair(0);
        Check(!std::memcmp(seenHairViewRel,nativeShadowViewRel,sizeof(seenHairViewRel)),"reflected hair retains native placement");
    } else if(mirrorTestFirstPerson) {
        // Stand in for temporary hand selections during the scoped FP draw.
        std::memset(appMemory+0x508,7,5);
    }
}
float faceBefore[33*12]{};
void __cdecl MirrorFaceDraw(uint8_t* item,int32_t,int32_t pass) {
    using namespace tr;
    float palette[33*12]{};
    const int n=Detour_GetJoints(item,palette,pass);
    Check(n==33 && g_mirrorHeadScope,"actual head draw recognized with its authored 33-bone palette");
    uint64_t mask=0;int count=0;
    const float* rendered=FirstPersonRenderBodyPalette(mask,count);
    Check(rendered && count==33 && mask==0x1ffffffffull,"face keeps all slots, including animated joints beyond native mesh bits");
    Check(!std::memcmp(faceBefore,palette,sizeof(faceBefore)),"native head, face and neck matrices remain untouched by VR IK");
    Check(rendered && !std::memcmp(faceBefore,rendered,sizeof(faceBefore)),"shader receives intact native facial animation");
    for(const auto& v:AuthoredFaceSkin) {
        unsigned visible=0;
        for(int j=0;j<3;++j) if(mask&(uint64_t(1)<<v[j])) visible+=v[j+3];
        Check(visible==255,"every authored facial vertex retains all its skin weights");
    }
}
int nativeMirrorCalls=0,nativeMirrorHairFlag=-1;
bool nativeMirrorExpected=false;
void __cdecl NativeMirrorHair(int32_t flag) {
    nativeMirrorHairFlag=flag;FakeHair(flag);
}
void __cdecl NativeMirrorOuter(uint8_t* item,int32_t drawType) {
    using namespace tr;
    Check(drawType==1,"TR4 mirror flag reaches the native outer HD draw");
    Check(FirstPersonNativeMirrorDrawing() && !MirrorReflectionCapturing(),
          "native TR4 mirror uses its own renderer without the TR5 capture");
    Check(!(*reinterpret_cast<uint32_t*>(appMemory+drva::app_off::cfgFlags)&2),
          "reflected body bypasses real-camera proximity hiding");
    Check(!g_holsterRenderScope && !std::memcmp(appMemory+0x508,nativeSelections,5),
          "native mirror keeps the original hand and weapon selections");
    Detour_DrawCreatureHD(item,0,2);
    Detour_DrawHair(drawType);
}
void __cdecl NativeMirrorRoutine() {
    using namespace tr;
    ++nativeMirrorCalls;
    Check(FirstPersonNativeMirrorDrawing()==nativeMirrorExpected,"native mirror scope obeys gameplay gates");
    if(!nativeMirrorExpected) return;
    Check(!BodyVisualOffsetApplies(itemMemory),"native reflected body excludes Classic room-scale fitting");
    Check(*reinterpret_cast<uint32_t*>(itemMemory+12)==g_meshBaseBits,
          "native reflection restores the intact head/body before Classic or HD rendering");
    const int prepareBefore=testMirrorPass;testMirrorPass=-17;
    if(*reinterpret_cast<uint32_t*>(appMemory+drva::app_off::cfgFlags)&1)
        Detour_CrouchDraw(itemMemory,1);
    else Detour_DrawHair(1);
    Check(testMirrorPass==-17,"native mirror never schedules an extra TR5 glass capture");
    testMirrorPass=prepareBefore;
    TestDynamicBonesMirrorIsolation(bodyBefore);
}
}
int main() {
    using namespace tr;
    using namespace motiongun;
    for(int yaw=-180;yaw<=180;yaw+=5) {
        const auto rotation=RotY(float(yaw));float r[3][3]{},pos[3]={34256,-24744,77308};
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) r[i][j]=rotation.r[i][j];
        const auto world=mirror::WorldView(r,pos),delta=mirror::CameraReflection(world);
        const auto reflected=Mul(delta,world),direct=Mul(world,mirror::Reflection());
        for(int i=0;i<3;++i) for(int j=0;j<4;++j)
            Check(std::fabs(reflected.r[i][j]-direct.r[i][j])<.025f,"camera reflection equals reflecting world geometry at every heading");
        const auto twice=Mul(delta,delta);
        for(int i=0;i<3;++i) for(int j=0;j<4;++j)
            Check(std::fabs(twice.r[i][j]-(i==j ? 1.f : 0.f))<.04f,"reflection is an involution");
    }
    // Read-only live sample, retail TR5 0x68C12FE9, RICH2 room 134:
    // number_rooms=179 although the HD TRG has 180 rendering records.
    Check(mirror::Profile(1,12,134,179,32768,72704,1,3989463,36),"live native room count accepts the audited mirror geometry");
    for(int field=0;field<9;++field) {
        int p[]={1,12,134,179,32768,72704,1,3989463,36};++p[field];
        Check(!mirror::Profile(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8]),"each unsupported profile component fails closed");
    }
    for(const auto& profile:mirror::Profiles) {
        Check(mirror::FindProfile(1,12,profile.room,179,profile.originX,profile.originZ,1,profile.first,profile.count)==&profile,
              "each verified restroom selects its own audited mirror plane");
        const auto reflection=mirror::Reflection(profile);
        Check(Near(reflection.r[0][3]-profile.planeX,profile.planeX),"both restroom glass planes remain fixed under reflection");
        Check(!mirror::FindProfile(1,12,profile.room,179,profile.originX,profile.originZ,1,profile.first,profile.count+3),
              "modified geometry cannot select a mirror profile");
    }
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    auto rva=[](const void* p) {return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.game=game=1;g_boundDll=&dll;dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);
    dll.objects=rva(mirrorObjects);dll.room=rva(&loadedRooms);dll.numberRooms=rva(&loadedRoomCount);dll.camera=rva(cameraMemory);
    loadedRooms=mirrorRooms;loadedRoomCount=179;auto* room=mirrorRooms+134*304;
    *reinterpret_cast<int32_t*>(room+40)=32768;*reinterpret_cast<int32_t*>(room+48)=72704;
    *reinterpret_cast<int32_t*>(room+256)=1;*reinterpret_cast<uint8_t**>(room+264)=mirrorChunk;
    *reinterpret_cast<uint32_t*>(mirrorChunk+36)=3989463;*reinterpret_cast<uint32_t*>(mirrorChunk+40)=36;
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;
    ShadowDll shadow{};shadow.renderPass=rva(&nativeShadowPass);g_shadowDll=&shadow;nativeShadowPass=0;
    g_active=g_scenePoseValid=g_runtimeEnabled=true;VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;
    config=Config{};config.enabled=config.firstPerson=true;config.firstPersonUnarmedIK=config.firstPersonFullBodyIK=config.firstPersonMotionGuns=true;
    *reinterpret_cast<int32_t*>(appMemory+1264)=12;auto& flags=*reinterpret_cast<uint32_t*>(appMemory+drva::app_off::cfgFlags);flags=3;
    *reinterpret_cast<int16_t*>(itemMemory+28)=134;*reinterpret_cast<int16_t*>(itemMemory+38)=1000;*reinterpret_cast<int16_t*>(itemMemory+18)=2;
    g_motionHooksReady=g_haveHeading=true;g_headingItem=itemMemory;
    Head(.3f);VR().RecenterFirstPersonHead();
    for(int hand=0;hand<2;++hand) {
        VR().m_controllerPoseValid[hand]=true;
        VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
        VR().m_rawControllerPose[hand].m[0][3]+=hand ? .25f : -.25f;
        VR().m_rawControllerPose[hand].m[1][3]-=.4f;
    }
    g_bodyVisualOffset={123,-78};g_bodyWallOffset={19,35};
    Check(UnarmedIKReady() && FullBodyIKReady() && BodyVisualOffsetApplies(itemMemory),"fixture activates actual first-person IK and body offsets before testing their exclusion");
    std::memcpy(appMemory+0x508,nativeSelections,5);
    nativeShadowViewRel[0]=12;nativeShadowViewRel[1]=-34;nativeShadowViewRel[2]=56;
    for(int j=0;j<15;++j) WriteRows(Frame{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},float(j)*.13f)),{float(j*13),float(j*17),float(j*23)}},bodyBefore+j*12);
    skinFixture=bodyBefore;skinFixtureCount=15;
    g_hGetJoints.m_trampoline=reinterpret_cast<void*>(&FakeSkinJoints);
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&MirrorNativeDraw);g_hDrawHair.m_trampoline=reinterpret_cast<void*>(&FakeHair);
    g_hCrouchDraw.m_trampoline=reinterpret_cast<void*>(&MirrorOuterDraw);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+12);bits=0x3fff;
    g_meshOverride=true;g_meshItem=itemMemory;g_meshBaseBits=0x7fff;g_headHidden=true;g_renderArm=-1;
    testMirrorReady=true;
    Check(MirrorRoomEligible(itemMemory),"actual first-person room gate accepts verified scene");
    loadedRoomCount=180;
    Check(!MirrorRoomEligible(itemMemory),"HD rendering record count is not substituted for native room count");
    loadedRoomCount=179;
    const int animations=animationTicks,simulation=simulationTicks;
    Detour_CrouchDraw(itemMemory,0);
    Check(mirrorOuterCalls==2 && mirrorNativeDraws==1 && hairs==1,"one extra body/head/hair render");
    Check(mirrorObservedMask==UINT32_MAX,"mirror body includes head and all limbs");
    Check(bits==0x3fff && flags==3 && !testMirrorCapturing && !g_holsterRenderScope && !g_bodySkinReady,"original visibility, controls and render scopes restored");
    Check(UnarmedIKReady() && FullBodyIKReady(),"ordinary first-person tracking remains enabled after reflection");
    Check(animationTicks==animations && simulationTicks==simulation,"reflection never advances game simulation");
    Check(testMirrorPass==0,"opaque native pass forwarded to mirror capture");
    // Y+LT switches to third person without removing the installed draw hook.
    mirrorTestFirstPerson=false;g_active=g_scenePoseValid=g_runtimeEnabled=false;bits=0x36cf;
    Check(MirrorRoomEligible(itemMemory),"third-person gameplay is eligible without a first-person camera");
    Detour_CrouchDraw(itemMemory,0);
    Check(mirrorOuterCalls==4 && mirrorNativeDraws==2 && hairs==2,"third person gets one native draw and one mirrored draw");
    Check(bits==0x36cf && flags==3 && !testMirrorCapturing && !g_holsterRenderScope,"third-person mask, controls and pose remain native");
    for(int pass:{1,2}) {
        nativeShadowPass=pass;Detour_CrouchDraw(itemMemory,0);
        Check(testMirrorPass==pass,"transparent and additive native passes reach mirror capture");
    }
    nativeShadowPass=0;
    // The user's live snapshot identifies room 135, next to the first restroom.
    const auto& newProfile=mirror::Profiles[1];auto* newRoom=mirrorRooms+135*304;
    static uint8_t newChunk[44]{};
    *reinterpret_cast<int32_t*>(newRoom+40)=newProfile.originX;*reinterpret_cast<int32_t*>(newRoom+48)=newProfile.originZ;
    *reinterpret_cast<int32_t*>(newRoom+256)=1;*reinterpret_cast<uint8_t**>(newRoom+264)=newChunk;
    *reinterpret_cast<uint32_t*>(newChunk+36)=newProfile.first;*reinterpret_cast<uint32_t*>(newChunk+40)=newProfile.count;
    *reinterpret_cast<int16_t*>(itemMemory+28)=135;
    Check(MirrorRoomProfile(itemMemory)==&newProfile,"actual room gate recognizes the user's second restroom");
    Detour_CrouchDraw(itemMemory,0);
    Check(testMirrorProfile==&newProfile,"native draw forwards room 135's geometry to the renderer");
    *reinterpret_cast<int16_t*>(itemMemory+28)=134;
    for(int ui:{drva::app_off::InTitle,drva::app_off::InventoryActive,drva::app_off::InFMV,drva::app_off::photoMode}) {
        *reinterpret_cast<int32_t*>(appMemory+ui)=1;
        Check(!MirrorRoomEligible(itemMemory),"third-person menus, movies and photo mode excluded");
        *reinterpret_cast<int32_t*>(appMemory+ui)=0;
    }
    // Replay the real shipped facial skeleton in both camera modes.
    static uint8_t faceGeometries[4*120]{};
    auto* geom=mirrorObjects+off::object_geom;
    *reinterpret_cast<int32_t*>(geom+28)=33;
    *reinterpret_cast<const int32_t**>(geom+48)=AuthoredFaceMap;
    *reinterpret_cast<void**>(geom+16)=faceGeometries;
    *reinterpret_cast<void**>(faceGeometries+16)=faceGeometries;dll.gLaraHeads=rva(faceGeometries);
    for(int j=0;j<33;++j) WriteRows(Frame{{{{1,0,0},{0,1,0},{0,0,1}}},{float(j),float(j*j),float(-j*7)}},faceBefore+j*12);
    skinFixture=faceBefore;skinFixtureCount=33;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&MirrorFaceDraw);
    testMirrorCapturing=true;
    for(bool fp:{false,true}) {
        g_active=g_scenePoseValid=g_runtimeEnabled=fp;
        Detour_DrawCreatureHD(itemMemory,0,0);
        Check(!g_mirrorHeadScope && !g_bodySkinReady && bits==0x36cf,"face and visibility scopes restored after native render");
    }
    testMirrorCapturing=false;skinFixture=nullptr;skinFixtureCount=0;
    // The native RUN hand must not borrow the resting VRIK hand descriptor.
    dll.gLaraHeads=0;
    static uint8_t hands[63*off::geom_stride]{};
    UnarmedHandDll handDll{dll.timestamp,rva(hands)};g_unarmedHandDll=&handDll;
    const auto* run=hands+2*off::geom_stride;const auto* rest=hands+off::geom_stride;
    *reinterpret_cast<const void**>(hands+2*off::geom_stride+off::geom_mesh)=run;
    *reinterpret_cast<const void**>(hands+off::geom_stride+off::geom_mesh)=rest;
    std::memcpy(geom,run,off::geom_stride);bits=0x3600;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&ObserveUnarmedHandDraw);
    Check(UnarmedIKReady(),"native hand regression has active controller IK");
    testMirrorCapturing=true;Detour_DrawCreatureHD(itemMemory,1,0);
    Check(!std::memcmp(observedHandGeometry,run,off::geom_stride),"reflection uses original animated hand geometry");
    testMirrorCapturing=false;Detour_DrawCreatureHD(itemMemory,1,0);
    Check(!std::memcmp(observedHandGeometry,rest,off::geom_stride),"ordinary first-person view still uses its tracked resting hands");
    Check(!std::memcmp(geom,run,off::geom_stride) && bits==0x3600,"hand descriptor and visibility are restored between views");
    g_unarmedHandDll=nullptr;
    // Native TR4 calls the real scoped hooks, independently of the GL capture.
    TestDynamicBonesMirrorArguments();
    g_hNativeMirror.m_trampoline=reinterpret_cast<void*>(&NativeMirrorRoutine);
    g_hCrouchDraw.m_trampoline=reinterpret_cast<void*>(&NativeMirrorOuter);
    g_hDrawHair.m_trampoline=reinterpret_cast<void*>(&NativeMirrorHair);
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&MirrorNativeDraw);
    std::memset(geom,0,off::geom_stride);skinFixture=bodyBefore;skinFixtureCount=15;
    dll.game=game=0;nativeMirrorExpected=true;
    g_active=g_scenePoseValid=g_runtimeEnabled=true;
    g_meshOverride=true;g_meshItem=itemMemory;g_meshBaseBits=0x7fff;bits=0x3fff;
    std::memcpy(appMemory+0x508,nativeSelections,5);
    for(int level:{9,15}) for(bool hd:{false,true}) for(int pass:{0,1,2}) {
        *reinterpret_cast<int32_t*>(appMemory+1264)=level;
        *reinterpret_cast<int16_t*>(itemMemory+28)=level==9 ? 11 : 69;
        flags=hd ? 3 : 2;nativeShadowPass=pass;
        // Sacred Lake's native mirror is underwater. Its first-person body
        // may be completely hidden while the reflected body must stay intact.
        g_underwaterHidden=level==9;bits=level==9 ? 0u : 0x3fffu;
        const uint32_t normalBits=bits;
        const int before=nativeMirrorCalls;
        Detour_NativeMirror();
        Check(nativeMirrorCalls==before+1,"one native reflection call per material pass in both TR4 rooms");
        Check(bits==normalBits && flags==(hd ? 3u : 2u) && !FirstPersonNativeMirrorDrawing(),
              "normal view visibility, controls and mirror scope restored after each pass");
        Check(nativeMirrorHairFlag==1,"native hair retains its mirror transform flag in both graphics modes");
        Check(animationTicks==animations && simulationTicks==simulation,"native mirror wrapper does not advance simulation");
    }
    g_underwaterHidden=false;bits=0x3fff;
    // Replay the authored face inside the native mirror with FP hiding active.
    flags=3;nativeShadowPass=0;
    *reinterpret_cast<int32_t*>(geom+28)=33;*reinterpret_cast<const int32_t**>(geom+48)=AuthoredFaceMap;
    *reinterpret_cast<void**>(geom+16)=faceGeometries;dll.gLaraHeads=rva(faceGeometries);
    skinFixture=faceBefore;skinFixtureCount=33;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&MirrorFaceDraw);
    Detour_NativeMirror();
    Check(!g_mirrorHeadScope && !g_bodySkinReady && bits==0x3fff,"native mirrored facial scope restores normal head hiding");
    // Disabled/inactive/other-game/shadow calls retain their original behavior.
    nativeMirrorExpected=false;
    for(int gate=0;gate<5;++gate) {
        config.firstPersonMirrorReflections=gate!=0;
        g_active=gate!=1;g_scenePoseValid=gate!=2;
        dll.game=gate==3 ? 1 : 0;nativeShadowPass=gate==4 ? 4 : 0;
        Detour_NativeMirror();
        Check(bits==0x3fff && flags==3 && !g_nativeMirrorDraw,"excluded native calls do not alter mesh or render state");
    }
    nativeShadowPass=0;g_active=g_scenePoseValid=true;
    Check(BodyVisualOffsetApplies(itemMemory),"normal room-scale body fitting resumes after native mirror draw");
    skinFixture=nullptr;skinFixtureCount=0;
    for(int gate=0;gate<5;++gate) {
        config.firstPersonMirrorReflections=gate!=0;flags=gate==1 ? 2 : 3;
        *reinterpret_cast<int32_t*>(appMemory+1264)=gate==2 ? 11 : 12;
        *reinterpret_cast<int16_t*>(itemMemory+28)=gate==3 ? 133 : 134;dll.game=gate==4 ? 0 : 1;
        Check(!MirrorRoomEligible(itemMemory),"feature opt-out, Classic graphics, other levels/rooms/games excluded");
    }
    VR().m_system=nullptr;
    std::printf("Mirror focused checks passed: %d\n",checks);return 0;
}
