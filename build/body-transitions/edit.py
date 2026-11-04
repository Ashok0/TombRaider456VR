from pathlib import Path
import shutil
roots=[Path('C:/dev/TombRaider456VR'),Path('C:/dev/TombRaider456VR/build/body-transitions/123-work')]
for index,root in enumerate(roots):
 p=root/'src/FirstPerson.cpp'
 shutil.copy2(p,Path('C:/dev/TombRaider456VR/build/body-transitions')/f'before-{index}.cpp')
 s=p.read_text(encoding='utf-8')
 def sub(a,b):
  global s
  assert s.count(a)==1,(index,a[:90],s.count(a))
  s=s.replace(a,b)
 # Sprint dive is a separate native roll state in TR3/4/5.
 sub('case 23: case 45: case 66: case 68: case 72: return true;',
     'case 23: case 45: case 66: case 68: case 72: return true;\n    case 74: return '+('true;' if index==0 else 'BoundGame()==2;')+' // Native dashdive (running somersault).')
 if index==0:
  sub('if (trackedHands) hideRoll=hideArms=false;','if (trackedHands) hideArms=false;')
  sub('g_active && g_rollHidden && !MotionReady() && g_boundDll','g_active && g_rollHidden && g_boundDll')
  sub('(status!=0 && !crouchHands) ||','(status!=0 && !crouchHands && !((status==2 || status==3) && MotionWeaponSupported(CurrentMotionWeapon()))) ||')
  sub('if(variant==4 && state!=28) continue;','if(variant==4 && state!=28 && *Ptr<int16_t>(g_boundDll->lara+2)!=2 &&\n                   *Ptr<int16_t>(g_boundDll->lara+2)!=3) continue;')
  sub('    const UnarmedRestHandScope restHands(item,lara ? useMeshBits : 0);\n','')
  sub('    if (motion) {\n        // Apply the same transition mask','    const UnarmedRestHandScope restHands(item,lara ? useMeshBits : 0);\n    if (motion) {\n        // Apply the same transition mask')
  sub('    const bool nativeWeaponArms=(weaponStatus==2 || weaponStatus==3) && !MotionReady();\n    if(nativeWeaponArms) for(auto& arm:g_traversalArms) arm.Reset();\n    for(int hand=0;!nativeWeaponArms && hand<2;++hand) {',
      '    // Native equip/holster changes weapon state, never controller ownership.\n    for(int hand=0;hand<2;++hand) {')
 else:
  sub('if (MotionReady() || UnarmedIKReady()) hideRoll=hideCrouch=ledgeArms=hideArms=false;',
      'if (MotionReady() || UnarmedIKReady()) hideCrouch=ledgeArms=hideArms=false;')
  sub('    if (status!=0 && !(status==1 && CanSteerCrouch(item))) return false;',
      '    const bool transition=!shadow && (status==2 || status==3) && MotionWeaponSupported(CurrentMotionWeapon());\n    if (status!=0 && !transition && !(status==1 && CanSteerCrouch(item))) return false;')
  sub('if(variant==3 && state!=28) continue;', 'if(variant==3 && state!=28 && *Ptr<int16_t>(g_boundDll->lara+off::lara_gun_status)!=2 &&\n               *Ptr<int16_t>(g_boundDll->lara+off::lara_gun_status)!=3) continue;')
  sub('lara && !trackedWeapon ? useMeshBits : 0','lara ? useMeshBits : 0')
  sub('    const uint32_t bodyMask=lara ? FirstPersonBodyMask',
      '    if(lara && g_rollHidden) return; // Includes tracked weapon/body passes.\n    const uint32_t bodyMask=lara ? FirstPersonBodyMask')
  sub('    const bool nativeWeaponArms=(weaponStatus==2 || weaponStatus==3) && !PhysicalHolsterOwnsWeapon();\n    for(int hand=0;!nativeWeaponArms && hand<2;++hand) {',
      '    // Native equip/holster changes weapon state, never controller ownership.\n    for(int hand=0;hand<2;++hand) {')
 # Do not blend away from exact tracked wrists at weapon state boundaries.
 start=s.index('    if(CurrentMotionWeapon()==1 && !grip) {\n        // Main body owns the blend;')
 end=s.index('    // Validate every correction',start)
 s=s[:start]+'    g_pistolAnimation.Reset(); // Tracked arms stay continuous through draw/holster.\n'+s[end:]
 # Keep the configured body position in the stable eye/body frame, not in
 # a settling animation/follower frame. Y posture and leg animation survive.
 heading='g_headingBase' if index==0 else 'g_heading.base'
 helper='''motiongun::Vec FixedBodyPlacement(uint8_t* item,motiongun::Vec pelvis,motiongun::Vec camera) {
    if(!g_active || !g_scenePoseValid || DrawingNativeShadow() ||
       !Cfg().firstPersonMovementStabilization || !g_groundEye.valid || !CanFitStandingBody(item)) return {};
    const float heading=g_bodyFollower.valid ? g_bodyFollower.yaw :
        locomotion::Radians(reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot);
    const auto anchor=locomotion::Rotate(g_groundEye.bodyLocal,heading);
    locomotion::Vec view{};VR().FirstPersonViewOffset(view.x,view.z);
    view=locomotion::Rotate(view,HEADING)*LiveWorldUnitsPerMetre();
    // Cancel horizontal animation/root settling before the user's offset and
    // hand IK. The stable eye carries intentional travel, turns and roomscale.
    return {g_scenePose.x_pos+view.x-anchor.x-camera.x-pelvis.x,0,
            g_scenePose.z_pos+view.z-anchor.z-camera.z-pelvis.z};
}

'''.replace('HEADING',heading)
 marker='bool PrepareJumpArm(' if index==0 else 'void TraceArmGlance('
 at=s.index(marker);s=s[:at]+helper+s[at:]
 sub('    const auto meshOffset=BodyMeshOffset(item);\n    for(auto& joint:posed) joint.origin=Add(joint.origin,{meshOffset.x,0,meshOffset.z});',
     '    const auto meshOffset=BodyMeshOffset(item);\n    const auto placement=Add(FixedBodyPlacement(item,posed[0].origin,cameraOffset),{meshOffset.x,0,meshOffset.z});\n    for(auto& joint:posed) joint.origin=Add(joint.origin,placement);')
 # The arm-only renderer still draws torso/legs; fix its placement too.
 sub('    const auto meshOffset=shadow ? locomotion::Vec{} : BodyMeshOffset(item);\n    const bool shifted=meshOffset.x!=0 || meshOffset.z!=0;',
     '    auto meshOffset=shadow ? locomotion::Vec{} : BodyMeshOffset(item);')
 marker='    motiongun::Frame correction[6]{};'
 pos=s.index(marker,s.index('bool ApplyArmIK('))
 addition='''    motiongun::Frame pelvis{},head{};
    if(!shadow && ReadIKJoint(obj,joints,count,bones,mapping,poses,0,pelvis) &&
       ReadIKJoint(obj,joints,count,bones,mapping,poses,14,head)) {
        const auto placement=FixedBodyPlacement(item,pelvis.origin,BodyPaletteCameraOrigin(item,head));
        meshOffset=meshOffset+locomotion::Vec{placement.x,placement.z};
    }
    const bool shifted=meshOffset.x!=0 || meshOffset.z!=0;
'''
 s=s[:pos]+addition+s[pos:]
 p.write_text(s,encoding='utf-8',newline='\r\n')
