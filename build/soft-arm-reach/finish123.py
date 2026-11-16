from pathlib import Path
import shutil,json,hashlib
r=Path.cwd();w=r/'build/soft-arm-reach';stage=w/'123-work';repo=Path('C:/dev/TombRaider123VR')
def read(p):return p.read_bytes().decode('cp1252').replace('\r\n','\n')
def write(p,s):p.write_bytes(s.replace('\n','\r\n').encode('cp1252'))
def edit(p,a,b):
 s=read(p);assert a in s,(p,a[:90]);write(p,s.replace(a,b))
for base in [stage]:
 p=base/'src/FirstPerson.cpp'
 edit(p,'reachCamera[hand]=motiongun::Sub(trackedOrigin,target.origin);','reachCamera[hand]=Sub(trackedOrigin,target.origin);\n        for(int i=0;i<3;++i) reachReference[hand][i]=chain[i];')
 edit(p,'PublishConnectedWeaponReach(item,hand,arm,reachCamera[hand]);','PublishConnectedWeaponReach(item,hand,reachReference[hand],reachCamera[hand]);')
 p=base/'src/Config.h';edit(p,'    float firstPersonBodyScale = 0;', '    float firstPersonArmLengthPercent = 100; // Manual arm fit, independent of body height (80..120).\n    float firstPersonBodyScale = 0;')
 p=base/'src/Config.cpp';edit(p,'    g_cfg.firstPersonBodyScale=GetFloat', '    const float armLength=GetFloat(L"FirstPersonArmLengthPercent",100,ini);\n    g_cfg.firstPersonArmLengthPercent=std::isfinite(armLength) ? std::round(std::clamp(armLength,80.f,120.f)) : 100;\n    g_cfg.firstPersonBodyScale=GetFloat')
 p=base/'src/VRMenuSettings.h';edit(p,'SettingCount=4','SettingCount=5')
 p=base/'src/VRMenuSettings.inl';edit(p,'firstPersonBodyForwardCm;}}','firstPersonBodyForwardCm;}},\n        {L"FirstPersonArmLengthPercent",L"Arm Length",80,120,1,false,[]{return Cfg().firstPersonArmLengthPercent;}}')
 edit(p,'    Log("VR Options saved;', '    if(values[4]!=original[4]) g_cfg.firstPersonArmLengthPercent=values[4];\n    Log("VR Options saved;')
 edit(p,'hybrid controls and body position','hybrid controls, body position and arm length')
 p=base/'src/VRMenuModel.h';edit(p,'float bodyForwardCm=0;','float bodyForwardCm=0,armLengthPercent=100;')
 edit(p,'float bodyForward=0)', 'float bodyForward=0,float armLength=100)')
 edit(p,'bodyForwardCm=bodyForward;', 'bodyForwardCm=bodyForward;armLengthPercent=armLength;')
 edit(p,'        if(selected!=3)', '        if(selected==4) return buttons&0x100000 ? 100.f : std::clamp(armLengthPercent+(buttons&0x40000000 ? -1.f : 1.f),80.f,120.f);\n        if(selected!=3)')
 edit(p,': "Body Position";', ': row==3 ? "Body Position" : "Arm Length";')
 edit(p,'        if(row==3) {','        if(row==4) { std::snprintf(bodyValue,sizeof(bodyValue),"%.0f%%",armLengthPercent);return bodyValue; }\n        if(row==3) {')
 edit(p,'        if(selected==3) return', '        if(selected==4) return tr6 ? "TR4/5 first-person arms only" : "Left/Right: 1%   Select: 100%";\n        if(selected==3) return')
 p=base/'src/VRNativeMenu.cpp';edit(p,'values[2]!=0,values[3]);','values[2]!=0,values[3],values[4]);');edit(p,'s.page.bodyForwardCm=values[3];','s.page.bodyForwardCm=values[3];s.page.armLengthPercent=values[4];')
 p=base/'TombRaiderVR.ini';s=read(p);s=s.replace('[VR]','[VR]\nFirstPersonArmLengthPercent=100',1);s+='\n; First-person arm fit, independent of body height. Live in VR Options.\n; 80..120 percent; default 100. Connected arms use bounded soft extension.\n';write(p,s)
print('Staged both collections and implemented bounded arm fit; tests next.')
