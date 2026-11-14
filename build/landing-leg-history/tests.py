from pathlib import Path
r=Path.cwd();w=r/'build/landing-leg-history';stage=w/'123-work'
check='''#include "native_landing_poses.inl"
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
'''
(r/'tools/landing_support_checks.inl').write_text(check);(stage/'tests/landing_support_checks.inl').write_text(check)
s=(w/'probe.inl').read_text().replace('#include "../../tools/native_landing_poses.inl"','#include "landing_support_checks.inl"').replace('ProbeLanding','TestLandingSupportPalette')
s=s.replace('for(float factor:{0.f,.7f,1.f,1.2f}) for(float drop:{0.f,.12f}) {','for(float factor:{0.f,.7f,1.f,1.2f}) for(float drop:{0.f,.12f,.3f}) for(int status:{0,4}) {\n        *reinterpret_cast<int16_t*>(laraMemory+2)=int16_t(status);')
s=s.replace('''        for(const auto& s:nativeLandingPoses) {''','''        for(size_t n=0;n<=std::size(nativeLandingPoses);++n) {
            const auto& s=n==std::size(nativeLandingPoses)?*standing:nativeLandingPoses[n];''')
a=s.index('            const bool full=');b=s.index('\n        }',a)
s=s[:a]+'''            const auto camera=g_scenePose;
            const auto memory=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            Check(ApplyFullBodyIK(itemMemory,palette,15),"landing retains the complete torso/leg/arm solve");
            Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)) && memory==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)),
                "leg support does not alter landing camera, collision or animation");
            Frame out[15]{};
            for(int j=0;j<15;++j) Check(ReadIKJoint(objectInfo,palette,15,0,nullptr,nullptr,j,out[j]),"supported landing joint readable");
            if(s.clip!=103) {
                CheckLandingSupport(s.joints,out,factor==0?.845f:factor);
                for(int hand=0;hand<2;++hand) {
                    Frame wrist{};Basis basis{};
                    Check(BuildControllerWrist(hand,wrist,basis) && firstperson::Length(Sub(out[hand?10:13].origin,wrist.origin))<.1f,
                          "supported landing keeps live controller wrists on the full-body arms");
                }
            } else Check(g_bodyFollower.valid && !g_bodyLanding,"standing solver resumes normally after landing");
''' +s[b:]
s=s.replace('    VR().m_system=nullptr;','    skinFixture=nullptr;skinFixtureCount=0;worldCameraValid=false;g_bodySkinScope=false;');(r/'tools/landing_support_regression.inl').write_text(s)
p=r/'tools/upjump_arms_test.cpp';s=p.read_text();s=s.replace('#include "body_transition_regression.inl"','#include "body_transition_regression.inl"\n#include "landing_support_regression.inl"');s=s.replace('''        if(fullBody) { reset(which,true);TestStandingCalibration(objectInfo); }''','''        if(fullBody) { reset(which,true);TestStandingCalibration(objectInfo); }
        if(fullBody) { reset(which,true);TestLandingSupportPalette(objectInfo); }''');p.write_text(s)
# The collection harness uses its native draw trampoline and validates physics separately.
p=stage/'tests/landing_floor_selftest.h';s=p.read_text();s=s.replace('#include "landing_leg_checks.inl"','#include "landing_support_checks.inl"').replace('TestNativeLandingFloors','TestLandingSupportPalettes').replace('for(float drop:{0.f,.12f})','for(float drop:{0.f,.12f,.3f})')
s=s.replace('''        trackingtest::Clock clock;''','''        if(factor==0) {g_bodySize.valid=true;g_bodySize.torsoFitted=true;g_bodySize.factor=.845f;g_bodySize.eyeGap=.209f*LiveWorldUnitsPerMetre();g_bodySize.units=LiveWorldUnitsPerMetre();g_bodySizeObject=0;}
        trackingtest::Clock clock;''')
a=s.index('            const motiongun::Vec eye=');b=s.index('            for(int j=0;j<15;',a)
s=s[:a]+'''            const motiongun::Vec eye=Transform(s.joints[14],{0,-32,140});
            g_scenePose={int32_t(eye.x),int32_t(eye.y),int32_t(eye.z)};
'''+s[b:];s=s.replace('const float size=AnimatedBodyScale(item);','const float size=factor==0?.845f:factor;');s=s.replace('CheckLandingLegPalette(s.joints,out,size);','CheckLandingSupport(s.joints,out,size);')
a=s.index('                if(s.clip==24 && s.frame==0)');b=s.index('            } else Check(',a);s=s[:a]+s[b:]
s=s.replace('''            if(s.clip!=103) {''','''            Check(g_bodyFollower.valid,"landing retains the full-body solver");
            if(s.clip!=103) {''')
(stage/'tests/landing_support_selftest.h').write_text(s)
p=stage/'tests/collection_integration_selftest.h';s=p.read_text().replace('void TestCollectionIntegration() {','#include "landing_support_selftest.h"\nvoid TestCollectionIntegration() {').replace('    TestGaitArmStability();','    TestLandingSupportPalettes();\n    TestGaitArmStability();');p.write_text(s)
