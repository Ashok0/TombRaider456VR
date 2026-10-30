from pathlib import Path
root=Path(__file__).parent
p=root/'src/FirstPerson.cpp'
s=p.read_text()
def change(old,new):
    global s
    assert s.count(old)==1,(old[:90],s.count(old))
    s=s.replace(old,new)
change('#include "BodyFollower.h"','#include "BodyFollower.h"\n#include "JumpLanding.h"')
change('int g_groundGaitObject=-1;', 'int g_groundGaitObject=-1;\nfirstperson::JumpLanding g_jumpLanding;\nfirstperson::GroundGaitReference g_jumpBodyReference;\nint g_jumpBodyObject=-1;')
change('    g_groundGait.Reset();g_groundGaitObject=-1;', '    g_groundGait.Reset();g_groundGaitObject=-1;\n    g_jumpLanding.Reset();g_jumpBodyReference.Reset();g_jumpBodyObject=-1;')
change('bool UseLandingCamera(const uint8_t* item) {', '''bool OrdinaryJumpLanding(const uint8_t* item) {
    return g_active && Cfg().firstPersonMovementStabilization && g_groundEye.valid &&
        g_jumpLanding.owner==item && g_jumpLanding.level==AppFlag(drva::app_off::level) &&
        g_jumpLanding.ordinary && item && CanWalk(item) &&
        locomotion::IsLandingAnimation(*reinterpret_cast<const int16_t*>(item+off::item_anim_state),
                                     *reinterpret_cast<const int16_t*>(item+off::item_anim_number));
}
bool UseLandingCamera(const uint8_t* item) {''')
change('    return item && CanWalk(item) && locomotion::IsLandingAnimation(', '    return item && CanWalk(item) && !OrdinaryJumpLanding(item) && locomotion::IsLandingAnimation(')
# Keep movement/input gates using native landing identity, independent of presentation.
change('           g_shifted || g_jumpPressed || UseLandingCamera(item) ||', '''           g_shifted || g_jumpPressed || locomotion::IsLandingAnimation(
               *reinterpret_cast<const int16_t*>(item+off::item_anim_state),
               *reinterpret_cast<const int16_t*>(item+off::item_anim_number)) ||''')
change('    const int anchorJoint=Cfg().firstPersonJoint;', '''    const bool ordinaryLanding=OrdinaryJumpLanding(item);
    const Frame landingRoot{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},nativeHeading)),
                            Sub(simulationRoot,cameraOffset)};
    const bool nativeLanding=locomotion::IsLandingAnimation(state,
        *reinterpret_cast<const int16_t*>(item+off::item_anim_number));
    if(mainBody && !nativeLanding && CanFitStandingBody(item)) {
        if(g_jumpBodyObject!=object) g_jumpBodyReference.Reset();
        g_jumpBodyObject=object;
        if(!g_jumpBodyReference.Apply(native,landingRoot,false,true,true,{std::sin(nativeHeading),0,std::cos(nativeHeading)})) return false;
    } else if(ordinaryLanding && g_jumpBodyObject==object && g_jumpBodyReference.valid) {
        // Remove the landing torso squat before applying live head/hand IK.
        // Feet remain native contacts, and button equip arms keep their pose.
        if(!g_jumpBodyReference.Apply(native,landingRoot,true,false,MotionReady() || UnarmedIKReady(),
                                      {std::sin(nativeHeading),0,std::cos(nativeHeading)})) return false;
    }
    const int anchorJoint=Cfg().firstPersonJoint;''')
change('            const bool standing=(state==2 || state==12) && !UseLandingCamera(item);',
       '            const bool standing=ordinaryLanding || ((state==2 || state==12) && !UseLandingCamera(item));')
change('    const bool quiet=(state==2 || state==12) && !UseLandingCamera(item);',
       '    const bool quiet=ordinaryLanding || ((state==2 || state==12) && !UseLandingCamera(item));')
change('void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {', '''void ObserveJumpLanding(uint8_t* item) {
    const bool eligible=item && g_active && Cfg().firstPersonMovementStabilization && Gate() &&
        item==*Ptr<uint8_t*>(g_boundDll->laraItem) && LaraWaterStatus()==0 && !InteractionAligning() &&
        *reinterpret_cast<const int16_t*>(item+off::item_hit_points)>0;
    if(!eligible) { g_jumpLanding.Reset();return; }
    const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
    const int animation=*reinterpret_cast<const int16_t*>(item+off::item_anim_number);
    g_jumpLanding.Observe(item,AppFlag(drva::app_off::level),true,state,animation,
        reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_pos,
        (*reinterpret_cast<const uint16_t*>(item+off::item_flags)&8)!=0,CanWalk(item),
        locomotion::IsLandingAnimation(state,animation),(*Ptr<uint32_t>(g_boundDll->input)&0x10u)!=0);
}
void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {''')
change('    jumpInput.Prepare();\n    if (g_groundMoveAction)', '    jumpInput.Prepare();\n    ObserveJumpLanding(item);\n    if (g_groundMoveAction)')
change('    g_groundMoveAction = 0;\n    g_crouchDrive=false;', '    ObserveJumpLanding(item);\n    g_groundMoveAction = 0;\n    g_crouchDrive=false;')
p.write_bytes(s.replace('\n','\r\n').encode())
