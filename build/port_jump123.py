from pathlib import Path
root=Path('build/jump-parity123');p=root/'src/FirstPerson.cpp'
s=p.read_text(encoding='utf-8');ref=Path('src/FirstPerson.cpp').read_text(encoding='utf-8')
def change(old,new,count=1):
 global s
 assert s.count(old)==count,(old[:80],s.count(old),count)
 s=s.replace(old,new)
change('typedef void (__cdecl* Fn_LaraAboveWater)(uint8_t*, void*);','''typedef void (__cdecl* Fn_LaraAboveWater)(uint8_t*, void*);
struct JumpDll { uint32_t timestamp,compress; uint8_t signature[8]; };
#include "JumpAddresses.inl"
Fn_LaraAboveWater g_nativeJumpCompression=nullptr;''')
change('bool CanFitStandingBody(const uint8_t* item) {','''bool GroundBodyAvailable(const uint8_t* item) {
    return item && g_boundDll && g_boundBase && !InteractionAligning() && LaraWaterStatus()==0 &&
        *reinterpret_cast<const int16_t*>(item+off::item_hit_points)>0 &&
        *reinterpret_cast<const int16_t*>(g_boundBase+g_boundDll->lara+40)<0 &&
        !(BoundGame()==3 && (*reinterpret_cast<const uint16_t*>(g_boundBase+g_boundDll->lara+60)&4u));
}
bool StandingJumpPreparation(const uint8_t* item) {
    return g_active && Cfg().firstPersonMovementStabilization && g_groundEye.valid &&
        GroundBodyAvailable(item) && *reinterpret_cast<const int16_t*>(item+off::item_anim_state)==15 &&
        !(*reinterpret_cast<const uint16_t*>(item+off::item_flags)&8u);
}
bool CanFitStandingBody(const uint8_t* item) {''')
change('return CanWalk(item) || (item &&','return CanWalk(item) || StandingJumpPreparation(item) || (item &&')
change('    if(IsNativeAirborneBodyState(item)) {','    const bool preparingJump=StandingJumpPreparation(item);\n    if(IsNativeAirborneBodyState(item) && !preparingJump) {')
change('if(!CanFitStandingBody(item) || landing ||','if(!CanFitStandingBody(item) || preparingJump || landing ||')
change('if(mainBody && !nativeLanding && CanFitStandingBody(item))','if(mainBody && !nativeLanding && !preparingJump && CanFitStandingBody(item))')
change('} else if(ordinaryLanding && g_jumpBodyObject==object','} else if((ordinaryLanding || preparingJump) && g_jumpBodyObject==object')
change('const bool standing=ordinaryLanding ||','const bool standing=ordinaryLanding || preparingJump ||')
change('const bool quiet=ordinaryLanding ||','const bool quiet=ordinaryLanding || preparingJump ||')
change('void AdvanceLaraAnimation(uint8_t* item) {','void SkipFirstPersonJumpPreparation(uint8_t* item);\nvoid AdvanceLaraAnimation(uint8_t* item) {')
change('    g_hAnimateLara.Original<Fn_AnimateLara>()(item);','    SkipFirstPersonJumpPreparation(item);\n    g_hAnimateLara.Original<Fn_AnimateLara>()(item);')
change('bool GroundJumpReady(const uint8_t* item) {','''bool GroundJumpControlsAvailable() {
    if(!g_boundDll || !g_boundBase || InCutscene() || AppFlag(drva::app_off::level_type)==3) return false;
    // TR1-3 has no TR4/5 bDisableLaraControl/spot-camera flags. Native
    // cinematic levels, cinematic camera and TR3 extra-animation sequences
    // own control. Playable fixed/heavy cameras only own presentation.
    if(BoundGame()==3 && (*Ptr<uint16_t>(g_boundDll->lara+60)&4u)) return false;
    const int camera=*Ptr<int32_t>(g_boundDll->camera+off::camera_type);
    return camera>=0 && camera<=5 && camera!=kCamCinematic;
}
bool GroundJumpReady(const uint8_t* item) {''')
change('    const int camera=*Ptr<int32_t>(g_boundDll->camera+off::camera_type);\n    if (camera==kCamFixed || camera>=kCamCinematic) return false;','    if(!GroundJumpControlsAvailable()) return false;')
change('        distance=(frames+1)*speed;','        distance=(g_active && g_nativeJumpCompression ? 1 : frames+1)*speed;')
change('           *reinterpret_cast<const int16_t*>(item+off::item_hit_points)<=0 ||\n           *reinterpret_cast<const int16_t*>(item+off::item_anim_state)!=15) return;',
       '           *reinterpret_cast<const int16_t*>(item+off::item_hit_points)<=0) return;')
change("        if(status==4 || (status==2 && (game==1 || weapon!=(game==2 ? 8 : 9)))) return;",'''        const bool armed=status==4 || (status==2 && (game==1 || weapon!=(game==2?8:9)));
        const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
        if(state==15) { if(armed) return; }
        else if(!armed || !CanWalk(item) || !(*Ptr<uint32_t>(g_boundDll->input)&0x10u)) return;''')
change('void ObserveJumpLanding(uint8_t* item) {','#include "JumpParity.inl"\n\nvoid ObserveJumpLanding(uint8_t* item) {')
change('void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {\n    using namespace locomotion;',
       'void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {\n    using namespace locomotion;\n    FinishFirstPersonLanding(item);')
change('    {\n        LedgePullInputScope pull(item,TurnTime());','    const bool airborneBefore=item && (*reinterpret_cast<const uint16_t*>(item+off::item_flags)&8u);\n    {\n        LedgePullInputScope pull(item,TurnTime());')
change('    ObserveJumpLanding(item);\n    g_groundMoveAction = 0;','    ObserveJumpLanding(item);\n    FinishFirstPersonLanding(item,airborneBefore);\n    g_groundMoveAction = 0;')
change('        g_ledgePull.Reset(); g_groundJump.Reset();','        g_ledgePull.Reset();\n        if(!sameBody || relocated) g_groundJump.Reset();')
change('Cfg().firstPersonMovementStabilization && state==25 &&','Cfg().firstPersonMovementStabilization && state>=25 && state<=27 &&')
change('            !CanWalk(item));','            !CanWalk(item) && !StandingJumpPreparation(item));')
change('''    UpdateControlMode(g_active && g_runtimeEnabled && g_boundDll && g_boundBase &&
        Cfg().enabled && VR().active());''','''    const bool enabled=g_runtimeEnabled && g_boundDll && g_boundBase && Cfg().enabled && VR().active();
    bool playableOverride=false;
    if(enabled && g_controlModeOwned && VR().poseValid() && LaraWaterStatus()==0 &&
       !InInventory() && !InTitle() && GroundJumpControlsAvailable()) {
        const int camera=*Ptr<int32_t>(g_boundDll->camera+off::camera_type);
        playableOverride=camera==kCamFixed || camera==5;
    }
    UpdateControlMode(enabled && (g_active || playableOverride));''')
change('bool Install(const GameDllLayout& d, uint64_t base) {','''bool Install(const GameDllLayout& d, uint64_t base) {
    g_nativeJumpCompression=nullptr;
    for(const auto& jump:kJumpDlls) if(jump.timestamp==d.timestamp &&
        !std::memcmp(reinterpret_cast<const void*>(base+jump.compress),jump.signature,sizeof(jump.signature)))
        g_nativeJumpCompression=reinterpret_cast<Fn_LaraAboveWater>(base+jump.compress);
    if(!g_nativeJumpCompression) Log("firstperson: instant jump unavailable; native preparation retained");''')
change('void Remove() {','void Remove() {\n    g_nativeJumpCompression=nullptr;')
p.write_text(s,encoding='utf-8')

# Adapt the already-tested reference algorithms to TR1-3's 40-byte animation
# records and 16-bit ITEM_INFO flags. Never transplant TR4/5 offsets.
landing=ref[ref.index('bool FinishFirstPersonLanding('):ref.index('\nvoid PrepareGroundDirection',ref.index('bool FinishFirstPersonLanding('))]
takeoff=ref[ref.index('void SkipFirstPersonJumpPreparation(uint8_t* item) {'):ref.index('\nvoid ObserveJumpLanding',ref.index('void SkipFirstPersonJumpPreparation(uint8_t* item) {'))]
code=landing+'\n'+takeoff
code=code.replace('(g_boundDll->game!=0 && g_boundDll->game!=1)','(BoundGame()<1 || BoundGame()>3)')
code=code.replace('g_boundDll->laraAsCompress','g_nativeJumpCompression')
code=code.replace('reinterpret_cast<Fn_LaraAboveWater>(g_boundBase+g_nativeJumpCompression)(item,nullptr)','g_nativeJumpCompression(item,nullptr)')
code=code.replace('locomotion::IsLandingAnimation(g_boundDll->game,state,clip)','locomotion::IsLandingAnimation(state,clip)')
code=code.replace('reinterpret_cast<const uint32_t*>(item+0x1820)','reinterpret_cast<const uint16_t*>(item+off::item_flags)')
code=code.replace('g_boundDll->lara+off::lara_move_angle','g_boundDll->lara+254')
code=code.replace('*48','*off::anim_stride')
for var in ['next','a','b','compression']:
 for old,new in [(28,20),(30,22),(32,24),(34,26),(40,32)]:
  code=code.replace(f'{var}+{old})',f'{var}+OFFSET_{new})')
code=code.replace('OFFSET_','')
code=code.replace('(frames==7 ? 5 : 2)','(frames==7 ? BoundGame()+2 : BoundGame()==1 ? 1 : 2)')
code=code.replace('(goal==1 ? 6 : 0)','(goal==1 ? BoundGame()*2 : 0)')
code=code.replace('(compressionCommands!=2 && compressionCommands!=4)','compressionCommands!=(BoundGame()==1 ? 2 : 3)')
code=code.replace('Both supported binaries leave the COLL_INFO argument unused.','All three native routines leave the COLL_INFO argument unused.')
(root/'src/JumpParity.inl').write_text('// First-person jump/landing port; native animation commands own physics.\n'+code,encoding='utf-8')
print('Applied staged jump parity port')

