from pathlib import Path
p=Path('src/FirstPerson.cpp'); s=p.read_text(); s=s.replace('*reinterpret_cast<const int32_t*>(App()+L().app_level)','AppState(drva::app_off::level)')
s=s.replace('uint32_t g_bodySkinMask=0;','uint64_t g_bodySkinMask=0;')
s=s.replace('    uint32_t findTargetPoint, lineOfSight;','    uint32_t findTargetPoint, lineOfSight;\n    uint32_t animatePistols, getSpheres, nextItemActive, items;')
for old,new in [('0x5E2F0, 0x149F0','0x5E2F0, 0x149F0, 0x5A680, 0xA3C30, 0x63DAB0, 0x699CA0'),('0x5B5D0, 0x12450','0x5B5D0, 0x12450, 0x579C0, 0x9CF90, 0x65AC92, 0x66D1A8'),('0x5DFF0, 0x145D0','0x5DFF0, 0x145D0, 0x5A380, 0xA47D0, 0x63E9F0, 0x69ABE0'),('0x5C320, 0x12500','0x5C320, 0x12500, 0x58710, 0x9D300, 0x65ABD2, 0x66D0E8')]: s=s.replace(old,new)
s=s.replace('const uint8_t kPistolHandlerTR4', 'const uint8_t kAnimatePistols[] = {0x48,0x89,0x5C,0x24,0x10};\nconst uint8_t kPistolHandlerTR4',1)
u=Path('C:/dev/TombRaider123VR/src/FirstPerson.cpp').read_text(); a=u.index('uint8_t* SelectGunTarget'); b=u.index('int32_t __cdecl Detour_FireWeapon',a); fn=u[a:b]
fn=fn.replace('const auto& d=*g_boundDll;','if (!g_motionDll) return nullptr;\n    const auto& d=*g_motionDll;').replace('d.los','d.lineOfSight').replace('d.laraItem','g_boundDll->laraItem').replace('d.getFloor','g_boundDll->getFloor').replace('d.objects','g_boundDll->objects').replace('off::item_room_number','off::item_room').replace('off::item_object_number','off::item_object')
fn=fn.replace('    int index=', '    const bool allowAssist=!FirstPersonHKScopeAiming();\n    int index=')
fn=fn.replace('index>=0 && visited<4096','index>=0 && index<4096 && visited<4096')
fn=fn.replace('if (bodyDistance==','if (!allowAssist || bodyDistance==').replace('bodyPoint,candidate,\n                               Cfg().firstPersonAutoAimDegrees)','bodyPoint,candidate)')
s=s.replace('int32_t FireWeaponForHand',fn+'int32_t FireWeaponForHand',1)
s=s.replace('        if (target) hpBefore=', '        g_firingDirection=gun.direction;\n        target=SelectGunTarget(gun,g_firingDirection);\n        if (target) hpBefore=',1)
s=s.replace('        g_firingDirection = gun.direction;\n        assisted=AssistGunShot(gun,target,g_firingDirection);','        assisted=motiongun::Dot(motiongun::Sub(g_firingDirection,gun.direction),\n            motiongun::Sub(g_firingDirection,gun.direction))>1e-8f;')
s=s.replace('if (tracked && g_motionShots[hand]==1)','if (tracked && g_motionShots[hand]<=100)')
a=u.index('void __cdecl Detour_AnimatePistols'); b=u.index('int32_t __cdecl Detour_GetTargetOnLOS',a); s=s.replace('void __cdecl Detour_PistolHandler',u[a:b]+'void __cdecl Detour_PistolHandler',1)
# GetJoints is also required for untracked first-person body drawing.
s=s.replace('    if (Cfg().firstPersonMotionGuns) {\n        for (const auto& entry : kMotionDlls)\n            if (entry.timestamp == d.timestamp) g_motionDll = &entry;', '''    for (const auto& entry : kMotionDlls)
        if (entry.timestamp == d.timestamp) g_motionDll = &entry;
    if (g_motionDll && !g_hGetJoints.Install(
            reinterpret_cast<void*>(base+g_motionDll->getJoints),
            reinterpret_cast<void*>(&Detour_GetJoints),5,
            kGetJointsPrologue,sizeof(kGetJointsPrologue),"GetJoints"))
        Log("firstperson: body palette correction unavailable; native masking retained");
    if (Cfg().firstPersonMotionGuns) {''')
a=s.index('            g_hGetJoints.Install(',s.index('if (Cfg().firstPersonMotionGuns) {',s.index('bool Install'))); b=s.index('            g_hFireWeapon.Install',a)
s=s[:a]+'''            g_hGetJoints.installed() &&
            g_hAnimatePistols.Install(
                reinterpret_cast<void*>(base+g_motionDll->animatePistols),
                reinterpret_cast<void*>(&Detour_AnimatePistols),5,
                kAnimatePistols,sizeof(kAnimatePistols),"AnimatePistols") &&
'''+s[b:]
s=s.replace('g_hPistolHandler.Remove();','g_hPistolHandler.Remove();\n    g_hAnimatePistols.Remove();')
# Keep independently installed body GetJoints hook when optional motion hooks fail.
s=s.replace('            g_hGetJoints.Remove();','').replace('        g_hGetJoints.Remove();','')
# Body scope is captured before native MaskJoints, restored by the shader path only.
s=s.replace('    g_renderHandJoint = -1;\n    const int32_t count', '    g_renderHandJoint = -1;\n    g_bodySkinReady=false;\n    const int32_t count',1)
a=s.index('    if (g_renderArm < 0 || count < 15',s.index('int32_t __cdecl Detour_GetJoints'))
s=s[:a]+'''    if (g_renderArm<0 && joints && count>0 && count<=33 && g_active &&
        g_boundDll && item && item==g_headingItem &&
        item==*Ptr<uint8_t*>(g_boundDll->laraItem)) {
        if (g_scenePoseValid && CanTurnBody(item))
            stabilization::OffsetBodyPalette(joints,count,g_bodyVisualOffset);
        const int object=*reinterpret_cast<const int16_t*>(item+off::item_object);
        if (g_bodySkinScope && object>=0) {
            const auto* geom=Ptr<uint8_t>(g_boundDll->objects)+object*off::object_stride+off::object_geom;
            const int bones=*reinterpret_cast<const int32_t*>(geom+28);
            const auto* mapping=bones>0 ? *reinterpret_cast<const int32_t* const*>(geom+48) : nullptr;
            if (bones<=0 || (bones==count && mapping)) {
                g_bodySkinMask=firstperson::SkinJointMask(
                    *reinterpret_cast<const uint32_t*>(item+off::item_mesh_bits),mapping,count);
                std::memcpy(g_bodySkinPalette,joints,count*12*sizeof(float));
                g_bodySkinCount=count; g_bodySkinReady=true;
            }
        }
    }
'''+s[a:]
s=s.replace('        g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item, 1, renderPass);\n        bits = savedBits;', '        g_bodySkinScope=true; g_bodySkinReady=false;\n        g_hDrawCreatureHD.Original<Fn_DrawCreatureHD>()(item, 1, renderPass);\n        g_bodySkinScope=g_bodySkinReady=false;\n        bits = savedBits;')
s=s.replace('bool FirstPersonActive() {','const float* FirstPersonBodyPalette(uint64_t& mask,int& count) {\n    mask=g_bodySkinMask; count=g_bodySkinCount;\n    return g_bodySkinScope && g_bodySkinReady && g_renderArm<0 ? g_bodySkinPalette : nullptr;\n}\n\nbool FirstPersonActive() {')
p.write_text(s)
p=Path('src/Engine.h'); s=p.read_text().replace('namespace app_off {','namespace app_off {\nconstexpr uint32_t level           = 1264;'); p.write_text(s)
p=Path('src/FirstPersonVisibility.h'); s=p.read_text().replace('uint32_t SkinJointMask','uint64_t SkinJointMask').replace('uint32_t result=0','uint64_t result=0').replace('i<32','i<64').replace('result|=uint32_t(1)<<i','result|=uint64_t(1)<<i'); p.write_text(s)
p=Path('src/FirstPerson.h'); s=p.read_text().replace('bool FirstPersonActive();','bool FirstPersonActive();\n// Unmasked palette only within Lara\'s first-person body draw.\nconst float* FirstPersonBodyPalette(uint64_t& mask,int& count);'); p.write_text(s)
