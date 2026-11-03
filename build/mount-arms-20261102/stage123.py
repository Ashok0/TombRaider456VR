from pathlib import Path
import hashlib, json

root = Path(r'C:\dev\TombRaider123VR')
stage = Path(__file__).parent
records = []

def edit(relative, replacements):
    original = (root / relative).read_bytes()
    text = original.decode('utf-8').replace('\r\n', '\n')
    for old, new in replacements:
        assert text.count(old) == 1, (relative, old, text.count(old))
        text = text.replace(old, new)
    newline = '\r\n' if b'\r\n' in original else '\n'
    result = text.replace('\n', newline).encode('utf-8')
    for folder, data in [('before', original), ('staged', result)]:
        path = stage / folder / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    records.append({'path': relative, 'before': hashlib.sha256(original).hexdigest()})

edit('src/FirstPersonVisibility.h', [(
    'inline bool TraversalTransitionHandsOnly(int state,int game,uint32_t recoveryMask) {',
    '''inline bool TraversalTransitionHandsOnly(int state,int game,uint32_t recoveryMask) {
    // Reveal arms on the first upright frame, including immediate movement.
    // Torso/camera/wrist recovery can continue without hiding the arms.
    if(locomotion::IsGroundLocomotionState(state,game)) return false;''')])

edit('README.md', [(
    'Mount/recovery draws show only hands with opaque black wrist caps. The mount-to-standing camera blend defaults to 160 ms; original arm-recovery timing is retained.',
    'Mount draws show only hands with opaque black wrist caps. Arms return on the first standing or ground-movement frame, without waiting for recovery timers. The mount-to-standing camera blend defaults to 160 ms; original torso, arm and wrist recovery timing is retained.')])

edit('tests/remaining_parity_selftest.h', [(
    'Check(jumpDrawBits==HandMeshBits,"first standing draw retains hands without flashing the recovering torso");',
    'Check(jumpDrawBits==ArmMeshBits,"first standing draw restores arms without flashing the recovering torso");')])

edit('tests/transition_hands_selftest.cpp', [(
    'int transitionDraws=0;', 'int transitionDraws=0;\nuint32_t observedBits=0;'), (
    '    Detour_GetJoints(static_cast<uint8_t*>(target));',
    '    observedBits|=Field<uint32_t>(off::item_mesh_bits);\n    Detour_GetJoints(static_cast<uint8_t*>(target));'), (
    '"native third person and shadows never inherit transition caps"',
    '"standing, third person and shadows never inherit transition caps"'), (
    '''        Field<int16_t>(off::item_anim_state)=2;g_torsoPose.recovering=true;
        Check(FirstPersonBodyMask(item)==firstperson::HandMeshBits,"post-climb recovery continues hiding arms");
        expectTransition=true;transitionDraws=0;Field<uint32_t>(off::item_mesh_bits)=0x7fff;Detour_DrawCreatureHD(item,0);
        Check(transitionDraws==2,"standing handoff retains sealed hands");''',
    '''        // A queued standing goal must not reveal arms during the actual mount.
        Field<int16_t>(off::item_goal_state)=2;
        Check(FirstPersonBodyMask(item)==firstperson::HandMeshBits,"queued standing goal keeps the active mount hands-only");
        g_torsoPose.recovering=true;
        for(int state:{2,0,1,5,6,7,16,20,21,22,73}) {
            if(state==73 && game!=3) continue;
            Field<int16_t>(off::item_anim_state)=int16_t(state);
            Check(FirstPersonBodyMask(item)==firstperson::ArmMeshBits,"first upright frame restores arms during ongoing torso recovery");
            for(int eye:{0,1}) for(uint32_t bits:{0x7fffu,0x2400u,0u}) {
                (void)eye;expectTransition=false;transitionDraws=0;observedBits=0;
                Field<uint32_t>(off::item_mesh_bits)=bits;
                Detour_DrawCreatureHD(item,0);
                Check(observedBits==firstperson::ArmMeshBits && transitionDraws==0,"each standing eye draw immediately shows complete arms, even with a stale hand-only mask");
                Check(Field<uint32_t>(off::item_mesh_bits)==bits,"standing arm visibility remains draw-local");
            }
        }'''), (
    '"arms return when recovery ends"', '"torso returns when recovery ends"')])

(stage / '123-edits.json').write_text(json.dumps(records, indent=2))
print('Staged', len(records), 'TR1-3 files with original backups.')
