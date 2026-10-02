from pathlib import Path
p=Path('README.md'); s=p.read_text(encoding='utf-8')
s=s.replace('draw/holster, **tap LT** to fire the left pistol/Uzi on release, and **tap RT**\nto fire the right pistol/Uzi or selected single weapon. Single weapons aim with\nthe right controller. Holding RT does not auto-repeat.', 'draw/holster with LT alone, **tap LT** to fire the left pistol/Uzi on release,\nand **hold RT** to fire the right pistol/Uzi or selected single weapon at its\nnative rate. Hold LT+RT to sustain both dual guns; this cannot become a\nholster gesture until LT is released. Single weapons aim with the right controller.')
s=s.replace('| Trigger handling | Independent pistol/Uzi taps;', '| Trigger handling | Independent pistol/Uzi taps and sustained LT+RT;')
s=s.replace('Ready guns retain independent tap firing and tracking-loss protection;', 'Ready guns retain independent firing and resume held RT after tracking recovery;')
s=s.replace('aim assistance to the game\'s already-selected living enemy, within 8192 game\nunits and only when native LOS from that muzzle is clear. This includes\nvertical aim.', 'aim assistance to a living enemy selected independently for that controller,\nwithin 8192 game units and only when native LOS from that muzzle is clear.\nDirect hit spheres take priority; Lara\'s native target lock is not required.\nThis includes vertical aim. The HK scope retains unassisted barrel aiming.')
s=s.replace('Logs include the native\nselected target, assist status, sphere-hit branch and target HP before/after.', 'The first 100 tracked shots per hand log the\ncontroller-selected target, assist status and target HP before/after.')
s=s.replace('The installed Quest 3/Touch fit checked on 2026-09-26 is:', 'The saved Quest 3/Touch fit updated on 2026-10-02 is:')
s=s.replace('FirstPersonMotionGunRaiseMetres=-0.0254','FirstPersonMotionGunRaiseMetres=-0.06985')
s=s.replace('That is an 8-inch local grip-back calibration, 1 inch down and 30 degrees\ndownward pitch, with no lateral/yaw/roll adjustment.', 'That is an 8-inch local grip-back calibration, 2.75 inches below zero and\n30 degrees downward pitch, with no lateral/yaw/roll adjustment. The height is\n**3.75 inches below the original `0.0254 m` setting**: `0.0254 - 3.75 * 0.0254\n= -0.06985 m`, matching the final TR1–3 profile.')
s=s.replace('new defaults, and the all-weapon/cutscene deployments preserved them unchanged.', 'new defaults. This port updates only the installed hand-height key; the other\npersonal calibration values are preserved.')
s=s.replace('- RT press: fire the right pistol/Uzi or the selected single weapon.\n- Hold LT for 0.5 seconds: draw/holster once; release before toggling again.', '- Hold RT: sustain the right pistol/Uzi or selected single weapon at its native rate.\n- Hold LT+RT: sustain both dual guns; releasing RT first keeps LT in firing mode.\n- Hold LT alone for 0.5 seconds: draw/holster once; release before toggling again.')
s=s.replace('Each tap requests one native shot (one full volley for the shotgun), including\nwith Uzis and HK; holding RT does not\nauto-repeat. A short LT tap waits for release to distinguish it from the long\nequip gesture, which never also fires the left gun. Both hands can have one\nshot queued at once.', 'Each native firing operation consumes one request (one full volley for the\nshotgun). Held triggers renew that request while native animation owns cadence,\nammo and effects. A short LT tap waits for release to distinguish it from the\nlong equip gesture. A dual-fire hold cannot become a holster gesture until LT\nis released. Both hands can have one shot queued at once.')
s=s.replace('and TR6 keep their existing controls. Entering the mode with a trigger\nalready held requires release before the new controls activate.', 'and TR6 keep their existing controls. Held RT resumes when tracking/readiness\nreturns. An inherited LT alone must be released before it can start an equip\ngesture. During requested controller fire, `AnimatePistols` temporarily clears\nthe native target and restores it on return, avoiding the selected-target but\nlost-arm-lock stall reported during bat attacks.')
section='''### TR1–3 first-person fixes ported on 2026-10-02

These changes apply to **TR4/5**. TR6 has no equivalent first-person/controller
weapon path and keeps its native stereo camera behavior.

- Bullet weapons acquire living targets per controller muzzle without Lara's
  native lock. Direct hit spheres take priority, then the existing 12-degree
  assist; the HK scope stays unassisted. Projectiles keep their native rules.
- Pistol animation can continue firing after native arm lock is lost. LT+RT
  sustains dual fire without holstering, and held RT resumes after tracking loss.
- Fixed/scripted camera handoffs preserve standing-eye calibration and rebase
  it for scripted body turns. Different Lara/level identities invalidate it.
- The rendered torso is fitted beneath the final collision-adjusted camera at
  intermediate physical-turn angles, without changing Lara's collision root.
- Zero health immediately selects the native death camera and restores body
  visibility; loading a living Lara restores first-person eligibility.
- HD body draws retain hidden bone transforms and trim fragments by visible
  skin weight. Shared shoulder/torso vertices no longer collapse at ledges.
  The shader supports TR4/5's 33-slot palettes and clears its mask for later draws.
- Eye clearance permits looking over empty crate/ledge drops while still
  rejecting walls and ceilings. Body dragging retains native ledge safety;
  collision pushback cannot accumulate as false roomscale travel.
- Saved personal hand height is `-0.06985 m`, **3.75 inches below the original
  `0.0254 m` setting**. Other calibration values and factory defaults are retained.

Automated validation: 325,669 production first-person regression checks; the
self-test suite including two-minute dual fire, camera handoffs, physical turns
at five-degree intervals and repeated ledge pushback; and 3,226 OpenGL checks
against 480 compatible shader pairs from the installed `tomb456.exe`.
The native motion-gun audit passes on both PDB and retail TR4/5 DLLs, including
new `AnimatePistols`, world-space `GetSpheres` and active-item dependencies.
These are offline/hidden-context checks, not an in-headset gameplay validation.

Headset checks still required: shoot moving bats without a body lock, sustain
LT+RT beyond 0.5 seconds, interrupt/recover tracking with RT held, return from
fixed/scripted cameras, turn physically through partial and full circles,
die/reload, and repeat crate-edge hangs, releases and jump-offs. Check that
body centering recovers and no shoulder/torso seam stretches.

BUILD_INSTALL_RECORD

'''
s=s.replace('## Installation\n',section+'## Installation\n',1)
p.write_text(s,encoding='utf-8')

