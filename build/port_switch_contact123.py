from pathlib import Path
root=Path(__file__).resolve().parents[1]
stage=root/'build/switch-contact123'
source=(root/'src/PhysicalSwitch.inl').read_text()
p=stage/'src/PhysicalSwitch.inl'
s=p.read_text()
def replace(old,new):
    global s
    assert old in s,old
    s=s.replace(old,new)
replace('void RestoreSwitchPreview();','void RestoreSwitchPreview();\nvoid RestoreParkedSwitches();')
replace('    RestoreSwitchPreview();\n    g_switchCandidate={};','    RestoreSwitchPreview();\n    RestoreParkedSwitches();\n    g_switchCandidate={};')
start=s.index('bool SwitchHand(');end=s.index('\nuint8_t* SwitchItem',start)
palm=source[source.index('constexpr motiongun::Vec SwitchPalmMesh'):source.index('bool SwitchHand(')]
s=s[:start]+palm+'''bool SwitchHand(motiongun::Vec& world) {
    motiongun::Frame wrist{};motiongun::Basis controller{};motiongun::Vec palm{};
    if (!VR().poseValid() || !TrackedControllerWrist(1,wrist,controller,&palm)) return false;
    // Unattached calibrated palm: never feed the attached lever pose back into tracking.
    SwitchPalmLocal(palm);
    world=motiongun::Transform(wrist,palm);
    return true;
}
'''+s[end:]
replace('points[256]{},grip{},lastHand{};','points[256]{},grip{},lastHand{},handOffset{};')
parked=source[source.index('struct ParkedSwitch {'):source.index('using Fn_SwitchItem')]
replace('using Fn_SwitchItem',parked+'using Fn_SwitchItem')
parking=source[source.index('bool ParkedSwitchUnchanged'):source.index('// Build matrices on a private item')]
parking=parking.replace('uint32_t*>(item+0x1820)','uint16_t*>(item+off::item_flags)')
parking=parking.replace('item+6244','item+488').replace('.interpolation','.onScreen').replace('item+6248','item+off::item_joints_prev')
replace('// Build matrices on a private item',parking+'// Build matrices on a private item')
replace('    auto& p=g_switchPreview;\n    const int object=', '    if (ResumeSwitchPreview(item,hand,now)) return true;\n    auto& p=g_switchPreview;\n    const int object=')
replace('p.lastHand=hand;p.lastTime=now;\n    return true;', 'p.lastHand=hand;p.lastTime=now;\n    p.handOffset=Sub(p.grip,hand);\n    return true;')
start=s.index('void AttachPhysicalSwitchHand(');end=s.index('\nbool UpdateSwitchPreview',start)
attached=source[source.index('void AttachPhysicalSwitchHand('):source.index('\nbool UpdateSwitchPreview')]
s=s[:start]+attached+s[end:]
replace('    float nearest=1.e30f;int selected=p.selected;', '    const auto contact=Add(hand,p.handOffset);\n    float nearest=1.e30f;int selected=p.selected;')
replace('Sub(hand,p.points[i])','Sub(contact,p.points[i])')
replace('SwitchLength(Sub(hand,p.points[0]))','SwitchLength(Sub(contact,p.points[0]))')
replace('RestoreSwitchPreview();g_switchGripClaimed=false;return false;', 'ParkSwitchPreview();g_switchGripClaimed=false;return false;')
replace('    const int state=*reinterpret_cast<const int16_t*>(target+off::item_anim_state);',
        '    const int state=*reinterpret_cast<const int16_t*>(target+off::item_anim_state);\n    const int idleAnimation=g_boundDll->module[4]==L\'3\' ? 103 : 11;')
replace('lara+off::item_anim_number)!=11','lara+off::item_anim_number)!=idleAnimation')
replace('    auto original=g_hSwitchCollision.Original<Fn_SwitchCollision>();', '''    auto original=g_hSwitchCollision.Original<Fn_SwitchCollision>();
    if (g_switchDll && g_boundDll && GameDllBound()==g_boundDll && GameDllBase()==g_boundBase &&
        (*Ptr<uint32_t>(g_boundDll->input)&0x40)) PrepareParkedSwitchForNative(index);''')
replace('    if (!down || !allowed || !PhysicalSwitchReady()) {\n        RestoreSwitchPreview();',
        '    if (!down || !allowed || !PhysicalSwitchReady()) {\n        if (PhysicalSwitchReady()) ParkSwitchPreview();\n        else RestoreSwitchPreview();')
replace('        RestoreSwitchPreview();\n        g_switchCompleted=false;g_switchGripClaimed=false;',
        '        ParkSwitchPreview();\n        g_switchCompleted=false;g_switchGripClaimed=false;')
p.write_text(s)
p=stage/'src/FirstPerson.cpp';s=p.read_text()
replace('#include <cstring>','#include <cstring>\n#include <vector>')
replace('#include "PhysicalSwitch.inl"','''bool TrackedControllerWrist(int hand,motiongun::Frame& out,motiongun::Basis& controller,motiongun::Vec* gripLocal=nullptr);
bool ReadIKBind(const uint8_t* obj,int count,int bones,const int32_t* mapping,
                const float* poses,int joint,motiongun::Frame& bind);
#include "PhysicalSwitch.inl"''')
replace('bool BuildControllerWrist(int hand, motiongun::Frame& out, motiongun::Basis& controller) {',
        'bool TrackedControllerWrist(int hand, motiongun::Frame& out, motiongun::Basis& controller,motiongun::Vec* gripLocal) {')
replace('    AttachPhysicalSwitchHand(hand,out,physical);', '''    if (gripLocal) *gripLocal=motiongun::GripPointLocal(calibration.gripForwardMetres*scale,
        calibration.raiseMetres*scale,calibration.rightMetres*scale);''')
replace('bool FlareIKReady() {','''bool BuildControllerWrist(int hand, motiongun::Frame& out, motiongun::Basis& controller) {
    motiongun::Vec local{};
    if (!TrackedControllerWrist(hand,out,controller,&local)) return false;
    AttachPhysicalSwitchHand(hand,out,motiongun::Transform(out,local));
    return true;
}

bool FlareIKReady() {''')
p.write_text(s)
print('Staged TR1-3 switch retention, regrab offset, palm contact and TR3 standing eligibility.')
