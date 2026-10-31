from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[1]
source=Path('C:/dev/TombRaider123VR/src')
shutil.copy2(source/'WristCap.h',root/'src/WristCap.h')
s=(source/'BoneSkin.cpp').read_text(encoding='utf-8')
helpers=s[s.index('struct WristRim {'):s.index('// Read one body buffer',s.index('struct WristRim {'))]
draw=s[s.index('void BoneSkinDrawWristCap('):s.index('void BoneSkinInstall()',s.index('void BoneSkinDrawWristCap('))]
draw=draw.replace('const float* palette=FirstPersonHandSkin(hand);\n    float centre[3]{};','float centre[3]{};\n    const float* palette=FirstPersonSealedHandSkin(hand,centre);')
draw=draw.replace('!FirstPersonWristCentre(centre) || ','')
draw=draw.replace('!g_prog[program].handLive','g_prog[program].handJoint!=hand')
draw=draw.replace('const int level=AppFlag(drva::app_off::level);','const auto* app=*reinterpret_cast<const uint8_t* const*>(Base()+L().app);\n    const int level=app ? *reinterpret_cast<const int*>(app+drva::app_off::level) : -1;')
p=root/'src/BoneSkin.cpp';t=p.read_text(encoding='utf-8')
t=t.replace('#include "MotionHandSkin.h"','#include "MotionHandSkin.h"\n#include "WristCap.h"\n#include "GameDll.h"')
assert t.count('// Read one body buffer')==1
t=t.replace('// Read one body buffer',helpers+'// Read one body buffer',1)
t=t.replace('void BoneSkinInstall() {',draw+'void BoneSkinInstall() {',1)
t=t.replace('void BoneSkinShutdown() {','void BoneSkinShutdown() {\n    ResetWristCaps();',1)
p.write_text(t,encoding='utf-8',newline='\n')
