from pathlib import Path
src=Path('C:/dev/TombRaider123VR/src')
def read(p): return p.read_text()
def write(p,s): p.write_text(s)
p=Path('src/MotionGunInput.h'); s=read(p); upstream=read(src/'MotionGunInput.h')
a=s.index('// One queued shot'); b=s.index('// Adapt the gesture')
u=upstream[upstream.index('// LT queues'):upstream.index('// Adapt the gesture')]
s=s[:a]+u+s[b:]
a=upstream.index('// AnimatePistols'); b=upstream.index('// SteamVR')
s=s.replace('// Native TR4/5',upstream[a:b]+'// Native TR4/5',1); write(p,s)
p=Path('src/FirstPersonStabilization.h'); s=read(p); u=read(src/p.name); s=s[:s.index('struct GroundEye')]+u[u.index('struct GroundEye'):]; a=u.index('inline void OffsetBodyPalette'); b=u.index('// Keep a standing',a); s=s.replace('struct GroundEye',u[a:b]+'struct GroundEye',1); write(p,s)
p=Path('src/LocomotionMath.h'); s=read(p); u=read(src/p.name); a=u.index('// Collision pushback'); b=u.index('inline float Wrap',a); s=s.replace('inline float Wrap',u[a:b]+'inline float Wrap',1); write(p,s)
p=Path('src/FirstPersonClearance.h'); s=read(p); u=read(src/p.name); a=u.index('// GetCollisionInfo'); b=u.index('} // namespace'); s=s.replace('// Sweep',u[a:b]+'\n// Sweep',1); write(p,s)
p=Path('src/MotionGunMath.h'); s=read(p); u=read(src/p.name); a=u.index('// Match FireWeapon'); b=u.index('inline Vec Transform',a); s=s.replace('inline Vec CollisionEndpoint',u[a:b]+'inline Vec CollisionEndpoint',1); s=s.replace('// only the native selected, living, unobstructed target.','// a living, unobstructed candidate selected for this controller.'); write(p,s)
write(Path('src/FirstPersonVisibility.h'),read(src/'FirstPersonVisibility.h'))
p=Path('src/FirstPerson.cpp'); s=read(p)
s=s.replace('#include "FirstPersonClearance.h"','#include "FirstPersonClearance.h"\n#include "FirstPersonVisibility.h"').replace('#include <cstring>','#include <cstring>\n#include <limits>')
s=s.replace('constexpr uint32_t item_mesh_bits','constexpr uint32_t item_stride = 9416;\nconstexpr uint32_t item_next_active = 32;\nconstexpr uint32_t lara_target = 208;\nconstexpr uint32_t item_mesh_bits',1)
s=s.replace('hook::InlineHook g_hPistolHandler;','hook::InlineHook g_hPistolHandler;\nhook::InlineHook g_hAnimatePistols;')
s=s.replace('stabilization::GroundEye g_groundEye;','stabilization::GroundEye g_groundEye;\nlocomotion::Vec g_bodyVisualOffset{};\nfloat g_lastBodyYaw=0;\nint g_headingLevel=-1;\nbool g_bodySkinScope=false, g_bodySkinReady=false;\nfloat g_bodySkinPalette[33*12]{};\nuint32_t g_bodySkinMask=0;\nint g_bodySkinCount=0;')
s=s.replace('    g_groundEye.Reset();','    g_groundEye.Reset();\n    g_bodyVisualOffset={};',1)
s=s.replace('    if (!*Ptr<uint8_t*>(dll.laraItem)) return false;','    const auto* item=*Ptr<uint8_t*>(dll.laraItem);\n    if (!item || !firstperson::UseHeadCamera(\n        *reinterpret_cast<const int16_t*>(item+off::item_hit_points))) return false;')
s=s.replace('        if (Length(Vec{float(acceptedX), float(acceptedZ)}) > 64) break;','        if (!AcceptCollisionDragStep({float(dx),float(dz)},\n            {float(acceptedX),float(acceptedZ)})) break;')
s=s.replace('    if (g_haveHeading && g_headingItem == item && !relocated) {','    const int level=*reinterpret_cast<const int32_t*>(App()+L().app_level);\n    const bool sameBody=g_headingItem==item && g_headingLevel==level;\n    if (g_haveHeading && sameBody && !relocated) {',1)
# app_level field verified below before compilation
s=s.replace('    if (!g_haveHeading || g_headingItem != item || relocated) {\n        ResetMovementStabilization();\n        const float facing = g_headingItem == item && !relocated\n            ? g_lastHeadWorld : Radians(pos.y_rot);\n        g_headingBase = Wrap(facing - VR().HeadYawRadians());','    if (!g_haveHeading || !sameBody || relocated) {\n        g_renderTurn.Reset(); g_turnTrace={}; g_rootMotion.Reset();\n        const float oldBase=g_headingBase;\n        const float bodyTurn=sameBody ? Wrap(Radians(pos.y_rot)-g_lastBodyYaw) : 0;\n        const float facing = sameBody && !relocated\n            ? Wrap(g_lastHeadWorld+bodyTurn) : Radians(pos.y_rot);\n        g_headingBase = Wrap(facing - VR().HeadYawRadians());\n        g_groundEye.Resume(sameBody,oldBase,g_headingBase,bodyTurn);\n        g_headingLevel=level;')
s=s.replace('    TurnBodyToHead(item, Elapsed(g_bodyTime));','    TurnBodyToHead(item, Elapsed(g_bodyTime));\n    g_lastBodyYaw=Radians(pos.y_rot);')
a=s.index('    // AS_SPLAT (12)',s.index('void ClampRenderedHeadToCollision')); b=s.index('    double eyeY',a); s=s[:a]+s[b:]
s=s.replace('    if (airborne && Cfg().positionalTracking','    if (Cfg().positionalTracking')
s=s.replace('            coll.badPos = airborne ? 4096 : 384;','            coll.badPos = 4096;').replace('            coll.badNeg = airborne ? -4096 : -384;','            coll.badNeg = -4096;').replace('            coll.flags = airborne ? 0 : 5; // airborne eyes need geometry, not walkable slope/pit rules','            coll.flags = 0; // Eye clearance permits empty space beyond a ledge.')
a=s.index('            if (airborne) {'); b=s.index('\n        });',a)
s=s[:a]+'''            return firstperson::EyeBlocked(coll.floorSamples,body[1],eyeY,coll.hitStatic!=0) ||
                coll.type == 8 || coll.type == 16 || coll.type == 32 ||
                coll.shift[0] || coll.shift[2];'''+s[b:]
s=s.replace('{float(head.x),float(head.y),float(head.z)});','{float(head.x),float(head.y),float(head.z)},\n            Radians(old.y_rot)+Wrap(Radians(pos.y_rot)-Radians(old.y_rot))*t);',1)
s=s.replace('// Real relocation, item changes, FP exit and cutscenes reset it separately.','// Item/level changes and explicit view toggles discard this calibration.')
u=read(src/'FirstPerson.cpp'); a=u.index('void FitBodyToRenderedEye('); b=u.index('void __cdecl Detour_GenerateW2V',a); fn=u[a:b].replace('CanWalk(item)','CanTurnBody(item)').replace('g_heading.base','g_headingBase').replace('180/Pi','180/kPi').replace('Lerp(prev.x_pos,pos.x_pos,frac)','int32_t(prev.x_pos+(int64_t(pos.x_pos)-prev.x_pos)*frac/256)').replace('Lerp(prev.z_pos,pos.z_pos,frac)','int32_t(prev.z_pos+(int64_t(pos.z_pos)-prev.z_pos)*frac/256)').replace('Cfg().firstPersonHeadTranslation) {','Cfg().firstPersonHeadTranslation && !VR().headAtCamera()) {')
s=s.replace('bool Anchor(PHD_3DPOS& pose)',fn+'bool Anchor(PHD_3DPOS& pose)')
s=s.replace('    ClampRenderedHeadToCollision(item, pose);','    ClampRenderedHeadToCollision(item, pose);\n    FitBodyToRenderedEye(item,pose);')
s=s.replace('        ResetMovementStabilization();\n        g_bodyTime = {};','        g_renderTurn.Reset(); g_turnTrace={}; g_rootMotion.Reset();\n        g_bodyVisualOffset={};\n        g_bodyTime = {};')
write(p,s)
