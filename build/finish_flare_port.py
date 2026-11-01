from pathlib import Path
import shutil
root=Path(r'C:\dev\TombRaider456VR');stage=root/'build/flare123-port'
p=stage/'src/FirstPerson.cpp';s=p.read_text()
s=s.replace('    CommitPhysicalFlareThrow();\n    if (g_crouchWeaponScope) g_crouchWeaponScope->Finish();','    if (g_crouchWeaponScope) g_crouchWeaponScope->Finish();\n    CommitPhysicalFlareThrow();')
s=s.replace('VR().RecenterHead();','ResetPhysicalFlareThrow(); VR().RecenterHead();')
s=s.replace('void FirstPersonToggle() {','void FirstPersonToggle() {\n    ResetPhysicalFlareThrow();')
s=s.replace('void FirstPersonShutdown() {','bool FirstPersonFlareGrip(bool down,bool allowed) {return PhysicalFlareGrip(down,allowed);}\n\nvoid FirstPersonShutdown() {')
p.write_text(s)
p=stage/'src/FirstPerson.h';s=p.read_text().replace('#pragma once','#pragma once\n\nnamespace tr { bool FirstPersonFlareGrip(bool down,bool allowed); }');p.write_text(s)
p=stage/'src/Gamepad.cpp';s=p.read_text().replace('    const bool switchGrip = FirstPersonSwitchGrip(R.grip > 0.5f,','''    const bool flareGrip=FirstPersonFlareGrip(L.grip>.5f,
        gameplay && L.valid && !(holsterGrips&1) && !gripAction && R.grip<=.5f &&
        !L.btnUpper && !L.btnLower && !photoChord && L.trigger<=.12f && R.trigger<=.12f);
    const bool switchGrip = FirstPersonSwitchGrip(R.grip > 0.5f,''').replace('if (!gripAction && !(holsterGrips&1) && L.grip > 0.5f)','if (!flareGrip && !gripAction && !(holsterGrips&1) && L.grip > 0.5f)');p.write_text(s)
p=stage/'README.md';p.write_text(p.read_text()+root.joinpath('README.md').read_text().split('### Physical flare throwing (TR2–5)')[-1].join(['\n\n### Physical flare throwing (TR2–5)','']))
for name in ['FlareThrowMotion.h','FlareThrowAddresses.inl','PhysicalFlareThrow.inl']:
    shutil.copyfile(root/'src'/name,stage/'src'/name)
p=root/'src/FirstPerson.cpp';s=p.read_text();s=s.replace('VR().RecenterFirstPersonHead();','ResetPhysicalFlareThrow(); VR().RecenterFirstPersonHead();');p.write_text(s)
