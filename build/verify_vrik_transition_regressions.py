"""Temporarily remove one fix, require its reproduction to fail, then restore.
Run with the game closed. Production files are restored even on test failure.
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
cases = [
    ('armed-turn-camera', 'src/LocomotionMath.h',
     'return IsHardLanding(state,animation) ||',
     'return IsHardLanding(state,animation) || (state==7 && animation==13) ||',
     'armed idle-left-fast-right turn sequence cannot switch away from the stable tracked eye'),
    ('standing-contact-refresh', 'src/BodyFollower.h',
     'if(hold && contacts && contactsValid &&',
     'if(false && hold && contacts && contactsValid &&',
     'settled native feet cannot remain latched in the air'),
    ('holster-grace', 'src/PhysicalHolster.h',
     'ReachMetres=.30f;', 'ReachMetres=.22f;',
     'expanded draw zone advertises hover readiness'),
    ('crouch-hands', 'src/FirstPerson.cpp',
     'return IsCrouchState(item) ? firstperson::HandMeshBits : mask;',
     'return mask;',
     'armed attachments retain exact gun hands'),
    ('transition-draw-mask', 'src/FirstPerson.cpp',
     'const uint32_t bodyMask=lara ? FirstPersonBodyMask(item) : UINT32_MAX;',
     'const uint32_t bodyMask=UINT32_MAX;',
     'armed attachments retain exact gun hands'),
    ('visibility-recovery', 'src/FirstPersonVisibility.h',
     'until=now+.35;', 'until=now;',
     'same-sample and first standing palettes'),
    ('physical-yaw-delay', 'src/BodyFollower.h',
     'delta-std::clamp(delta,-s.yawSlack,s.yawSlack);',
     '(delta-std::clamp(delta,-s.yawSlack,s.yawSlack))*(1-std::exp(-10*dt));',
     'physical yaw outside slack is resolved in the same sample'),
    ('clearance-cutoff', 'src/FirstPersonClearance.h',
     'dx=int64_t(std::lround(double(dx)*512.0/distance));',
     'eye[0]=body[0];eye[2]=body[2];return;\n        dx=int64_t(std::lround(double(dx)*512.0/distance));',
     'head movement across sweep reach saturates continuously'),
    ('wall-state', 'src/FirstPerson.cpp',
     'if(CanFitStandingBody(item) || CanSteerCrouch(item) || FullBodyGripState(item)) return true;',
     'if(CanTurnBody(item) || CanSteerCrouch(item) || FullBodyGripState(item)) return true;',
     'wall impact retains full-body rendering'),
    ('mount-busy', 'src/FirstPerson.cpp',
     'const bool nativeGrip=status==1 && FullBodyGripState(item);',
     'const bool nativeGrip=false;',
     'native hands-busy mount keeps the full-body solver active'),
    ('wall-camera', 'src/FirstPersonClearance.h',
     'refine<5', 'refine<0',
     'head movement across sweep reach saturates continuously'),
    ('mount-same-sample', 'src/BodyFollower.h',
     'serial!=frame || grip!=lastGrip || (!grip && mountPending)',
     'serial!=frame',
     'uploaded mount-to-standing pelvis and chest cannot pop'),
    ('knee-cycling', 'src/FirstPerson.cpp',
     ['const float upright=standing ? firstperson::StandingPostureWeight(VR().FirstPersonPostureDrop()) : 0;',
      'grounded,g_bodyFollower.phase,standing ? 0.f : g_bodyFollower.stepWeight,posed,upright)',
      'const bool quiet=(state==2 || state==12) && !UseLandingCamera(item);'],
     ['const float upright=standing ? firstperson::StandingPostureWeight(VR().FirstPersonPostureDrop())*(1-g_bodyFollower.stepWeight) : 0;',
      'grounded,g_bodyFollower.phase,g_bodyFollower.stepWeight,posed,upright)',
      'const bool quiet=state==2 && !moving && !g_bodyFollower.turning && !g_bodyFollower.stepping && g_bodyFollower.stepWeight<.01f && !UseLandingCamera(item);'],
     'visual roomscale following cannot switch standing legs into synthetic crouching'),
    ('tracking-frame', 'src/FirstPerson.cpp',
     'locomotion::Rotate(g_groundEye.bodyLocal,bodyYaw)',
     'locomotion::Rotate({g_groundEye.local.x,g_groundEye.local.z},bodyYaw)',
     'calibrated body offset uses body coordinates after tracking yaw recenter'),
]
for name, relative, fixed, broken, expected in cases:
    if len(sys.argv)>1 and name not in sys.argv[1:]:
        continue
    path = root / relative
    original = path.read_bytes()
    text = original.decode('utf-8')
    replacements = zip(fixed, broken) if isinstance(fixed, list) else [(fixed, broken)]
    for before, after in replacements:
        assert text.count(before) == 1, (name, 'ambiguous replacement')
        text = text.replace(before, after)
    try:
        path.write_bytes(text.encode('utf-8'))
        result = subprocess.run(['cmd', '/c', 'build\\run_ik_debug_tests.cmd'], cwd=root,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (root / 'build' / ('vrik-negative-' + name + '.log')).write_text(result.stdout, encoding='utf-8')
        assert result.returncode != 0 and expected in result.stdout, (name, result.stdout)
        print('Confirmed regression reproduction:', name, flush=True)
    finally:
        path.write_bytes(original)
