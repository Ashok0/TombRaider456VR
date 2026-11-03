from pathlib import Path
import hashlib, json, re

source=Path('C:/dev/TombRaider456VR')
dest=Path('C:/dev/TombRaider123VR')
stage=source/'build/index-diagnostics-20261102/123-staged'
records=[]
def read(path): return path.read_text(encoding='utf-8-sig')
def stage_file(name,text):
    original=dest/name
    record={'path':name,'before':hashlib.sha256(original.read_bytes()).hexdigest() if original.exists() else None}
    target=stage/name;target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(text,encoding='utf-8',newline='\r\n')
    records.append(record)
def replace(text,old,new):
    assert text.count(old)==1,repr(old)
    return text.replace(old,new)

for name in ['src/IndexInput.h','src/ControllerProbe.h','src/ControllerProbe.inl','tools/index_input_test.cpp','tools/index_diagnostics_test.inl']:
    stage_file(name,read(source/name))

text=read(dest/'src/VRSystem.h')
text=replace(text,'#include "IndexInput.h"','#include "IndexInput.h"\n#include "ControllerProbe.h"')
text=replace(text,'    void ReadControllers(HandState out[2]) const;','    void ReadControllers(HandState out[2]) const;\n    void PollControllerProbe();')
block=read(source/'src/VRSystem.h').split('    mutable IndexInput m_indexInput;')[1].split('    mutable vr::EVRInputError')[0]
text=replace(text,'    mutable IndexInput m_indexInput;','    mutable IndexInput m_indexInput;'+block.rstrip('\n'))
stage_file('src/VRSystem.h',text)

text=read(dest/'src/VRSystem.cpp')
text=replace(text,'VRSystem& VR() { return g_vr; }','VRSystem& VR() { return g_vr; }\n\n#include "ControllerProbe.inl"')
start='    // Register before the first PollNextEvent/UpdateActionState, as OpenVR requires.'
end='    // Latch the per-eye constants.'
a=text.index(start);b=text.index(end,a)
origin=read(source/'src/VRSystem.cpp')
text=text[:a]+origin[origin.index(start):origin.index(end)]+text[b:]
text=replace(text,'    m_input=nullptr;m_indexInput.Reset();m_indexReadError=vr::VRInputError_None;',
    '    m_input=nullptr;m_indexInput.Reset();m_indexReadError=vr::VRInputError_None;\n    m_controllerProbe={};')
text=replace(text,'LogF("vr: Index action polling error=%d (0=recovered)",int(error));',
    'LogF("vr: Index action polling error=%s(%d) (0=recovered)",InputErrorName(error),int(error));')
stage_file('src/VRSystem.cpp',text)

text=read(dest/'src/Gamepad.cpp')
text=replace(text,'uint32_t           g_packet    = 0;',
    'uint32_t           g_packet    = 0;\nbool g_probeCapture=false;\nuint64_t g_probePolls=0;\nXGamepad g_probePad{};\nuint16_t g_probeButtons=0;')
text=replace(text,'    *state = mine;',
    '    *state = mine;\n    if(g_probeCapture) {\n        ++g_probePolls;g_probePad=mine.Gamepad;g_probeButtons|=mine.Gamepad.wButtons;\n    }')
origin=read(source/'src/Gamepad.cpp')
block=origin[origin.index('void GamepadControllerProbe('):origin.index('bool WorldInventoryActive()')]
text=replace(text,'bool WorldInventoryActive() {',block+'bool WorldInventoryActive() {')
stage_file('src/Gamepad.cpp',text)

text=read(dest/'src/Gamepad.h')
text=replace(text,'void GamepadUpdate();','void GamepadUpdate();\n// Read-only F7 capture of the final pad state actually returned to the game.\nvoid GamepadControllerProbe(bool begin,bool end);')
stage_file('src/Gamepad.h',text)
text=replace(read(dest/'src/Hooks.cpp'),'    GamepadUpdate();','    GamepadUpdate();\n    VR().PollControllerProbe();')
stage_file('src/Hooks.cpp',text)
text=read(dest/'TombRaiderVR.vcxproj')
text=replace(text,'    <ClInclude Include="src\\IndexInput.h" />',
    '    <ClInclude Include="src\\IndexInput.h" />\n    <ClInclude Include="src\\ControllerProbe.h" />\n    <ClInclude Include="src\\ControllerProbe.inl" />')
stage_file('TombRaiderVR.vcxproj',text)
origin=read(source/'README.md')
block=origin[origin.index('### Controller troubleshooting with F7'):origin.index('\n\n## Installation',origin.index('### Controller troubleshooting with F7'))]
text=read(dest/'README.md')
text=replace(text,'### Physical crouch port from TR4/5 (2026-10-30)',block+'\n\n### Physical crouch port from TR4/5 (2026-10-30)')
stage_file('README.md',text)
(stage.parent/'123-source-manifest.json').write_text(json.dumps(records,indent=2))
print(f'Staged {len(records)} files for TR1-3.')
