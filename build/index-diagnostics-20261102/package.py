from pathlib import Path
import hashlib,json,zipfile

root=Path(__file__).resolve().parent
instructions='''Index controller diagnostic update — TR{collection}

This is an UPDATE for an existing mod installation, not a complete first install.

1. Close the game. Back up its existing TombRaiderVR.dll.
2. Extract this archive into the matching game's installation folder, replacing
   TombRaiderVR.dll and the two shipped JSON files in TombRaiderVRBindings.
   Keep your existing INI and other mod files. Do not mix the two collections' DLLs.
3. In the title menu, choose Options > VR Options > Controller Type > Valve Index.
4. Fully exit and restart the game. This setting only takes effect after restart.
5. With the desktop game window focused, press F7 WITHOUT Ctrl, Shift or Alt.
   No popup appears. This starts a 20-second controller capture in TombRaiderVR.log.
6. Move both sticks in all directions, click both sticks, press A and B on each
   hand, pull both triggers and squeeze both grips. Hold each briefly, separately.
   The title menu works; a save is not needed. Normal game inputs remain active.
7. Wait 20 seconds, or press F7 again to stop early. Close the game and send the
   complete TombRaiderVR.log from the game folder BEFORE launching again.
   Every launch replaces the log. It should contain input-probe: BEGIN and END.

If you see no BEGIN line, confirm this DLL is installed, the game window has
keyboard focus, and your keyboard sends F7 (some laptops require Fn+F7).

The log distinguishes saved versus running Index mode, missing binding files,
failed SteamVR initialization, controller detection/roles/tracking, unbound or
inactive actions, button/axis data, and the final gamepad input sent to the game.
Raw legacy states can be unavailable in action mode; that alone is not a failure.

Existing checks to try:
- SteamVR should detect both controllers and accept their input in its own UI.
- Both actions.json and bindings_index.json must be inside TombRaiderVRBindings
  next to TombRaiderVR.dll, not just somewhere in the extracted download.
- If using a custom SteamVR binding, try the shipped Tomb Raider Index binding.
- Confirm you restarted after selecting Index. The menu's saved value can differ
  from the input mode of an already-running game.

This build adds diagnostics; it does not claim to fix an unconfirmed Index fault.
It does not change bindings during capture, inject inputs or test vibrations.
Ctrl+F7 and Ctrl+Shift+F7 retain weapon-calibration functions.
Physical Index hardware validation is still needed from the tester.
'''
manifest=[]
for collection in ('123','456'):
    output=root/collection/'bin'
    note=root/f'TESTER-TR{collection}.txt'
    note.write_text(instructions.format(collection=collection),encoding='utf-8')
    files=[(output/'TombRaiderVR.dll','TombRaiderVR.dll'),
        (output/'TombRaiderVRBindings/actions.json','TombRaiderVRBindings/actions.json'),
        (output/'TombRaiderVRBindings/bindings_index.json','TombRaiderVRBindings/bindings_index.json'),
        (note,'INDEX-DIAGNOSTICS.txt')]
    archive=root/f'TombRaider{collection}VR-Index-Diagnostics.zip'
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for path,name in files:
            z.write(path,name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert len(z.namelist())==4
    manifest.append({'collection':collection,'archive':str(archive),
        'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
        'dll_sha256':hashlib.sha256((output/'TombRaiderVR.dll').read_bytes()).hexdigest(),
        'focused_checks':413})
(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps(manifest,indent=2))
