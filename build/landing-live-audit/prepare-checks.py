from pathlib import Path
import re
r=Path.cwd()
w=r/'build/landing-live-audit'
s=(r/'tools/first_person_tests.cpp').read_text()
assert s.count('float LiveWorldUnitsPerMetre() { return 1000.0f; }')==1
s=s.replace('float LiveWorldUnitsPerMetre() { return 1000.0f; }','float LiveWorldUnitsPerMetre() { return 423.0f; }')
s=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+(r/'tools'/m[1]).resolve().as_posix()+'"',s)
(w/'units423-tests.cpp').write_text(s)
s=(w/'focused.cpp').read_text().replace('../../tools/first_person_tests.cpp','units423-tests.cpp')
(w/'units423-focused.cpp').write_text(s)
s=(w/'focused.cmd').read_text().replace('focused.cpp','units423-focused.cpp').replace('focused.exe','units423-focused.exe')
(w/'units423-focused.cmd').write_text(s)
for p in [r/'README.md',w/'123-work/README.md']:
    s=p.read_text(encoding='utf-8')
    old='''Regression checks compare the final rendered eye with the neck transported by
the rendered torso, including a downward gaze and 0/12/15/30 cm of physical
lowering. This catches the previous build's camera/body mismatch; checking the
independent, camera-driven head joint alone cannot detect it. With drift logging
enabled, `landing-alignment` records the same eye/connected-neck separation.'''
    new='''Automatic body fitting also keeps its calibrated eye-to-collar clearance
through the impact. The camera and body solver use that same clearance; losing
it during landing could put the view too low even when a check reported that
the eye remained above the neck base. Manual body scale retains its existing
offset, and the reserve is not applied a second time as torso compression.

Regression checks compare the final rendered eye with the neck transported by
the rendered torso, requiring the calibrated clearance as well as testing a
downward gaze and 0/12/15/30 cm of physical lowering. Checking the independent,
camera-driven head joint alone cannot detect a body/view mismatch. With drift
logging enabled, `landing-alignment` records the eye/connected-neck separation
and calibration reserve once per tracked pose throughout the impact.'''
    assert s.count(old)==1,p
    s=s.replace(old,new).replace('solve. During that impact pose, the standing chest-height reserve and extra\nwalking gait no longer add a second crouch.','solve. During that impact pose, the calibration reserve belongs to the shared\neye reference; extra walking gait and inferred torso compression do not add a\nsecond crouch.')
    p.write_text(s,encoding='utf-8')
print('Prepared actual-scale checks and updated landing documentation.')
