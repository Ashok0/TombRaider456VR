from pathlib import Path
p=Path('src/FirstPersonBodyIK.h');s=p.read_text();old='''    posed[0].origin=Add(footprint[0].origin,pelvisShift);
    posed[0].basis=footprint[0].basis;''';new='''    if(authoredKnees && grounded) {
        // The authored impact pose already supplies the deepest supported
        // crouch. A headset dip must not drive the pelvis through its feet.
        pelvisShift.y=std::min(0.f,pelvisShift.y);
        float highest=-INFINITY;
        for(int first:{1,4}) {
            const float a=Length(Sub(footprint[first+1].origin,footprint[first].origin));
            const float b=Length(Sub(footprint[first+2].origin,footprint[first+1].origin));
            const Vec socket=Add(footprint[first].origin,pelvisShift),foot=footprint[first+2].origin;
            const float x=socket.x-foot.x,z=socket.z-foot.z,reach=a+b-.001f;
            if(x*x+z*z<reach*reach) highest=std::max(highest,
                foot.y-std::sqrt(reach*reach-x*x-z*z)-footprint[first].origin.y);
        }
        pelvisShift.y=std::max(pelvisShift.y,std::min(0.f,highest));
    }
    posed[0].origin=Add(footprint[0].origin,pelvisShift);
    posed[0].basis=footprint[0].basis;''';assert s.count(old)==1;s=s.replace(old,new);p.write_text(s)
