from pathlib import Path
p=Path(__file__).parent
s=(p/'before.h').read_text().replace('#include "BodyFollower.h"','#include "BodyFollower.h"\n#include "PlantedSwivel.h"')
s=s.replace('PhysicalStepIntent stepIntent;','PhysicalStepIntent stepIntent;\n    PlantedSwivel swivel;')
s=s.replace('float turnOutput=0;', 'Vec intentArc{};\n    float turnOutput=0;')
s=s.replace('heading.Update(eye,left,right,bodyYaw,headYaw,now,head);\n        rotating=heading.turning;', '''const bool torsoSwivel=!walking && swivel.Update(eye,left,right,headForward,now);
        if(torsoSwivel) heading={};
        if(torsoSwivel && Length(previousPair)>.12f && Length(currentPair)>.12f) {
            const Vec arm{-previousPair.z/Length(previousPair)*.35f,0,previousPair.x/Length(previousPair)*.35f};
            intentArc=Add(intentArc,Sub(YawVector(arm,signedHandTurn),arm));
        }
        heading.Update(eye,left,right,bodyYaw,headYaw,now,head);
        rotating=heading.turning;''')
s=s.replace('const Vec headDelta=Flat(Sub(walkEye,motionEye));','Vec headDelta=Flat(Sub(walkEye,motionEye));')
s=s.replace('stepIntent.Observe(walkEye,left,right,now,','stepIntent.Observe(Sub(walkEye,intentArc),left,right,now,')
s=s.replace('const float c=Length(common);','''if(torsoSwivel && beforeSpan>.12f && afterSpan>.12f) {
            const float angle=BodyAngle(std::atan2(afterPair.x,afterPair.z)-std::atan2(beforePair.x,beforePair.z));
            const Vec torsoForward{-beforePair.z/beforeSpan,0,beforePair.x/beforeSpan};
            const Vec pivot=Sub(motionEye,Scale(torsoForward,.35f));
            auto residual=[&](Vec old,Vec current) {return Flat(Sub(current,Add(pivot,YawVector(Sub(old,pivot),angle))));};
            stepDelta=residual(motionEye,walkEye);headDelta=stepDelta;h=Length(stepDelta);
            delta[0]=residual(hands[0],left);delta[1]=residual(hands[1],right);
            common=Scale(Add(delta[0],delta[1]),.5f);
        }
        const float c=Length(common);''')
(p/'src/ControllerBodyAnchor.h').write_text(s)
