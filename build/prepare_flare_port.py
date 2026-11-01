from pathlib import Path
import hashlib,json,shutil
root=Path(r'C:\dev\TombRaider456VR'); old=root.parent/'TombRaider123VR'; stage=root/'build/flare123-port'
files=['src/FirstPerson.cpp','src/FirstPerson.h','src/Gamepad.cpp','README.md']
hashes={}
for file in files:
    p=old/file; hashes[file]=hashlib.sha256(p.read_bytes()).hexdigest().upper()
    dest=stage/file;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest)
(stage/'original-hashes.json').write_text(json.dumps(hashes,indent=2))

def replace(path,a,b):
    s=path.read_text(encoding='utf-8');assert s.count(a)==1,(str(path),a,s.count(a));path.write_text(s.replace(a,b),encoding='utf-8')
for target,newer in [(root,True),(stage,False)]:
    p=target/'src/FirstPerson.cpp'
    replace(p,'#include "MotionGunMath.h"','#include "MotionGunMath.h"\n#include "FlareThrowMotion.h"')
    if newer:
        replace(p,'void __cdecl Detour_CrouchGun() {','void CommitPhysicalFlareThrow();\n\nvoid __cdecl Detour_CrouchGun() {')
        replace(p,'    auto original=g_hCrouchGun.Original<void (__cdecl*)()>();','    CommitPhysicalFlareThrow();\n    auto original=g_hCrouchGun.Original<void (__cdecl*)()>();')
        replace(p,'bool FullBodyIKReady() {','int FlareLevel() {return AppState(drva::app_off::level);}\nfloat FlareHeading() {return g_headingBase;}\nuint32_t FlareLOS() {return g_motionDll ? g_motionDll->lineOfSight : 0;}\n#include "PhysicalFlareThrow.inl"\n\nbool FullBodyIKReady() {')
        replace(p,'    if(FlareJointPoint(item,point,joint,false)) return;','    if(PhysicalFlareSpawnPoint(item,point ? &point->x : nullptr,joint)) return;\n    if(FlareJointPoint(item,point,joint,false)) return;')
        replace(p,'    InstallPhysicalSwitches(d,base);','    InstallPhysicalFlareThrow(d.timestamp,g_flareHooksReady && g_hGunJoint.installed() && g_hCrouchGun.installed());\n    InstallPhysicalSwitches(d,base);')
    else:
        replace(p,'bool UnarmedIKReadyForPass(bool shadow) {','int FlareLevel() {return AppFlag(drva::app_off::level);}\nfloat FlareHeading() {return g_heading.base;}\nuint32_t FlareLOS() {return g_boundDll ? g_boundDll->los : 0;}\n#include "PhysicalFlareThrow.inl"\n\nbool UnarmedIKReadyForPass(bool shadow) {')
        replace(p,'void __cdecl Detour_ShotgunEffectJoint(uint8_t* item,int32_t* point,int32_t joint) {','void __cdecl Detour_ShotgunEffectJoint(uint8_t* item,int32_t* point,int32_t joint) {\n    if(PhysicalFlareSpawnPoint(item,point,joint)) return;')
        replace(p,'void __cdecl Detour_LaraGun() {','void __cdecl Detour_LaraGun() {\n    CommitPhysicalFlareThrow();')
        replace(p,'    InstallPhysicalSwitches(d,base);','    InstallPhysicalFlareThrow(d.timestamp,d.module[4]!=L\'1\' && g_hShotgunEffectJoint.installed() && g_hLaraGun.installed());\n    InstallPhysicalSwitches(d,base);')
    replace(p,'void Remove() {','void Remove() {\n    ResetPhysicalFlareThrow();g_flareThrowDll=nullptr;')
    replace(p,'void FirstPersonRecenter() {','void FirstPersonRecenter() {\n    ResetPhysicalFlareThrow();')
    replace(p,'void FirstPersonToggle() {','void FirstPersonToggle() {\n    ResetPhysicalFlareThrow();')
    replace(p,'void FirstPersonShutdown() {','bool FirstPersonFlareGrip(bool down,bool allowed) {return PhysicalFlareGrip(down,allowed);}\n\nvoid FirstPersonShutdown() {')
    p=target/'src/FirstPerson.h'
    replace(p,'#pragma once','#pragma once\n\nnamespace tr {\n// Left grip prepares a held flare; release uses physical hand velocity.\nbool FirstPersonFlareGrip(bool down,bool allowed);\n}')
    p=target/'src/Gamepad.cpp'; action='touchGripAction' if newer else 'gripAction'
    anchor='    const bool switchGrip = FirstPersonSwitchGrip(R.grip > 0.5f,'
    add=f'''    const bool flareGrip=FirstPersonFlareGrip(L.grip>.5f,
        gameplay && L.valid && !(holsterGrips&1) && !{action} && R.grip<=.5f &&
        !L.btnUpper && !L.btnLower && !(L.stickClick && R.stickClick) &&
        L.trigger<=.12f && R.trigger<=.12f);
'''
    replace(p,anchor,add+anchor)
    if newer: replace(p,'if (leftGrip && !touchGripAction','if (leftGrip && !flareGrip && !touchGripAction')
    else: replace(p,'if (!gripAction && !(holsterGrips&1) && L.grip > 0.5f)','if (!flareGrip && !gripAction && !(holsterGrips&1) && L.grip > 0.5f)')
    p=target/'README.md'
    with p.open('a',encoding='utf-8') as f:f.write('''

### Physical flare throwing (TR2–5)

In first person with Remastered graphics and tracked flare hands, light/equip a
flare normally. Squeeze the **left grip**, move your left hand, and release the
grip to throw it from that hand. Direction and speed come from your physical
controller motion; a gentle release drops it. The normal flare lifetime,
lighting, inventory and collision behavior remain native. Enabled automatically
with tracked flare hands; no new INI setting is required. TR1 has no flares.

Both grips still take priority as Action. Menu/equip chords, tracking loss,
recentring, switching view/graphics, or changing levels cancel a prepared throw.
A hand obstructed by a wall cannot release a flare through it. Third person and
Classic graphics retain the normal flare controls.
''')
for name in ['FlareThrowMotion.h','FlareThrowAddresses.inl','PhysicalFlareThrow.inl']:
    shutil.copyfile(root/'src'/name,stage/'src'/name)
