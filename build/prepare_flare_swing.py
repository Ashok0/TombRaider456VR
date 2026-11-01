from pathlib import Path
import hashlib,json,shutil
r=Path(r'C:\dev\TombRaider456VR');old=r.parent/'TombRaider123VR';stage=r/'build/flare-swing123-port'
files=['src/FirstPerson.cpp','src/FirstPerson.h','src/Gamepad.cpp','src/FlareThrowMotion.h','src/PhysicalFlareThrow.inl','tests/flare_throw_test.cpp','tests/flare_input_test.cpp','README.md']
hashes={}
for name in files:
    hashes[name]=hashlib.sha256((old/name).read_bytes()).hexdigest().upper()
    target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(old/name,target)
(stage/'original-hashes.json').write_text(json.dumps(hashes,indent=2))
def edit(file,a,b):
    text=file.read_text(encoding='utf-8');assert text.count(a)==1,(file,a);file.write_text(text.replace(a,b),encoding='utf-8')
edit(stage/'src/FirstPerson.cpp','bool FirstPersonFlareGrip(bool down,bool allowed) {return PhysicalFlareGrip(down,allowed);}','void FirstPersonFlareMotion(bool allowed) {PhysicalFlareMotion(allowed);}')
edit(stage/'src/FirstPerson.h','bool FirstPersonFlareGrip(bool down,bool allowed);','void FirstPersonFlareMotion(bool allowed);')
p=stage/'src/Gamepad.cpp';s=p.read_text(encoding='utf-8');start=s.index('    const bool flareGrip=');end=s.index('    const unsigned holsterGrips=',start);s=s[:start]+s[end:]
s=s.replace('L.valid&&!flareGrip,R.valid','L.valid,R.valid').replace('!flareGrip && !gripAction','!gripAction')
mark='    uint16_t dpad = 0;';assert s.count(mark)==1
s=s.replace(mark,'''    FirstPersonFlareMotion(gameplay && L.valid && !holsterGrips && !switchGrip && !gripAction &&
        !L.btnUpper && !L.btnLower && !photoChord && L.trigger<=.12f && R.trigger<=.12f);
'''+mark);p.write_text(s,encoding='utf-8')
for base in [r,stage]:
    p=base/'README.md';s=p.read_text(encoding='utf-8');i=s.rindex('### Physical flare throwing')
    s=s[:i]+'''### Physical flare throwing (TR2-5)

In first person with Remastered graphics and tracked flare hands, light/equip a
flare normally, then **swing the left controller outward to throw it**. No button
press or release is needed. Left grip keeps its normal crouch binding. Direction
and speed come from your hand motion; normal flare burn time, lighting and native
collision behavior remain intact. Enabled automatically; no new INI setting.
TR1 has no flares.

The gesture requires a deliberate, sustained swing after the hand settles.
Small movements, a backwards windup behind the head, and moving the hand and HMD
together while crouching or walking do not trigger a throw. Tracking loss,
recentring, menu/equip/Action chords and view/level changes cancel the gesture.
Third person and Classic graphics retain their normal flare controls.
''';p.write_text(s,encoding='utf-8')
