from pathlib import Path
p=Path('src/FirstPersonBodyIK.h');s=p.read_text();at='inline bool SolveFollowingBody('
helper='''// Choose the nearest knee bend on the two-bone circle that does not pass
// below its authored kneeling support (or ankle height for an upright leg).
// Only the bend direction changes: thigh/shin lengths and foot target stay fixed.
inline Vec SupportedKneePole(const Frame (&chain)[3],Vec foot,Vec preferred,float limitY) {
    const Vec delta=Sub(foot,chain[0].origin);Vec axis{};
    const float distance=Length(delta);if(distance<.001f || !Unit(delta,axis)) return preferred;
    const float a=Length(Sub(chain[1].origin,chain[0].origin)),b=Length(Sub(chain[2].origin,chain[1].origin));
    const float along=(a*a-b*b+distance*distance)/(2*distance);
    const float radius=std::sqrt(std::max(0.f,a*a-along*along));
    Vec bend{};if(!Unit(Sub(preferred,Scale(axis,Dot(preferred,axis))),bend)) return preferred;
    const float centreY=chain[0].origin.y+axis.y*along;
    if(centreY+radius*bend.y<=limitY || radius<.001f) return preferred;
    const Vec projected=Sub(Vec{0,-1,0},Scale(axis,-axis.y));Vec up{};
    if(!Unit(projected,up)) return preferred;
    const float required=std::clamp((centreY-limitY)/(radius*Length(projected))+.00001f,0.f,1.f);
    Vec side{};if(!Unit(Sub(bend,Scale(up,Dot(bend,up))),side)) Unit(Cross(axis,up),side);
    return Add(Scale(up,required),Scale(side,std::sqrt(std::max(0.f,1-required*required))));
}
'''
s=s.replace(at,helper+at)
s=s.replace('''            if(Length(nativePole)>a*.05f) pole=nativePole;
        }
        if(!SolveArm(chain,foot,pole,correction''','''            if(Length(nativePole)>a*.05f) pole=nativePole;
            pole=SupportedKneePole(chain,foot.origin,pole,std::max(footprint[first+1].origin.y,foot.origin.y));
        }
        if(!SolveArm(chain,foot,pole,correction''')
s=s.replace('''            if(Length(nativePole)>a*.05f) pole=nativePole;
        }
        if(!SolveArm(chain,foot,pole,changes''','''            if(Length(nativePole)>a*.05f) pole=nativePole;
            pole=SupportedKneePole(chain,foot.origin,pole,std::max(legs[first+1].origin.y,foot.origin.y));
        }
        if(!SolveArm(chain,foot,pole,changes''');p.write_text(s)
