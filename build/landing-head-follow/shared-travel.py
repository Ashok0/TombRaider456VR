from pathlib import Path
p=Path('build/landing-head-follow/candidate/PlantedPivotEvidence.h');s=p.read_text();at='    bool Update(Vec eye,Vec left,Vec right,Vec forward,double now) {'
s=s.replace(at,'''    // Separate steady shared travel from a changing aiming arc. Fit head and
    // hand midpoint independently to rotation about a fixed pivot plus velocity.
    bool SharedTravel(Vec eye,Vec left,Vec right,double now) const {
        if(count<12) return false;
        const auto& first=samples[count==32 ? next : 0];
        if(now-first.time<.32) return false;
        const Vec pair=Flat(Sub(first.right,first.left));
        if(Length(pair)<.12f) return false;
        const float angle=std::atan2(pair.x,pair.z);
        const Vec p[2]={first.eye,Scale(Add(first.left,first.right),.5f)};
        double m[4][8]{};int used=0;
        auto add=[&](const Sample& v) {
            const Vec q=Flat(Sub(v.right,v.left));if(Length(q)<.12f) return;
            const double turn=BodyAngle(std::atan2(q.x,q.z)-angle);
            const double c=std::cos(turn),sn=std::sin(turn),t=v.time-first.time;
            const double a[2][4]={{1-c,-sn,t,0},{sn,1-c,0,t}};
            const Vec point[2]={v.eye,Scale(Add(v.left,v.right),.5f)};
            for(int row=0;row<2;++row) for(int i=0;i<4;++i) {
                for(int j=0;j<4;++j) m[i][j]+=a[row][i]*a[row][j];
                for(int k=0;k<2;++k) {
                    const double b=row==0 ? point[k].x-c*p[k].x-sn*p[k].z : point[k].z+sn*p[k].x-c*p[k].z;
                    m[i][4+k]+=a[row][i]*b;
                }
            }
            ++used;
        };
        for(int i=0;i<count;++i) add(samples[i]);add({eye,left,right,now});
        if(used<12) return false;
        for(int k=0;k<4;++k) {
            int pivot=k;for(int i=k+1;i<4;++i) if(std::fabs(m[i][k])>std::fabs(m[pivot][k])) pivot=i;
            if(std::fabs(m[pivot][k])<1.e-7) return false;
            if(pivot!=k) for(int j=0;j<6;++j) std::swap(m[k][j],m[pivot][j]);
            const double divisor=m[k][k];for(int j=k;j<6;++j) m[k][j]/=divisor;
            for(int i=0;i<4;++i) if(i!=k) {const double f=m[i][k];for(int j=k;j<6;++j) m[i][j]-=f*m[k][j];}
        }
        const Vec h{float(m[2][4]),0,float(m[3][4])},v{float(m[2][5]),0,float(m[3][5])};
        return Length(h)>.12f && Length(v)>.12f && Length(Sub(h,v))<.08f && Dot(h,v)>.95f*Length(h)*Length(v);
    }
''' +at)
s=s.replace('const bool inspecting=(forward.y', 'if(closeInspection && SharedTravel(eye,left,right,now)) straight=true;\n        const bool inspecting=(forward.y');p.write_text(s)
