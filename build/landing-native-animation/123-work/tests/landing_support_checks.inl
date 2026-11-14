#include "native_landing_poses.inl"
void CheckLandingSupport(const tr::motiongun::Frame (&native)[15],const tr::motiongun::Frame (&out)[15],float size) {
    using namespace tr;using namespace tr::motiongun;
    Frame expected[15]{};firstperson::ScaleBody(native,size,expected);
    for(int first:{1,4}) {
        Check(std::fabs(out[first+2].origin.y-expected[first+2].origin.y)<.2f,
              "landing retains native scaled foot contact height");
        Check(out[first+1].origin.y<=std::max(expected[first+1].origin.y,expected[first+2].origin.y)+.2f,
              "landing knee cannot fold below authored kneeling support");
        for(int j=0;j<2;++j) Check(std::fabs(firstperson::Length(Sub(out[first+j+1].origin,out[first+j].origin))-
            firstperson::Length(Sub(expected[first+j+1].origin,expected[first+j].origin)))<.2f,
            "landing legs remain rigid connected segments without shrinking or stretching");
    }
}
