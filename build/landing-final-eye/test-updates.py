from pathlib import Path
w=Path('build/landing-final-eye');stage=w/'123-work'
p=stage/'tests/firstperson_ik_selftest.h';s=p.read_text();a=s.index('    for (const auto clip : {std::pair<int,int>{2,24}');b=s.index('    Check(!locomotion::IsLandingAnimation',a);part=s[a:b];part=part.replace('''    for (const auto clip''','''    for(int game:{1,2,3}) for(float rise:{-.4f,-.17f,0.f,.2f}) for(bool tracked:{false,true})
    for (const auto clip''');part=part.replace('''        Reset(clip.first,{},true);''','''        Reset(clip.first,{},true);
        dll.module=game==1?L"tomb1.dll":game==2?L"tomb2.dll":L"tomb3.dll";
        testViewRise=rise;testConfig.positionalTracking=tracked;testConfig.firstPersonHeadTranslation=true;
''');part=part.replace('''        Check(eye.x_pos==30 && eye.y_pos==-480 && eye.z_pos==150,''','''        const float trackingY=tracked ? -rise*LiveWorldUnitsPerMetre() : 0;
        Check(eye.x_pos==30 && std::fabs(eye.y_pos+trackingY-(-480+std::min(0.f,trackingY)))<1.1f && eye.z_pos==150,''');part=part.replace('landing eye follows the head instead of staying at standing height','final landing eye stays at head height when physically lowering; upward tracking remains live');s=s[:a]+part+'    testViewRise=0;\n'+s[b:];p.write_text(s)
p=Path('tools/first_person_tests.cpp');s=p.read_text();old='const float expected=std::min(float(height)+tracking,-64.f);';assert s.count(old)==1;s=s.replace(old,'const float expected=std::min(float(height)+(stable ? std::min(0.f,tracking) : tracking),-64.f);');p.write_text(s)
