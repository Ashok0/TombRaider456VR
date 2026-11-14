from pathlib import Path
p=Path('src/FirstPersonBodyIK.h');s=p.read_text();s=s.replace('float postureEyeOffset=0,Vec horizontalPostureOffset={}) {','float postureEyeOffset=0,Vec horizontalPostureOffset={},bool authoredKnees=false) {')
s=s.replace('''        if(!SolveArm(chain,foot,forward,correction,nullptr,nullptr,false)) return false;''','''        Vec pole=forward;
        if(authoredKnees) {
            Vec axis{};Unit(Sub(footprint[first+2].origin,footprint[first].origin),axis);
            const Vec knee=Sub(footprint[first+1].origin,footprint[first].origin);
            const Vec nativePole=Sub(knee,Scale(axis,Dot(knee,axis)));
            if(Length(nativePole)>a*.05f) pole=nativePole;
        }
        if(!SolveArm(chain,foot,pole,correction,nullptr,nullptr,false)) return false;''')
s=s.replace('Vec forward,const Frame* planted=nullptr) {','Vec forward,const Frame* planted=nullptr,bool authoredKnees=false) {')
s=s.replace('''        if(!SolveArm(chain,foot,forward,changes,nullptr,nullptr,false)) return false;''','''        Vec pole=forward;
        if(authoredKnees) {
            Vec axis{};Unit(Sub(legs[first+2].origin,legs[first].origin),axis);
            const Vec knee=Sub(legs[first+1].origin,legs[first].origin);
            const Vec nativePole=Sub(knee,Scale(axis,Dot(knee,axis)));
            if(Length(nativePole)>a*.05f) pole=nativePole;
        }
        if(!SolveArm(chain,foot,pole,changes,nullptr,nullptr,false)) return false;''');p.write_text(s)
p=Path('src/BodyFollower.h');s=p.read_text();s=s.replace('const Vec* contacts=nullptr) {','const Vec* contacts=nullptr,bool authoredKnees=false) {');s=s.replace('ReposeTorso(pose,target,forward,planted ? legReference : nullptr)','ReposeTorso(pose,target,forward,planted ? legReference : nullptr,authoredKnees)');p.write_text(s)
p=Path('src/FirstPerson.cpp');s=p.read_text();s=s.replace('landing ? Vec{} : Sub(postureEye,eye)))','landing ? Vec{} : Sub(postureEye,eye),landing))');s=s.replace('quiet,VisualBodyPostureDrop(item),contacts))','quiet,VisualBodyPostureDrop(item),contacts,landing))');p.write_text(s)
