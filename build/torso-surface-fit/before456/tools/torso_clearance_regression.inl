#include "../src/TorsoClearance.h"
void TestTorsoClearance() {
    using namespace tr::firstperson;using Vec=tr::motiongun::Vec;
    const Vec points[15]={{0,-500,0},{-100,-500,0},{-100,-260,70},{-100,0,0},
        {100,-500,0},{100,-260,70},{100,0,0},{0,-650,0},
        {180,-700,0},{240,-460,30},{180,-250,70},{-180,-700,0},{-240,-460,30},{-180,-250,70},{0,-850,0}};
    Frame bind[15]{};for(int j=0;j<15;++j) bind[j]={IdentityBasis(),points[j]};
    TorsoEnvelope shape;Check(shape.Capture(bind),"torso clearance fits the neutral skeleton");
    for(float units:{423.f,1000.f}) for(float yaw:{0.f,.7f,1.5707963f,-2.5f})
    for(float scale:{.8f,1.f,1.2f}) for(float offset:{-32.f,32.f}) {
        const float size=units/1000*scale;
        Basis basis=RotationMatrix(AxisRotation({0,1,0},yaw));
        for(auto& row:basis.r) for(float& x:row) x*=size;
        // A real camera-relative rebase must not alter the inferred posture.
        const Frame world{basis,{100-offset,-200,300}};
        Frame pose[15]{},original[15]{};
        for(int j=0;j<15;++j) original[j]=pose[j]=Multiply(world,bind[j]);
        const Vec eye=Transform(world,{0,-800,40});
        TorsoClearance guard;
        Check(shape.Distance(pose[7],eye,units*.05f)<1,"straight-legged fixture puts the eye near the upper torso");
        Check(guard.Apply(&bind,pose,bind,eye,yaw,units,1,1,true),"torso clearance solves without changing calibration");
        Check(std::fabs(guard.angle)<=TorsoClearance::MaxAngle+.00001f && std::fabs(guard.angle)>0,
              "upper torso correction is nonzero and bounded to twelve degrees");
        Check(shape.Distance(pose[7],eye,units*.05f)>=shape.Distance(original[7],eye,units*.05f),
              "bounded correction improves camera clearance");
        if(scale>=1) Check(shape.Distance(pose[7],eye,units*.05f)>=.9999f,
              "reachable chest overlap clears both eyes with the standing legs preserved");
        for(int j=0;j<7;++j) Check(!std::memcmp(&pose[j],&original[j],sizeof(Frame)),
              "hip, knees and planted feet remain exactly unchanged");
        Check(!std::memcmp(&pose[14],&original[14],sizeof(Frame)),"tracked head frame is untouched");
        Check(std::fabs(Length(Sub(pose[7].origin,pose[0].origin))-Length(Sub(original[7].origin,original[0].origin)))<.001f,
              "clearance preserves spine length instead of stretching or translating the torso");
        for(int hand=0;hand<2;++hand) {
            const int first=hand?8:11;
            Check(std::fabs(Length(Sub(pose[first].origin,pose[7].origin))-Length(Sub(original[first].origin,original[7].origin)))<.001f,
                  "shoulder remains rigidly connected to the corrected chest");
            Frame chain[3]={pose[first],pose[first+1],pose[first+2]},arms[3]={bind[first],bind[first+1],bind[first+2]},correction[3]{};
            Frame target=original[first+2];target.origin=Transform(world,{hand?250.f:-250.f,-600,300});
            Check(SolveTrackedArm(chain,target,{0,1,-1},arms,correction,true),"corrected shoulder still solves to the live controller");
            const auto wrist=Multiply(correction[2],chain[2]);
            Check(Length(Sub(wrist.origin,target.origin))<.05f,"tracked wrist stays exact after torso clearance");
            const auto shoulder=Multiply(correction[0],chain[0]);
            Check(Length(Sub(shoulder.origin,pose[first].origin))<.001f,"arm solving cannot detach the shoulder");
        }
        Frame repeat[15]{};for(int j=0;j<15;++j) repeat[j]=original[j];
        const float held=guard.angle;
        Check(guard.Apply(&bind,repeat,bind,eye,yaw,units,1,1.01,true) && guard.angle==held,
              "repeated stereo and material passes cannot advance the correction");
        for(int j=7;j<14;++j) Check(Length(Sub(repeat[j].origin,pose[j].origin))<.001f,"repeated draws produce identical corrected joints");
        const Vec clearEye=Transform(world,{0,-1000,40});
        for(int frame=2;frame<180;++frame) {
            for(int j=0;j<15;++j) repeat[j]=original[j];
            const float before=guard.angle;
            guard.Apply(&bind,repeat,bind,clearEye,yaw,units,frame,1+frame/90.0,true);
            Check(std::fabs(guard.angle-before)<.018f,"clearance releases smoothly without a pose pop");
        }
        Check(std::fabs(guard.angle)<.00001f,"leaving the torso restores the original authored posture");
    }
    Frame crouched[15]{};for(int j=0;j<15;++j) {crouched[j]=bind[j];if(j==0 || j>=7) crouched[j].origin.y+=100;}
    TorsoClearance guard;guard.Apply(&bind,crouched,bind,{0,-800,40},0,1000,1,1,true);
    Check(guard.angle==0,"already-clear bent-knee torso needs no adjustment");
}
