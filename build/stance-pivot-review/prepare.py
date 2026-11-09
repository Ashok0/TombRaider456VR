from pathlib import Path
p=Path(__file__).parent
s=(p/'before.h').read_text().replace('#include "BodyFollower.h"','#include "BodyFollower.h"\n#include "PlantedPivotEvidence.h"')
s=s.replace('PhysicalStepIntent stepIntent;','PhysicalStepIntent stepIntent;\n    PlantedPivotEvidence pivotEvidence;')
s=s.replace('heading.Update(eye,left,right,bodyYaw,headYaw,now,head);\n        rotating=heading.turning;', '''const bool plantedPivot=pivotEvidence.Update(eye,left,right,headForward,now) && !walking && !heading.turning && !translating;
        if(plantedPivot) { heading={};stepIntent.Capture(walkEye,left,right,now);translating=false; }
        heading.Update(eye,left,right,bodyYaw,headYaw,now,head);
        rotating=heading.turning;''')
s=s.replace('const bool translationCoherent=h>0','const bool translationCoherent=!plantedPivot && h>0')
(p/'src/ControllerBodyAnchor.h').write_text(s)
for name in ['probe.cmd','test-new.cmd','test-candidates.cmd']:
 s=(Path('build/hands-out-capture')/name).read_text().replace('hands-out-capture\\src','stance-pivot-review\\src').replace('/Febuild\\hands-out-capture','/Febuild\\stance-pivot-review').replace('/Fobuild\\hands-out-capture','/Fobuild\\stance-pivot-review')
 for exe in ['probe.exe','new-recorded.exe','curve.exe','pitch.exe','extended.exe','turn.exe']:s=s.replace('build\\hands-out-capture\\'+exe,'build\\stance-pivot-review\\'+exe)
 (p/name).write_text(s)
