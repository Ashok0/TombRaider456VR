from pathlib import Path

stage = Path(__file__).parent
source = Path('C:/dev/TombRaider456VR/src')
def read(name):
    return (stage / name).read_text(encoding='utf-8-sig')
def write(name, text):
    (stage / name).write_bytes(text.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8'))
def replace(text, old, new):
    assert text.count(old) == 1, (old[:100], text.count(old))
    return text.replace(old, new)

ref = (source / 'BodyFollower.h').read_text()
ref = ref[ref.index('// Side/back gaits'):ref.index('// Filter only inferred body motion')]
write('src/BodyFollower.h', replace(read('src/BodyFollower.h'), '// Filter only inferred body motion', ref + '// Filter only inferred body motion'))

text = read('src/FirstPerson.cpp')
text = replace(text, 'firstperson::TorsoPoseFilter g_torsoPose;', 'firstperson::TorsoPoseFilter g_torsoPose;\nfirstperson::GroundGaitReference g_groundGait;\nint g_groundGaitObject=-1;')
text = replace(text, '    g_torsoPose.Reset();\n    g_bodyLanding=false;', '    g_torsoPose.Reset();\n    g_groundGait.Reset();g_groundGaitObject=-1;\n    g_bodyLanding=false;')
text = text.replace('g_bodyFollower.Reset();g_torsoPose.Reset();g_bodyLanding=false;g_pistolAnimation.Reset();', 'g_bodyFollower.Reset();g_torsoPose.Reset();g_groundGait.Reset();g_bodyLanding=false;g_pistolAnimation.Reset();')
a = text.index('bool ApplyFullBodyIK(')
b = text.index('    const int anchorJoint=', a)
insert = (source / 'FirstPerson.cpp').read_text()
insert = insert[insert.index('    const auto& current=', insert.index('bool ApplyFullBodyIK(')):insert.index('    const int anchorJoint=', insert.index('bool ApplyFullBodyIK('))]
insert = insert.replace('Radians(', 'locomotion::Radians(').replace('Wrap(', 'locomotion::Wrap(')
head = text[:b]
tail = text[b:]
tail = replace(tail, '    const Vec cameraOffset=BodyPaletteCameraOrigin(item,native[14]);\n', '')
start = tail.index('    const auto& current=')
end = tail.index('    const bool grip=', start)
tail = tail[:start] + tail[end:]
start = tail.index('    const Vec simulationRoot{')
end = tail.index('    // Collision retraction', start)
tail = tail[:start] + tail[end:]
tail = replace(tail, '            const bool mainBody=g_bodySkinScope &&\n                (*reinterpret_cast<const uint32_t*>(item+off::item_mesh_bits)&0x49)==0x49;\n', '')
text = head + insert + tail
text = replace(text, '    const int32_t offsetX = static_cast<int32_t>(std::lround(tracked.x));\n    const int32_t offsetZ = static_cast<int32_t>(std::lround(tracked.z));', '''    // Always protect the base camera; optional tracked clearance adds the
    // physical lean to this sweep instead of bypassing collision altogether.
    const bool collideTracking=Cfg().firstPersonWallCollision;
    const int32_t offsetX = collideTracking ? static_cast<int32_t>(std::lround(tracked.x)) : 0;
    const int32_t offsetZ = collideTracking ? static_cast<int32_t>(std::lround(tracked.z)) : 0;''')
text = replace(text, '    if (Cfg().firstPersonWallCollision && g_blockCamera.collision) {', '    if (g_blockCamera.collision) {')
text = replace(text, "        // their visible near face explicitly, including the HMD's final offset.", '        // their visible near face explicitly; include HMD lean only if enabled.')
text = replace(text, '''    if(Cfg().firstPersonWallCollision) ClampHeadToCollision(item, body, renderedHead, eyeY,capsuleHeight);
    else if(crouch || UseGroundRollCamera(item) || UseLandingCamera(item)) clampVertical(body[0],body[2]);
    if (crouch) clampVertical(renderedHead[0],renderedHead[2]);
    // The stereo layer adds tracking after this scene pose. Move the anchor by
    // the same amount in reverse so the actual eye centre stays on the clear
    // side of the wall, even when the player physically leans toward it.''', '''    if(!collideTracking && !crouch && (UseGroundRollCamera(item) || UseLandingCamera(item)))
        clampVertical(body[0],body[2]);
    ClampHeadToCollision(item, body, renderedHead, collideTracking ? eyeY : double(pose.y_pos),capsuleHeight);
    if (collideTracking && crouch) clampVertical(renderedHead[0],renderedHead[2]);
    // Undo only tracking included in the sweep; free physical leaning remains
    // entirely outside the base-camera correction applied by stereo later.''')
write('src/FirstPerson.cpp', text)

for name in ('TombRaiderVR.ini', 'src/DefaultIni.h'):
    t = read(name)
    at = t.index('FirstPersonWallCollision=0')
    # Keep existing template settings; clarify the option at its declaration.
    t = t[:at] + '; Base eye clearance always applies; 0 permits physical leaning through walls, 1 also clamps leaning.\n' + t[at:]
    write(name, t)
t = read('src/Config.h')
t = replace(t, '    bool firstPersonWallCollision = false;', '    // Base eye clearance always applies; this optionally clamps physical leaning too.\n    bool firstPersonWallCollision = false;')
write('src/Config.h', t)
