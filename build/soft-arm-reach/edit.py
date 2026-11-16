from pathlib import Path
import shutil,json,hashlib
r=Path.cwd();w=r/'build/soft-arm-reach';stage=w/'123-work';repo=Path('C:/dev/TombRaider123VR')
def read(p):return p.read_bytes().decode('cp1252').replace('\r\n','\n')
def write(p,s):p.write_bytes(s.replace('\n','\r\n').encode('cp1252'))
def edit(p,a,b):
 s=read(p);assert a in s,(p,a[:90]);write(p,s.replace(a,b))
for d in ['src','tools']:
 shutil.copytree(r/d,w/'before'/d,dirs_exist_ok=True)
for n in ['README.md','TombRaiderVR.ini']:shutil.copy2(r/n,w/'before'/n)
for d in ['src','tests','tools','third_party','bindings']:
 if (repo/d).exists():shutil.copytree(repo/d,stage/d,dirs_exist_ok=True)
for p in repo.iterdir():
 if p.is_file() and (p.suffix in ['.vcxproj','.filters','.props','.targets','.sln'] or p.name in ['README.md','TombRaiderVR.ini']):shutil.copy2(p,stage/p.name)
orig={p.relative_to(repo).as_posix():hashlib.sha256(p.read_bytes()).hexdigest().upper() for p in stage.rglob('*') if p.is_file() for p in [repo/p.relative_to(stage)]}
(w/'original123.json').write_text(json.dumps(orig,indent=2))
p=r/'src/FirstPersonIK.h';s=read(p);start=s.index('// Keep the shoulder fixed and both rigid bone lengths unchanged.')
s=s[:start]+'''// Manual arm-length fit is independent of body height. No pose history or
// automatic growing: repeated eyes/draws must never increase the reach envelope.
inline float ConnectedArmScale(float percent) {
    return std::isfinite(percent) ? std::clamp(percent,80.f,120.f)*.01f : 1.f;
}
// Exact tracking through 104% of fitted reach, then a C1 soft limit asymptotic
// to 112%. Preserve controller orientation and hand/weapon size throughout.
inline bool ConnectedArmTarget(const Frame (&chain)[3],const Frame& tracked,Frame& target,float percent=100) {
    const auto length=[](Vec v) {return std::sqrt(Dot(v,v));};
    const float scale=ConnectedArmScale(percent);
    const float upper=length(Sub(chain[1].origin,chain[0].origin))*scale;
    const float lower=length(Sub(chain[2].origin,chain[1].origin))*scale;
    const Vec offset=Sub(tracked.origin,chain[0].origin);
    const float distance=length(offset);
    Frame inverse{};
    if(!std::isfinite(upper+lower+distance) || upper<.001f || lower<.001f || !Inverse(tracked,inverse)) return false;
    Vec direction{};
    if(!Unit(offset,direction) && !Unit(Sub(chain[2].origin,chain[0].origin),direction)) direction={0,0,1};
    const float margin=std::min(upper,lower)*.001f;
    const float natural=upper+lower-margin,begin=natural*1.04f,band=natural*.08f;
    float reach=std::max(distance,std::fabs(upper-lower)+margin);
    if(distance>begin) reach=begin-band*std::expm1(-(distance-begin)/band);
    target=tracked;
    if(reach!=distance) target.origin=Add(chain[0].origin,Scale(direction,reach));
    return true;
}
inline bool SolveConnectedTrackedArm(const Frame (&native)[3],const Frame& tracked,
                                    Vec pole,const Frame (&bind)[3],Frame* corrections,
                                    bool upperArmPresent=true,ArmRollContinuity* continuity=nullptr,float percent=100) {
    Frame target{};
    if(!ConnectedArmTarget(native,tracked,target,percent)) return false;
    const auto length=[](Vec v) {return std::sqrt(Dot(v,v));};
    const float scale=ConnectedArmScale(percent);
    const float a=length(Sub(native[1].origin,native[0].origin))*scale;
    const float b=length(Sub(native[2].origin,native[1].origin))*scale;
    const float radius=length(Sub(target.origin,native[0].origin));
    const float natural=a+b-std::min(a,b)*.001f;
    // Grow gradually over the last 10% of ordinary reach. At full extension
    // there is still a small elbow bend, avoiding an abrupt straight-arm lock.
    const float x=std::max(0.f,radius/natural-.9f);
    const float extension=x<.2f ? x*x/.4f : x-.1f;
    const float extra=natural*extension;
    // The rig has no clavicles. Keep its socket fixed and distribute give into
    // the upper arm (60%) and forearm (40%) instead of translating the chest.
    // Use proportional weights for unusual rigs to bound each limb to +15%.
    const float share=std::clamp(.6f,1.f-1.25f*b/(a+b),1.25f*a/(a+b));
    const float lengths[2]={a+extra*share,b+extra*(1-share)};
    Frame reference[3]={native[0],native[1],native[2]},fit[3]{};
    for(int i=0;i<2;++i) {
        Vec axis{};const Vec bone=Sub(native[i+1].origin,native[i].origin);
        if(!Unit(bone,axis)) return false;
        const float ratio=lengths[i]/length(bone),v[3]={axis.x,axis.y,axis.z};
        auto stretch=IdentityBasis();
        for(int row=0;row<3;++row) for(int col=0;col<3;++col)
            stretch.r[row][col]+=(ratio-1)*v[row]*v[col];
        reference[i].basis=Multiply(stretch,native[i].basis);
        reference[i+1].origin=Add(reference[i].origin,Scale(axis,lengths[i]));
    }
    for(int i=0;i<3;++i) {
        Frame inverse{};if(!Inverse(native[i],inverse)) return false;
        fit[i]=Multiply(reference[i],inverse);
    }
    if(!SolveTrackedArm(reference,target,pole,bind,corrections,upperArmPresent,continuity)) return false;
    const Vec residual=Sub(native[0].origin,Multiply(corrections[0],reference[0]).origin);
    for(int i=0;i<3;++i) {
        corrections[i]=Multiply(corrections[i],fit[i]);
        corrections[i].origin=Add(corrections[i].origin,residual);
    }
    return true;
}
} // namespace tr::firstperson
'''
write(p,s);shutil.copy2(p,stage/'src/FirstPersonIK.h')
for base in [r,stage]:
 p=base/'src/FirstPerson.cpp'
 edit(p,'float heading,units,bodyOffset;','float heading,units,bodyOffset,armLength;')
 edit(p,'reach.bodyOffset=Cfg().firstPersonBodyForwardCm;','reach.bodyOffset=Cfg().firstPersonBodyForwardCm;reach.armLength=Cfg().firstPersonArmLengthPercent;')
 edit(p,'reach.bodyOffset!=Cfg().firstPersonBodyForwardCm) return;','reach.bodyOffset!=Cfg().firstPersonBodyForwardCm || reach.armLength!=Cfg().firstPersonArmLengthPercent) return;')
 edit(p,'ConnectedArmTarget(chain,wrist,limited)','ConnectedArmTarget(chain,wrist,limited,Cfg().firstPersonArmLengthPercent)')
 edit(p,'const bool animatedSidearms=SidearmAnimationActive();\n    if(animatedSidearms)', 'Frame reachReference[2][3]{};\n    const bool animatedSidearms=SidearmAnimationActive();\n    if(animatedSidearms)')
 edit(p,'auto roll=(g_pistolBodyScope && SidearmWeapon())', 'for(int i=0;i<3;++i) reachReference[hand][i]=chain[i];\n        auto roll=(g_pistolBodyScope && SidearmWeapon())')
 edit(p,'? &roll : nullptr)) return false;', '? &roll : nullptr,Cfg().firstPersonArmLengthPercent)) return false;')
 edit(p,'PublishConnectedWeaponReach(item,hand,arm,cameraOffset);','PublishConnectedWeaponReach(item,hand,reachReference[hand],cameraOffset);')
 edit(p,'motiongun::Vec reachCamera[2]{};','motiongun::Vec reachCamera[2]{};\n    motiongun::Frame reachReference[2][3]{};')
 edit(p,'reachCamera[hand]=Sub(trackedOrigin,target.origin);','reachCamera[hand]=Sub(trackedOrigin,target.origin);\n        for(int i=0;i<3;++i) reachReference[hand][i]=chain[i];')
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
 p=base/'TombRaiderVR.ini';s=read(p);s+='\n; First-person arm fit, independent of body height. Live in VR Options.\n; 80..120 percent; default 100. Connected arms use bounded soft extension.\n[VR]\nFirstPersonArmLengthPercent=100\n';write(p,s)
print('Staged both collections and implemented bounded arm fit; tests next.')
