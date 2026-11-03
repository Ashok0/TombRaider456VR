from pathlib import Path
import hashlib,json
root=Path('C:/dev/TombRaider456VR');dest=Path('C:/dev/TombRaider123VR')
out=root/'build/hybrid-controls-20261102';stage=out/'123-staged';records=[]
def read(path): return path.read_text(encoding='utf-8-sig')
def replace(text,old,new):
    assert text.count(old)==1,repr(old)
    return text.replace(old,new)
def save(name,text):
    path=dest/name;target=stage/name;target.parent.mkdir(parents=True,exist_ok=True)
    records.append({'path':name,'before':hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None})
    target.write_text(text,encoding='utf-8',newline='\r\n')
text=read(dest/'src/Config.h')
text=replace(text,'    bool  valveIndexControllers = false;',
    '    // Hybrid Modern/Tank Controls: opt-in third-person Walk + back hop.\n    // Off preserves native controls, including Modern camera-relative walking.\n    bool hybridModernTankControls = false;\n    bool  valveIndexControllers = false;')
save('src/Config.h',text)
text=read(dest/'src/Config.cpp')
marker='    g_cfg.valveIndexControllers = GetBool('
assert text.count(marker)==1
text=text.replace(marker,'    g_cfg.hybridModernTankControls = GetBool(L"HybridModernTankControls",false,ini);\n    LogF("config: Hybrid Modern/Tank Controls=%s (third-person Walk + back hop)",\n        g_cfg.hybridModernTankControls ? "ON" : "OFF");\n'+marker)
save('src/Config.cpp',text)
text=replace(read(dest/'src/FirstPerson.cpp'),'g_thirdPersonBackHopPressed=walkPressed &&',
    'g_thirdPersonBackHopPressed=Cfg().hybridModernTankControls && walkPressed &&')
save('src/FirstPerson.cpp',text)
text=read(dest/'src/ThirdPersonBackHop.inl')
text=replace(text,'// RG + LS down is a third-person-only, Lara-relative native back-hop.',
    '// Opt-in Hybrid Modern/Tank Controls: RG + LS down requests a third-person back-hop.')
text=replace(text,'if(!g_thirdPersonBackHopPressed ||','if(!Cfg().hybridModernTankControls || !g_thirdPersonBackHopPressed ||')
save('src/ThirdPersonBackHop.inl',text)
ini=read(root/'TombRaiderVR.ini');block=ini[ini.index('; Hybrid Modern/Tank Controls:'):ini.index('HybridModernTankControls=0')+len('HybridModernTankControls=0')]
text=replace(read(dest/'TombRaiderVR.ini'),'GamepadEnabled=1','GamepadEnabled=1\n\n'+block)
save('TombRaiderVR.ini',text)
text=read(dest/'tests/back_hop_test.cpp')
text=replace(text,'    testConfig.firstPersonMoveWithHead=true;testConfig.gamepadEnabled=true;',
    '    testConfig.firstPersonMoveWithHead=true;testConfig.gamepadEnabled=true;\n    testConfig.hybridModernTankControls=true;')
tests='''    Check(!Config{}.hybridModernTankControls,"Hybrid Modern/Tank Controls defaults Off");
    for(int game:{1,2,3}) for(bool modern:{false,true}) for(bool armed:{false,true})
    for(bool smooth:{false,true}) for(float head:{0.f,.7f}) for(int clip:{103,40,12,13,44,69}) {
        HopSetup(game,modern,armed,smooth,head,clip);
        testConfig.hybridModernTankControls=false;captureOnly=true;
        float x=.1f,z=-1,turn=.3f;
        FirstPersonInput(x,z,turn,false,false,true);
        Check(!g_thirdPersonBackHopPressed && x==.1f && z==-1 && turn==.3f,
            "disabled hybrid preserves third-person stick input without queuing a hop");
        auto* axes=Ptr<int16_t>(dll.analogInput);axes[0]=1234;axes[1]=-23456;
        *Ptr<uint32_t>(dll.input)=Back|Walk;
        const int16_t originalAxes[4]={axes[0],axes[1],axes[2],axes[3]};
        const int originalState=Field<int16_t>(18),originalAnim=Field<int16_t>(24);
        Detour_LaraAboveWater(item,nullptr);
        Check(seenHopInput==(Back|Walk) && !std::memcmp(seenHopAxes,originalAxes,sizeof(originalAxes)),
            "disabled hybrid forwards native Walk/direction bits and camera-relative analog axes");
        Check(Field<int16_t>(18)==originalState && Field<int16_t>(24)==originalAnim,
            "disabled hybrid does not force a standing or hop animation");
        g_thirdPersonBackHopPressed=true;
        { ThirdPersonBackHopScope stale(item);
          Check(*Ptr<uint32_t>(dll.input)==(Back|Walk) && !std::memcmp(axes,originalAxes,sizeof(originalAxes)),
              "disabled hybrid also rejects a previously queued hop at simulation time"); }
    }
'''
text=replace(text,'uint32_t seenHopInput=0;','uint32_t seenHopInput=0;\nint16_t seenHopAxes[4]{};')
text=replace(text,'    seenHopInput=*Ptr<uint32_t>(dll.input);','    seenHopInput=*Ptr<uint32_t>(dll.input);\n    std::memcpy(seenHopAxes,Ptr<int16_t>(dll.analogInput),sizeof(seenHopAxes));')
text=replace(text,'int main() {','int main() {\n'+tests)
text=replace(text,'        for(bool grip:{false,true}) {','        for(bool hybrid:{false,true}) for(bool grip:{false,true}) {')
text=replace(text,'            g_active=g_runtimeEnabled=g_haveHeading=true;g_headingItem=item;',
    '            testConfig.hybridModernTankControls=hybrid;\n            g_active=g_runtimeEnabled=g_haveHeading=true;g_headingItem=item;')
save('tests/back_hop_test.cpp',text)
save('tools/hybrid_controls_config_test.cpp',read(root/'tools/hybrid_controls_config_test.cpp'))
text=read(dest/'README.md')
old=next(line for line in text.splitlines() if line.startswith('| Third-person backward hop |'))
text=replace(text,old,'| Hybrid Modern/Tank Controls | **Off by default.** `HybridModernTankControls=0` leaves Walk + stick down to native movement, walking towards the camera with Modern Controls. Set it to `1` for the **third-person-only** RG + LS down hop in TR1-3. First-person movement and crawling are unchanged. See [backward hop](#third-person-backward-hop). |')
text=replace(text,'| Backward hop | RG + LS down, **third person only**, without pressing Jump. |',
    '| Backward hop (optional) | Enable `HybridModernTankControls=1`, then RG + LS down, **third person only**, without Jump. |')
row='| `HybridModernTankControls` | `0` | Hybrid Modern/Tank Controls: Off uses native walking; On enables the third-person Walk + back hop. |\n'
text=replace(text,'| `FirstPersonMountSmoothingSeconds` |',row+'| `FirstPersonMountSmoothingSeconds` |')
text=replace(text,'effect. The hop, object interactions, flare gesture, wrist caps and Classic body',
    'effect. Object interactions, flare gesture, wrist caps and Classic body')
marker='### Third-person backward hop\n\n'
block='''**Hybrid Modern/Tank Controls is Off by default.** Under `[VR]` in
`TombRaiderVR.ini`, `HybridModernTankControls=0` leaves Walk + stick down to the
original game. With Modern Controls selected, Lara walks towards the camera.
The mod does not change the third-person control style selected in the game.
Set `HybridModernTankControls=1` and restart to enable the shortcut below.

'''
text=replace(text,marker,marker+block)
save('README.md',text)
(out/'123-source-manifest.json').write_text(json.dumps(records,indent=2))
print(f'Staged {len(records)} files; regenerate DefaultIni.h after applying.')
