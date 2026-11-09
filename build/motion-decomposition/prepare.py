from pathlib import Path
p=Path(__file__).parent
s=(p/'before.h').read_text().replace('#include "BodyFollower.h"','#include "BodyFollower.h"\n#include "TrackedTriangle.h"')
s=s.replace('#include "TrackedTriangle.h"','#include "TrackedTriangle.h"\n#include "PlantedSwivel.h"')
s=s.replace('PhysicalStepIntent stepIntent;','PhysicalStepIntent stepIntent;\n    PlantedSwivel swivel;')
s=s.replace('heading.Update(eye,left,right,bodyYaw,headYaw,now,head);\n        rotating=heading.turning;', '''if(!walking && swivel.Update(eye,left,right,headForward,now)) heading={};
        heading.Update(eye,left,right,bodyYaw,headYaw,now,head);
        rotating=heading.turning;''')
s=s.replace('auto candidate=[&](float angle) {','''bool articulated=false;
        const bool inspect=headForward.y<-.65f && raisedHands;
        auto candidate=[&](float angle) {''')
needle='''        const float c=Length(common);'''
replacement='''        if(inspect && beforeSpan>.12f && afterSpan>.12f) {
            TrackedTriangle oldPose,newPose;
            if(oldPose.Capture(motionEye,hands[0],hands[1]) && newPose.Capture(walkEye,left,right)) {
                const Vec pivot{motionPivot.x,head.y-.7f,motionPivot.z};
                auto residual=[&](Vec old,Vec current) { return Flat(Sub(current,Add(pivot,newPose.RotateFrom(oldPose,Sub(old,pivot))))); };
                stepDelta=residual(motionEye,walkEye);
                delta[0]=residual(hands[0],left);delta[1]=residual(hands[1],right);
                common=Scale(Add(delta[0],delta[1]),.5f);h=Length(stepDelta);articulated=true;
            }
        }
        const Vec motionDelta=articulated ? stepDelta : headDelta;
        const float c=Length(common);'''
assert needle in s;s=s.replace(needle,replacement)
s=s.replace('const Vec travel=Sub(headDelta,consumedTravel);','const Vec travel=Sub(motionDelta,consumedTravel);')
(p/'src/ControllerBodyAnchor.h').write_text(s)
for name in ['probe.cmd','test-new.cmd','test-candidates.cmd']:
 s=(Path('build/hands-out-capture')/name).read_text().replace('hands-out-capture\\src','motion-decomposition\\src').replace('/Febuild\\hands-out-capture','/Febuild\\motion-decomposition').replace('/Fobuild\\hands-out-capture','/Fobuild\\motion-decomposition')
 for exe in ['probe.exe','new-recorded.exe','curve.exe','pitch.exe','extended.exe','turn.exe']:s=s.replace('build\\hands-out-capture\\'+exe,'build\\motion-decomposition\\'+exe)
 (p/name).write_text(s)
