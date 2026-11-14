from pathlib import Path
p=Path('src/FirstPerson.cpp');s=p.read_text();old='''                (Cfg().firstPersonBodyScale==0 ? g_bodySize.eyeGap : 0.f)-
                std::max(0.f,VR().FirstPersonHeightDrop()-VisualBodyPostureDrop(item))*scale,Sub(postureEye,eye))) return false;''';assert s.count(old)==1;s=s.replace(old,'''                landing ? 0.f : (Cfg().firstPersonBodyScale==0 ? g_bodySize.eyeGap : 0.f)-
                std::max(0.f,VR().FirstPersonHeightDrop()-VisualBodyPostureDrop(item))*scale,landing ? Vec{} : Sub(postureEye,eye))) return false;''')
old='''    if(Cfg().firstPersonMovementStabilization && g_groundEye.valid && CanFitStandingBody(item) &&
       !g_groundTorso.Apply''';assert s.count(old)==1;s=s.replace(old,'''    if(!landing && Cfg().firstPersonMovementStabilization && g_groundEye.valid && CanFitStandingBody(item) &&
       !g_groundTorso.Apply''');p.write_text(s)
