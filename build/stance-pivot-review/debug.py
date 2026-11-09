from pathlib import Path
p=Path(__file__).parent
s=(p/'src/ControllerBodyAnchor.h').read_text()
s=s.replace('#pragma once','#pragma once\n#include <cstdio>')
s=s.replace('if(plantedPivot) { heading={};','if(plantedPivot) std::printf("GUARD,%llu\\n",frame);\n        if(plantedPivot) { heading={};')
(p/'src/ControllerBodyAnchor.h').write_text(s)
