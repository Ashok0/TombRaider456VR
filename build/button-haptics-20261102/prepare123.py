from pathlib import Path
import hashlib
import json

repo = Path('C:/dev/TombRaider123VR')
out = Path(__file__).parent
changes = {
    'src/VRSystem.h': [
        ('    void GunShotHaptic(int hand);',
         '    void GunShotHaptic(int hand);\n'
         '    // Physical button acknowledgement: right hand, using the existing\n'
         '    // bounded 80 ms burst and Touch/Index haptic routing.\n'
         '    void ButtonPressHaptic() { GunShotHaptic(1); }'),
    ],
    'src/PhysicalButton.inl': [
        ('    if(activated) LogF("vr-button: touched push button item=%d; native control continues",index);',
         '    if(activated) {\n'
         '        VR().ButtonPressHaptic();\n'
         '        LogF("vr-button: touched push button item=%d; native control continues",index);\n'
         '    }'),
    ],
    'tests/button_touch_selftest.cpp': [
        ('        uint64_t now=GetTickCount64();auto tick=[&](float z) {auto d=Sub(motiongun::Transform(frame,{x,y,z}),initial);testControllerOffset[hand]={d.x/1000,d.y/1000,d.z/1000};AdvancePhysicalButton(item,nullptr,++now);};',
         '        uint64_t now=GetTickCount64();auto tick=[&](float z) {\n'
         '            auto d=Sub(motiongun::Transform(frame,{x,y,z}),initial);testControllerOffset[hand]={d.x/1000,d.y/1000,d.z/1000};\n'
         '            const int before=leverActivations,left=testHapticShots[0],right=testHapticShots[1];\n'
         '            AdvancePhysicalButton(item,nullptr,++now);\n'
         '            Check(testHapticShots[0]==left && testHapticShots[1]-right==leverActivations-before,\n'
         '                "each successful button press acknowledges exactly once on the right, regardless of touching hand");\n'
         '        };'),
        ('        tick(face.z);Check(!leverActivations,"initial hand overlap is not a press");tick(outside);tick(face.z);',
         '        tick(face.z);Check(!leverActivations,"initial hand overlap is not a press");\n'
         '        buttonReject=true;tick(outside);tick(face.z);buttonReject=false;\n'
         '        Check(!leverActivations,"native rejection gives neither activation nor haptic acknowledgement");\n'
         '        tick(outside);tick(face.z);'),
    ],
    'README.md': [
        ('object interactions retain their original controls.\n',
         'object interactions retain their original controls.\n\n'
         'Each successful physical push-button activation in TR2/3 gives a short\n'
         '80 ms vibration on the **right controller**, even when the left hand presses.\n'
         'Holding contact or a rejected activation does not repeat the feedback. It\n'
         'uses the existing Touch/Index haptics path automatically, with no new INI\n'
         'setting. TR1 has no supported push-button family, and this collection has\n'
         'no matching physical numeric-keypad path.\n'),
    ],
}
manifest = []
for relative, replacements in changes.items():
    original = (repo/relative).read_bytes()
    text = original.decode('utf-8').replace('\r\n', '\n')
    for old, new in replacements:
        assert text.count(old) == 1, (relative, old)
        text = text.replace(old, new)
    staged = out/'123-staged'/relative
    staged.parent.mkdir(parents=True, exist_ok=True)
    staged.write_bytes(text.replace('\n', '\r\n').encode('utf-8'))
    backup = out/'123-before'/relative
    backup.parent.mkdir(parents=True, exist_ok=True)
    backup.write_bytes(original)
    manifest.append({'source':str(staged),'destination':str(repo/relative),
                     'before':hashlib.sha256(original).hexdigest(),
                     'after':hashlib.sha256(staged.read_bytes()).hexdigest()})
(out/'source123.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print('Prepared four TR1-3 files; original hashes recorded.')
