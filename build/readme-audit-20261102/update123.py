from pathlib import Path
root=Path('C:/dev/TombRaider123VR')
text=(root/'README.md').read_text(encoding='utf-8-sig')
def replace(old,new):
    global text
    assert text.count(old)==1,(old[:100],text.count(old))
    text=text.replace(old,new)

overview='''## Recent updates

Current behavior reviewed through **2026-11-02**, covering the past week's changes
(October 26–November 2). This summary takes precedence over older development
snapshots below. The game starts in third person; **Y + LT** toggles first person.

| Area | Current behavior |
|---|---|
| Third-person backward hop | **RG + LS down**, without Jump, requests a short native hop behind Lara in TR1, TR2 and TR3, armed or unarmed. The shortcut is **third-person only**; first-person backward walking and vent crawling retain their original behavior. See [backward hop](#third-person-backward-hop). |
| Jump parity | The TR4/5 takeoff, soft-landing and camera-interruption fixes are ported: no preparatory knee dip/foot shuffle, immediate ordinary landing recovery, stable sideways cameras and correct armed backward/sideways directions. Hard falls, clearance and airborne physics remain native. |
| Physical-turn and roomscale corrections | Neutral jumps stay neutral after physical turning; forward jumps keep the selected direction. The body stays centered after roomscale jumps, physical steps can leave ledges, and TR1's unused vehicle field no longer disables the jump fixes. |
| Classic graphics | Physical turning and roomscale body centering now use the Classic render origin instead of stale Remastered body-follow state. **Y + RT** toggles graphics in the title ring as well as gameplay. |
| Physical crouching | Enabled by default. In TR3, the legs bend through a partial squat before a 55 cm headset drop requests native crouch; rising within 23 cm of standing releases it subject to ceiling clearance. In TR1/2 the same posture hides/restores the body but does not add native crawling. |
| Crouch and standing responsiveness | TR3 keeps headset-relative crawl steering and tracked hands through queued crouch transitions. A leftover DUCK queue after standing no longer delays ordinary gait changes. TR1/2 keep their own state rules. |
| Climbing and body recovery | Ledge and TR3 monkey-bar arms preserve native grip contacts with stable elbows. Mount/recovery draws show only hands with opaque black wrist caps. The mount-to-standing camera blend defaults to 160 ms; original arm-recovery timing is retained. |
| Native VR Options | **Options > VR Options** is inside the existing title-ring Options page, not a new ring object or popup. Controller Type and Color Scheme display their selected values. Dark is the default; Brighter applies immediately. |
| Valve Index | Optional SteamVR action bindings separate face buttons from grip squeeze. `ValveIndexControllers=0` remains the default; choosing Index requires restarting and the shipped bindings folder. |
| Physical switches | Ordinary TR1–3 wall levers support direct RG grabs without an Action icon, calibrated palm attachment and retained partial travel. TR2/3 add touch push buttons, small wall levers and rotating airlock wheels. See coverage below. |
| Physical flare throwing | In TR2/3 Remastered first person, swing the left controller to throw a lit flare. No release button is required; left grip retains crouch. TR1 has no flares. |
| Other parity fixes | Full-body IK, holsters and feedback, pistol/flare tracking, spatial inventory, controller polling, water-exit handling, ordinary-landing stabilization, costume cuffs and controller-arm shadows remain included. See the dated port notes for their individual scopes. |

### Recent controls

These use Meta Touch labels: **RG/LG** are right/left grip, **LT/RT** are the
triggers, and **LS** is the left stick. See the [Index mapping](#valve-index-controllers)
for its equivalent face buttons.

| Action | Input / scope |
|---|---|
| Toggle first person | Y + LT during gameplay; launches still start in third person. |
| Toggle Classic/Remastered | Y + RT in gameplay or the title ring. |
| Backward hop | RG + LS down, **third person only**, without pressing Jump. |
| First-person backward walking | LS down; holding RG retains the existing first-person behavior. |
| Physical crouch | Lower the headset after calibrating upright; rise to release. TR3 crouches natively; TR1/2 only hide the body. |
| Physical lever/airlock wheel | Hold RG on the handle/rim and move the right controller; release/regrab to continue. |
| Physical push button, TR2/3 | Touch the face with either controller, then withdraw before pressing again. |
| Throw lit flare, TR2/3 | Swing the left controller in Remastered first person; no grip release required. |
| Native Action / block pushing | Y or both grips. Native Action remains available for all interactions. |
| VR Options | Open the original Options item in the title ring, scroll to VR Options, then use Up/Down and Left/Right or Select. |

### Current interaction coverage

Physical interactions below require first person and holstered weapons. The
Action icon need not be visible. Native Y/Action remains available; third-person
object interactions retain their original controls.

| Mechanism | Games | Physical input |
|---|---|---|
| Ordinary wall levers | TR1, TR2, TR3 | Right Grip near the visible handle, then move along its arc. Partial pulls survive release, movement and regrabbing without snapping back. See [first-person lever controls](#first-person). |
| Push buttons | TR2, TR3, including supported expansion meshes | Touch the visible face with either controller. Both idle faces of reversible buttons are supported. See [push buttons](#touching-push-buttons-in-vr-tr23). |
| Small wall switches | TR2, TR3 | Grip the handle and move through the authored travel in either direction. See [small switches and airlocks](#physical-small-switches-and-airlock-wheels-tr23). |
| Airlock wheels | TR2, TR3 | Grip the rim and rotate around the axle. Regripping retains partial rotation; TR2 one-way and TR3 reversible native rules remain. See [airlock wheels](#physical-small-switches-and-airlock-wheels-tr23). |

These are supported object families, not every decorative switch or custom-level
asset. Underwater switches and unsupported special mechanisms keep native use.
Recenter, mode/level changes, native Action and explicit resets can clear partial
previews; these are not new persistent savegame positions.

The TR4 train floor/rail lever has no matching native mechanism in this collection.
This port does not add TR4/5-specific cog-wheel, pulley, crowbar-socket, searchable
drawer/cabinet, monitor or numeric-keypad handlers to TR1–3. The TR4/5 mirror fix
is also not installed here: the reflection audit found no confirmed matching
missing-mirror case in the TR1–3 assets it inspected. That is not a guarantee
about every decorative reflective surface.

### Current defaults for the recent features

| INI setting under `[VR]` | Default | Purpose |
|---|---|---|
| `FirstPersonPhysicalCrouch` | `1` | TR3 native crouch; TR1/2 physical posture-driven body hiding. Requires full-body IK and positional tracking. |
| `FirstPersonMountSmoothingSeconds` | `0.16` | First-person mount-to-standing camera blend; `0` disables it. |
| `ValveIndexControllers` | `0` | Legacy Touch-style input; `1` selects the Index-oriented action path after restart. |
| `ValveIndexGripPress` / `ValveIndexGripRelease` | `0.55` / `0.35` | Squeeze thresholds in Index mode. |
| `VRBrighterColors` | `0` | Dark presentation; `1` selects Brighter. Also available in VR Options. |

Defaults apply to new INIs and missing keys; existing explicit values remain in
effect. The hop, object interactions, flare gesture, wrist caps and Classic body
centering add no separate INI toggles. Older port records describe their original
snapshots; use this README and the current shipped INI for current defaults.
The detailed notes retain historical test counts. Automated checks do not replace
a full headset playthrough or physical Index hardware validation.

### Classic graphics and roomscale

Classic first-person Lara follows live headset turning instead of reusing a stale
Remastered body-follow pose. Its draw origin also receives the rendered-body
offset, keeping the visible mesh centered after walking around the play space.
The correction applies to Lara's rendering; collision movement, other objects and
native shadows retain their own transforms. It does not add the Remastered
full-body IK solver to the Classic mesh. Switching graphics does not change the
Dark/Brighter color setting in VR Options.

'''
replace('### Jump direction after physical turns (2026-10-31)',overview+'### Jump direction after physical turns (2026-10-31)')
replace('Use Up/Down to select a row, Left/Right or Select to change it, and Back to return.',
        'Use Up/Down to select a row, Left/Right or Select to change it, and Back to return.\nBoth rows display their current selected value on the native page.')
replace('wall collision for physical leaning is opt-in. See the port record for controls, settings and validation.',
        'wall collision for physical leaning is opt-in. The port record describes the\noriginal implementation; the newer crouch defaults/thresholds and hands-only\nclimbing behavior are documented in this README.')
replace('''controllers. Index mode supplies an Index profile only. While the game is running,
SteamVR's controller binding editor can customize its named actions; choose the''',
        '''controllers. The setting selects an input path, not a hardware lock: Touch may
still work with `ValveIndexControllers=1` if SteamVR supplies compatible action
bindings. The shipped manifest supplies an Index profile, not automatic controller
detection. While the game is running, SteamVR's controller binding editor can
customize its named actions; choose the''')
replace('''The two Y/trigger
chords are recognized only in gameplay; title, inventory and cutscene input is
passed through unchanged.''',
        '''The Y + LT
view-toggle chord is recognized during gameplay. The graphics chord also
works in the title ring; it does not require first-person mode. Inventory and
cutscene controls retain their own native handling.''')
replace('''again. Away from a lever, Right Grip still means Walk. Both-grip Action, menus,''',
        '''again. In first person, Right Grip still means Walk away from a lever.
The [third-person backward-hop shortcut](#third-person-backward-hop) is separate.
Both-grip Action, menus,''')
replace('### Physical flare throwing (TR2-5)','### Physical flare throwing (TR2/3)')
replace('''TR1 has no flares.

The gesture accepts''','''TR1 has no flares. The companion TR4–6 mod supplies the same gesture in TR4/5.

The gesture accepts''')
replace('''### Third-person backward hop

In TR1–3 third person only, right grip + LS down requests Lara's native backward
hop relative to her facing. First-person movement and vent crawling retain their
existing controls. Jump and interaction inputs take priority; native floor and
wall collision remain in control.
''','''### Third-person backward hop

In TR1, TR2 and TR3 **third person only**, hold **Right Grip (RG) + LS down**
to request Lara's short native backward hop. No Jump button is needed. It goes
behind Lara's current facing instead of turning her toward the stick or headset,
with guns drawn or holstered. Native clearance, animation, gravity and collision
still decide the outcome; the input/camera override lasts only for the native tick.

First-person movement is restored to its pre-shortcut behavior: LS back remains
continuous backward walking, including with RG held. Vent crawling is unchanged.
Jump, Action, roll/drop, shifted D-pad input, menus, vehicles and scripted control
take priority. A selected first-person view temporarily suspended by a native
camera does not enable the shortcut. The earlier D-pad ledge-hang proposal was
reverted and is not included.

The corrected port passed focused third-person-hop/first-person-preservation
checks and the existing ground-movement regressions. The original first-person
simulation, ground-gait and directional-jump paths were compared against their
pre-shortcut versions. In-headset confirmation of the corrected shortcut is
still needed.
''')
out=Path('C:/dev/TombRaider456VR/build/readme-audit-20261102/README123.md')
out.write_text(text,encoding='utf-8',newline='\r\n')
print('Prepared',out)
