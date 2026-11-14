from pathlib import Path
w=Path('build/landing-head-follow');(w/'candidate').mkdir(exist_ok=True)
for name in ['ControllerBodyAnchor.h','PlantedPivotEvidence.h']:(w/'candidate'/name).write_text((Path('src')/name).read_text())
p=w/'candidate/PlantedPivotEvidence.h';s=p.read_text();s=s.replace('const bool inspecting=forward.y<-.65f && left.y>eye.y-.5f && right.y>eye.y-.5f;', '''const bool closeInspection=forward.y<-.5f && left.y>eye.y-.12f && right.y>eye.y-.12f;
        const bool inspecting=(forward.y<-.65f && left.y>eye.y-.5f && right.y>eye.y-.5f) || closeInspection;''');s=s.replace('behind>.28f && behind<.85f', 'behind>(closeInspection ? .06f : .28f) && behind<.85f');p.write_text(s)
(w/'replay.cmd').write_text('''@echo off
call "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\\landing-head-follow\\candidate /Isrc build\\stance-pivot-review\\replay-loop.cpp /Febuild\\landing-head-follow\\replay.exe /Fobuild\\landing-head-follow\\replay.obj
if errorlevel 1 exit /b 1
''')
