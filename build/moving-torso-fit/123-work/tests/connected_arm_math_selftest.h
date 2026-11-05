void TestConnectedArmMath() {
    using namespace tr;using namespace tr::motiongun;
    const auto identity=firstperson::IdentityBasis();
    Frame native[15]{};
    const motiongun::Vec positions[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    for(int i=0;i<15;++i) native[i]={identity,positions[i]};
    auto nearVec=[](motiongun::Vec a,motiongun::Vec b) { return firstperson::Length(Sub(a,b))<.04f; };
    // A fixed controller near the shoulder used to magnify a .057-degree
    // torso change into roughly 20 degrees of material roll. Test both sides,
    // reversal of the head glance, repeated draws, and exact wrist tracking.
    for(int hand=0;hand<2;++hand) {
        const float side=hand ? 1.f : -1.f;
        const Frame armBind[3]={{identity,{side*180,-700,0}},
            {identity,{side*240,-460,30}},{identity,{side*180,-250,70}}};
        const Frame torsoBind{identity,{0,-650,-60}};
        const Frame target{Multiply(firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},side*3)),motiongun::GunBasis(identity)),
            {side*180,-700,50}};
        firstperson::ArmRollContinuity history{};
        Frame previous[3]{};bool havePrevious=false;
        for(int sample=0;sample<=1400;++sample) {
            const int step=sample<=700 ? sample-350 : 1050-sample;
            Frame torso{firstperson::RotationMatrix(firstperson::AxisRotation({0,1,0},step*.001f)),torsoBind.origin};
            Frame chain[3]{},changes[3]{};auto seed=history;
            Check(firstperson::PistolArmReference(torso,torsoBind,armBind,1,chain),"glance fixture builds pistol model reference");
            Check(firstperson::SolveTrackedArm(chain,target,{side*.6f,1,-.25f},armBind,changes,true,&history),"pistol glance roll solve succeeds");
            for(int j=0;j<3;++j) {
                const auto actual=Multiply(changes[j],chain[j]);
                if(havePrevious && j<2) {
                    const auto q=firstperson::RotationProduct(firstperson::RotationOf(actual.basis),
                        firstperson::RotationInverse(firstperson::RotationOf(previous[j].basis)));
                    const float angle=2*std::atan2(firstperson::Length(q.v),std::fabs(q.w));
                    if(angle>=.07f) std::fprintf(stderr,"glance sample=%d step=%d hand=%d joint=%d angle=%f distance=%f\n",sample,step,hand,j,angle,firstperson::Length(Sub(target.origin,chain[0].origin)));
                    Check(angle<.07f,"small shoulder glance cannot cause a pistol arm roll spike");
                }
                if(j<2) Check(nearVec(Transform(changes[j],chain[j+1].origin),Multiply(changes[j+1],chain[j+1]).origin),
                    "roll continuity keeps the arm mesh joined at its elbow and wrist");
                if(j==0) Check(nearVec(actual.origin,chain[0].origin),"roll continuity never detaches the shoulder");
                if(j==2) {
                    Check(nearVec(actual.origin,target.origin),"glance protection leaves pistol wrist position exact");
                    for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                        Check(std::fabs(actual.basis.r[r][c]-target.basis.r[r][c])<.001f,"glance protection leaves pistol aim and wrist roll exact");
                }
                previous[j]=actual;
            }
            Frame repeat[3]{};
            Check(firstperson::SolveTrackedArm(chain,target,{side*.6f,1,-.25f},armBind,repeat,true,&seed),"same-pose pistol redraw succeeds");
            Check(!std::memcmp(repeat,changes,sizeof(repeat)),"same-pose redraw cannot advance pistol roll history");
            havePrevious=true;
        }
    }
    // Model-proportion pistol arms must keep connected shoulders and exact wrists
    // through full wrist rolls, mirrored arms and calibrated body scales.
    for(int hand=0;hand<2;++hand) for(float size:{.8f,1.f,1.2f}) for(int roll=-8;roll<=8;++roll) {
        const int first=hand ? 8 : 11;
        const Frame armBind[3]={native[first],native[first+1],native[first+2]};
        Frame chain[3]{},changes[3]{};
        Check(firstperson::PistolArmReference(native[7],native[7],armBind,size,chain),"model-proportion pistol reference is valid");
        const Frame target{firstperson::RotationMatrix(firstperson::AxisRotation({0,0,1},roll*.4f)),
                           Add(chain[0].origin,{hand ? 100.f : -100.f,0,900.f})};
        Check(firstperson::SolveTrackedArm(chain,target,{hand ? .6f : -.6f,1,-.25f},armBind,changes),
              "pistol reaches beyond anatomical range with full controller roll");
        Frame output[3]{};for(int j=0;j<3;++j) output[j]=Multiply(changes[j],chain[j]);
        Check(nearVec(output[2].origin,target.origin),"pistol wrist follows controller immediately");
        Check(nearVec(output[0].origin,chain[0].origin),"pistol shoulder remains anchored to the torso beyond natural reach");
        const float reach=firstperson::Length(Sub(target.origin,chain[0].origin));
        const float natural=firstperson::Length(Sub(chain[1].origin,chain[0].origin))+firstperson::Length(Sub(chain[2].origin,chain[1].origin));
        for(int j=0;j<2;++j) {
            Check(std::fabs(firstperson::Length(Sub(output[j+1].origin,output[j].origin))-
                std::max(1.f,reach/(natural-std::min(firstperson::Length(Sub(chain[1].origin,chain[0].origin)),firstperson::Length(Sub(chain[2].origin,chain[1].origin)))*.001f))*size*firstperson::Length(Sub(armBind[j+1].origin,armBind[j].origin)))<.04f,
                "pistol reach uses only the extension needed to keep the shoulder attached and wrist tracked");
            Check(nearVec(Transform(changes[j],chain[j+1].origin),output[j+1].origin),"pistol arm meshes remain joined at elbow and wrist");
        }
    }
}
