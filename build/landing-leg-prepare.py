from pathlib import Path
import shutil
w=Path('build/landing-leg-history');w.mkdir(exist_ok=True)
for n in ['src/FirstPerson.cpp','src/FirstPersonBodyIK.h','src/GroundTorso.h']:
 p=w/'before'/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(n,p)
s=Path('build/landing-floor-fit/make-focused.py').read_text().replace('landing-floor-fit','landing-leg-history');(w/'make-focused.py').write_text(s)
s=Path('build/landing-floor-fit/probe.inl').read_text().replace('#include "native-poses.inl"','#include "../../tools/native_landing_poses.inl"').replace('''        for(const auto& s:nativeLandingPoses) {''','''        g_groundTorso.Reset();g_torsoSurface.Reset();g_torsoPose.Reset();
        if(factor==0) {g_bodySize.valid=true;g_bodySize.torsoFitted=true;g_bodySize.factor=.845f;g_bodySize.eyeGap=88.4f;g_bodySize.units=LiveWorldUnitsPerMetre();g_bodySizeObject=0;}
        for(const auto& s:nativeLandingPoses) {''').replace('''            const Vec eye=Transform''','''            VR().m_rawHeadPose.m[1][3]=VR().m_firstPersonNeutral[1]-(s.clip==103 ? 0.f : drop);
            const Vec eye=Transform''')
(w/'probe.inl').write_text(s)
