from pathlib import Path
p=Path('build/hands-out-capture/before.h').read_text()
p=p.replace('#include "BodyFollower.h"','#include "BodyFollower.h"\n#include "PlantedSwivel.h"')
p=p.replace('PhysicalStepIntent stepIntent;','PhysicalStepIntent stepIntent;\n    PlantedSwivel swivel;')
p=p.replace('heading.Update(eye,left,right,bodyYaw,headYaw,now,head);\n        rotating=heading.turning;', '''const bool plantedSwivel=swivel.Update(eye,left,right,headForward,now) && !walking;
        if(plantedSwivel) { heading={};stepIntent.Capture(walkEye,left,right,now);translating=false; }
        heading.Update(eye,left,right,bodyYaw,headYaw,now,head);
        rotating=heading.turning;''')
p=p.replace('const bool translationCoherent=h>0','const bool translationCoherent=!plantedSwivel && h>0')
Path('build/hands-out-capture/src/ControllerBodyAnchor.h').write_text(p)
