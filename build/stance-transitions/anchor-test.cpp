#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* s) {++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
#include "close_hands_motion_regression.inl"
#include "recorded_hands_out_regression.inl"
#include "recorded_planted_glance_regression.inl"
#include "pitched_glance_regression.inl"
#include "extended_arm_glance_regression.inl"
#include "body_anchor_capture.inl"

void TestControllerBodyAnchor() {
    TestRecordedPlantedGlances();
    TestRecordedHandsOutSwivels();
    TestCloseHandsRecordedMotion();
    TestPitchedGlanceTracking();
    TestExtendedArmGlances();
    TestCapturedBodyAnchorTransitions();
    for(float drop:{0.f,.1f,.3f,.6f}) for(float radius:{.45f,.7f,.95f}) {
        Check(tr::firstperson::PlantedPostureDrop(drop,0,radius)==drop,
            "an actual vertical crouch retains its complete posture drop");
        const float lean=radius*.6f; // A 3-4-5 triangle lowers the neck by one fifth of its radius.
        Check(std::fabs(tr::firstperson::PlantedPostureDrop(drop+radius*.2f,lean,radius)-drop)<.000001f,
            "a real crouch combined with a planted side bend retains only the true crouch component");
    }

    using namespace tr::firstperson;
    // The facing blend reduces the false yaw caused by changing one arm's
    // reach, and stays well defined across angular wrap and close hand spacing.
    for(float base:{0.f,1.5f,3.1f,-3.1f}) for(float side:{-1.f,1.f}) {
        const Vec eye{0,1.7f,0};
        const Vec left=YawVector(Vec{-.2f,1,.3f},base),right=YawVector(Vec{.2f,1,.3f},base);
        BodyRelativeFacing estimate;estimate.Capture(eye,base,left,right);
        const Vec movedLeft=side<0 ? Add(left,YawVector(Vec{0,0,.15f},base)) : left;
        const Vec movedRight=side>0 ? Add(right,YawVector(Vec{0,0,.15f},base)) : right;
        const Vec pair=Sub(movedRight,movedLeft),original=Sub(right,left);
        const float line=BodyAngle(base+std::atan2(pair.x,pair.z)-std::atan2(original.x,original.z));
        const float facing=estimate.Estimate(eye,base,line,movedLeft,movedRight);
        Check(std::fabs(BodyAngle(facing-base))<std::fabs(BodyAngle(line-base))*.6f,
            "body-relative hand votes reduce the facing error caused by asymmetric arm reach");
        const float close=estimate.Estimate(eye,base,base,YawVector(Vec{-.01f,1,.3f},base),YawVector(Vec{.01f,1,.3f},base));
        Check(std::isfinite(close) && std::fabs(BodyAngle(close-base))<.00001f,
            "body-relative facing is stable when the hands approach one another at any heading");
    }
    {
        BodyRelativeFacing facing;const Vec eye{0,1.7f,0};
        facing.Capture(eye,0,Vec{-.2f,1,.3f},Vec{.2f,1,.3f});
        const Vec pivot{0,1.7f,-.15f};
        const auto left=Add(pivot,YawVector(Vec{-.2f,-.7f,.45f},3.12f));
        const auto right=Add(pivot,YawVector(Vec{.2f,-.7f,.45f},-3.12f));
        Check(std::fabs(BodyAngle(facing.Estimate(eye,0,3.14159265f,left,right)-3.14159265f))<.03f,
            "opposite-facing hand votes straddling angle wrap cannot cancel into forward facing");
    }
    // Rendering follows elapsed time rather than controller-checkpoint cadence.
    for(int hz:{72,90,120}) {
        BodyFollower body;body.Update(1,0,0,0,0,{},{},false,BodyFollowSettings());
        for(int i=1;i<=hz;++i) {
            body.Update(i+1,double(i)/hz,0,0,0,{},{},false,BodyFollowSettings(),{},false,true,0,true,true,.08f,true);
            Check(body.yaw==0,"small accepted facing errors keep the lower-body geometric lock planted");
        }
        for(int i=1;i<=hz*3;++i) {
            const float goal=.2f+i*.6f/hz;
            body.Update(hz+i+1,1.+double(i)/hz,0,0,0,{},{},false,BodyFollowSettings(),{},false,true,0,true,true,goal,true);
            if(i>hz/2) Check(std::fabs(BodyAngle(body.yaw-goal))<.03f,
                "continuous physical facing remains responsive at different headset refresh rates");
        }
    }
    ControllerBodyAnchor a;
    const Vec left{-.2f,1,.3f},right{.2f,1,.3f},head{0,1.7f,0};
    a.Update(1,1,head,left,right,0,0,true,false);
    a.Update(2,1.15,head,left,right,0,0,true,false);
    Check(a.held,"stationary controller positions arm body hold after settling");
    for(int frame=3;frame<100;++frame) {
        const Vec eye{.12f*std::sin(frame*.1f),1.7f,.10f*std::cos(frame*.1f)};
        a.Update(frame,1.15+(frame-2)/90.,eye,left,right,0,0,true,false);
        Check(a.held && Length(Flat(Sub(Sub(eye,a.offset),head)))<.00001f,
              "stationary controllers hold body anchor while HMD moves in every horizontal direction");
    }
    {
        ControllerBodyAnchor release;
        release.Update(1,0,head,left,right,0,0,true,false);
        release.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec lean{.30f,0,0},step{.03f,0,0};
        release.Update(3,.21,Add(head,lean),left,right,0,0,true,false);
        for(int i=1;i<=20;++i) {
            const auto delta=Scale(step,i/20.f);
            release.Update(i+3,.21+i/90.,Add(head,delta),Add(left,delta),Add(right,delta),0,0,true,false);
        }
        BodyFollower facing;
        facing.Update(1,0,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        facing.Update(2,.02,0,1.5f,0,step,{},false,tr::firstperson::BodyFollowSettings(),{},false,true,0,false,true);
        Check(std::fabs(facing.yaw)<.00001f,"translation release cannot snap the held body toward a head-only glance");
        facing.Update(3,.03,0,1.6f,0,step,{},false,tr::firstperson::BodyFollowSettings(),{},false,true,.1f,false,true);
        Check(std::fabs(facing.yaw-.1f)<.00001f,"controller-owned facing still accepts corroborated physical turns");
        const auto footprint=Sub(Add(head,step),release.offset);
        Check(Length(Flat(Sub(footprint,Add(head,step))))<.004f,
              "physical release carries only controller movement, never accumulated head lean");
        for(int frame=24;frame<40;++frame) {
            const auto eye=Add(head,{.15f+(frame-23)*.01f,0,0});
            release.Update(frame,.44+(frame-23)/90.,eye,Add(left,step),Add(right,step),0,0,true,false);
            Check(Length(Flat(Sub(Sub(eye,release.offset),footprint)))<.00001f,
                  "head-only motion stays excluded during physical-release settling");
        }
    }
    {
        // A large one-hand gesture must not drag the torso when the other hand
        // drifts only a few centimetres in the same direction (session repro).
        ControllerBodyAnchor gesture;
        gesture.Update(1,0,head,left,right,0,0,true,false);
        gesture.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec lstep{.40f,0,0},rstep{.03f,0,0},eyeStep{.215f,0,0};
        gesture.Update(3,.21,Add(head,eyeStep),Add(left,lstep),Add(right,rstep),0,0,true,false);
        Check(gesture.held && Length(Flat(Sub(Sub(Add(head,eyeStep),gesture.offset),head)))<.00001f,
              "asymmetric controller drift cannot be averaged into roomscale body travel");
        ControllerBodyAnchor glance;
        glance.Update(1,0,head,left,right,0,0,true,false);
        glance.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec drift{.03f,0,0};
        glance.Update(3,.21,Add(head,drift),Add(left,drift),Add(right,drift),0,0,true,false,1.f);
        Check(glance.held,"small common hand drift during a sideways head glance cannot release the body");
        ControllerBodyAnchor parallax;
        parallax.Update(1,0,head,left,right,0,0,true,false);
        parallax.Update(2,.2,head,left,right,0,0,true,false);
        parallax.Update(3,.21,head,Add(left,drift),Add(right,drift),0,0,true,false,.07f);
        Check(parallax.held,"parallel controller drift is not a physical turn around the head");
        ControllerBodyAnchor stale;
        stale.Update(1,0,head,left,right,0,0,true,false);
        stale.Update(2,.2,head,left,right,0,0,true,false);
        const tr::motiongun::Vec gestureShift{.1f,0,0};
        stale.Update(3,.21,head,Add(left,gestureShift),Add(right,gestureShift),0,0,true,false);
        stale.Update(4,.5,head,Add(left,gestureShift),Add(right,gestureShift),0,0,true,false);
        stale.Update(5,.51,Add(head,gestureShift),Add(left,gestureShift),Add(right,gestureShift),0,0,true,false);
        Check(stale.held && Length(Flat(Sub(Sub(Add(head,gestureShift),stale.offset),head)))<.00001f,
              "head movement cannot revive an old controller gesture as a physical step");
        PhysicalBodyTurn signal;
        signal.Observe(1,0,0,Sub(left,head),Sub(right,head),true);
        Check(signal.Observe(2,.01,.07f,Sub(Add(left,drift),head),Sub(Add(right,drift),head),true)==0,
              "head-relative hand parallax cannot accumulate a latent body turn");
    }
    {
        ControllerBodyAnchor resumed;
        resumed.Update(1,0,head,left,right,0,0,true,false);
        resumed.Update(2,.2,head,left,right,0,0,true,false);
        resumed.Update(3,.5,head,left,right,0,0,true,false,1.2f);
        for(int i=1;i<=90;++i) {
            const tr::motiongun::Vec shift{.001f*i,0,0};
            resumed.Update(i+3,.5+i/90.,Add(head,shift),Add(left,shift),Add(right,shift),0,0,true,false,1.2f);
        }
        Check(!resumed.held && resumed.head.x>.06f,
              "slow real walking remains available after a sideways head glance settles");
    }
    ControllerBodyAnchor noisy;noisy.Update(1,0,head,left,right,0,0,true,false);
    noisy.Update(2,.3,head,left,right,0,0,true,false);
    for(int i=3;i<100;++i) {
        const tr::motiongun::Vec eye{.12f*std::sin(i*.1f),1.7f,.1f*std::cos(i*.1f)};
        const tr::motiongun::Vec jitter{.0065f*(i%2 ? 1.f : -1.f),.002f,0}; // 13 mm between samples, seen in the session log.
        noisy.Update(i,.3+(i-2)/90.,eye,Add(left,jitter),Sub(right,jitter),0,0,true,false);
        Check(noisy.held && Length(Flat(Sub(Sub(eye,noisy.offset),head)))<.00001f,
              "recorded controller-scale jitter cannot release the body to head translation");
    }
    a.Update(100,2.4,head,left,right,0,0,true,false); // Settle the glance before a real step.
    for(int i=1;i<=20;++i) {
        const Vec step{.002f*i,0,0};
        a.Update(100+i,2.4+i/90.,Add(head,step),Add(left,step),Add(right,step),0,0,true,false);
    }
    Check(!a.held,"coordinated physical head and controller translation releases the hold");
    a.Update(121,2.85,Add(head,{.04f,0,0}),Add(left,{.04f,0,0}),Add(right,{.04f,0,0}),0,0,true,false);
    Check(a.held,"controllers can settle again after motion");
    a.Update(122,2.86,head,left,right,.4f,0,true,false);
    Check(Length(a.offset)==0 && a.reason==4,"stick turning discards the physical hold without stale correction");
    a.Update(123,3.1,head,left,right,.4f,0,true,false);
    a.Update(124,3.11,head,left,right,.4f,0,true,true);
    Check(!a.held,"stick travel remains available with physically stationary controllers");
    ControllerBodyAnchor turning;BodyFollower follower;PhysicalBodyTurn signal;
    turning.Update(1,0,head,left,right,0,0,true,false);
    turning.Update(2,.3,head,left,right,0,0,true,false);
    signal.Observe(2,.3,0,Sub(left,head),Sub(right,head),true);
    follower.Update(2,.3,0,0,0,{}, {},false,BodyFollowSettings());
    bool released=false;
    for(int i=1;i<=100;++i) {
        const float angle=i*.005f;const double now=.3+i/90.;
        const auto l=Add(head,YawVector(Sub(left,head),angle)),r=Add(head,YawVector(Sub(right,head),angle));
        turning.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
        const auto turn=signal.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
        follower.Update(i+2,now,0,angle,0,{}, {},false,BodyFollowSettings(),{},false,true,turning.turnOutput,turning.held,true,turning.heading.active ? turning.heading.target : NAN,turning.rotating);
        released=released || turning.rotating;
        Check(std::fabs(BodyAngle(follower.yaw-angle))<.3f,
              "whole-body turns clear the conservative planted-stance threshold and follow controller heading");
    }
    Check(released,"deliberate physical turning releases only the angular hold");
    {
        ControllerBodyAnchor jitter;PhysicalBodyTurn signal;BodyFollower follower;
        jitter.Update(1,0,head,left,right,0,0,true,false);
        jitter.Update(2,.2,head,left,right,0,0,true,false);
        signal.Observe(2,.2,0,Sub(left,head),Sub(right,head),true);
        follower.Update(2,.2,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        for(int i=1;i<=900;++i) {
            const double now=.2+i/90.;const float glance=1.5f*std::sin(i*.03f);
            const auto eye=Add(head,{.12f*std::sin(i*.027f),0,.08f*std::sin(i*.019f)});
            const auto l=Add(left,{.0065f*std::sin(i*1.7f),0,.0065f*std::cos(i*1.3f)});
            const auto r=Add(right,{.0065f*std::cos(i*1.1f),0,.0065f*std::sin(i*1.9f)});
            jitter.Update(i+2,now,eye,l,r,0,follower.yaw,true,false,glance);
            const float turn=signal.Observe(i+2,now,glance,Sub(l,eye),Sub(r,eye),true);
            follower.Update(i+2,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,jitter.turnOutput,jitter.held,true,jitter.heading.active ? jitter.heading.target : NAN,jitter.rotating);
            Check(jitter.held && std::fabs(follower.yaw)<.00001f,
                  "controller jitter in both horizontal axes cannot release head-only glance rotation");
        }
    }
    // Evaluate the complete angular detector + body hold, including realistic
    // millimetre hand variation. Missed samples must not erase part of a turn.
    for(float speed:{.002f,.008f,.02f}) for(float sign:{-1.f,1.f}) for(int lag:{0,3}) {
        ControllerBodyAnchor hold;PhysicalBodyTurn signal;BodyFollower follower;
        hold.Update(1,0,head,left,right,0,0,true,false);
        hold.Update(2,.2,head,left,right,0,0,true,false);
        signal.Observe(2,.2,0,Sub(left,head),Sub(right,head),true);
        follower.Update(2,.2,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        tr::motiongun::Vec l{},r{};
        for(int i=1;i<=360;++i) {
            const float angle=sign*speed*i;const double now=.2+i/90.;
            l=Add(head,YawVector(Add(Sub(left,head),{.003f*std::sin(i*1.7f),0,.003f*std::cos(i*1.3f)}),sign*speed*std::max(0,i-lag)));
            r=Add(head,YawVector(Add(Sub(right,head),{.003f*std::cos(i*1.1f),0,.003f*std::sin(i*1.9f)}),sign*speed*std::max(0,i-lag-(lag ? 2 : 0))));
            hold.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
            const float turn=signal.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
            follower.Update(i+2,now,0,angle,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,hold.turnOutput,hold.held,true,hold.heading.active ? hold.heading.target : NAN,hold.rotating);
            Check(std::fabs(BodyAngle(follower.yaw-angle))<(std::fabs(angle)<.4f ? .4f : lag ? .35f : .12f),
                  "noisy real roomscale turns retain their rotation through the stationary-controller hold");
        }
        const float final=sign*speed*360,bodyBefore=follower.yaw;
        const Vec pair=Sub(r,l),initialPair=Sub(right,left);
        const float controllerYaw=BodyAngle(std::atan2(pair.x,pair.z)-std::atan2(initialPair.x,initialPair.z));
        Check(std::fabs(BodyAngle(bodyBefore-controllerYaw))<.09f,"roomscale turn ends within the controller noise deadband without cumulative yaw loss");
        auto fixedHold=hold;auto fixedFollower=follower;
        for(int i=1;i<=90;++i) {
            const double now=4.2+i/90.;const float glance=final+i*.015f;
            hold.Update(362+i,now,head,l,r,0,follower.yaw,true,false,glance);
            const float turn=signal.Observe(362+i,now,glance,Sub(l,head),Sub(r,head),true);
            follower.Update(362+i,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,hold.turnOutput,hold.held,true,hold.heading.active ? hold.heading.target : NAN,hold.rotating);
            fixedHold.Update(362+i,now,head,l,r,0,fixedFollower.yaw,true,false,final);
            fixedFollower.Update(362+i,now,0,final,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,
                fixedHold.turnOutput,fixedHold.held,true,fixedHold.heading.active ? fixedHold.heading.target : NAN,fixedHold.rotating);
            Check(std::fabs(BodyAngle(follower.yaw-fixedFollower.yaw))<.00001f,
                  "a head glance cannot add rotation beyond completing the already accepted controller turn");
        }
    }
    // Real arms need not remain a rigid triangle while the player turns.
    // Small elbow movements can defeat the strict incremental detector for
    // an entire half turn; the mesh must still recover its overall facing.
    for(float sign:{-1.f,1.f}) {
        ControllerBodyAnchor anchor;BodyFollower follower;PhysicalBodyTurn fine;
        anchor.Update(1,0,head,left,right,0,0,true,false);
        anchor.Update(2,.2,head,left,right,0,0,true,false);
        fine.Observe(2,.2,0,Sub(left,head),Sub(right,head),true);
        follower.Update(2,.2,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        for(int i=1;i<=400;++i) {
            const double now=.2+i/90.;const float angle=sign*std::min(i,360)*.01f;
            const float flex=i<=360 ? 1.f : std::max(0.f,1-(i-360)/15.f);
            const auto l=Add(head,YawVector(Add(Sub(left,head),{0,0,flex*.07f*std::sin(i*.31f)}),angle));
            const auto r=Add(head,YawVector(Add(Sub(right,head),{0,0,flex*.07f*std::cos(i*.27f)}),angle));
            anchor.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
            const float turn=fine.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
            follower.Update(i+2,now,0,angle,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,anchor.turnOutput,anchor.held,true,anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
        }
        Check(std::fabs(BodyAngle(follower.yaw-sign*3.6f))<.15f,
              "overall roomscale heading cannot remain facing backward when the arms move naturally during a turn");
        const float stopped=follower.yaw;
        const auto stoppedLeft=Add(head,YawVector(Sub(left,head),sign*3.6f));
        const auto stoppedRight=Add(head,YawVector(Sub(right,head),sign*3.6f));
        for(int i=1;i<=360;++i) {
            const double now=4.7+i/90.;const float glance=sign*3.6f+1.5f*std::sin(i*.03f);
            const auto eye=Add(head,{.12f*std::sin(i*.027f),0,.08f*std::sin(i*.019f)});
            const auto l=Add(stoppedLeft,{.0065f*std::sin(i*1.7f),0,.0065f*std::cos(i*1.3f)});
            const auto r=Add(stoppedRight,{.0065f*std::cos(i*1.1f),0,.0065f*std::sin(i*1.9f)});
            anchor.Update(i+403,now,eye,l,r,0,follower.yaw,true,false,glance);
            const float turn=fine.Observe(i+403,now,glance,Sub(l,eye),Sub(r,eye),true);
            follower.Update(i+403,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,anchor.turnOutput,anchor.held,true,
                anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
            Check(std::fabs(BodyAngle(follower.yaw-stopped))<.02f,
                  "absolute heading recovery cannot chase a head glance after the body turn ends");
        }
    }
    {
        ControllerBodyAnchor sway;
        const tr::motiongun::Vec neck{0,0,.12f};
        sway.Update(1,0,head,left,right,0,0,true,false);
        sway.Update(2,.2,head,left,right,0,0,true,false);
        for(int i=1;i<=720;++i) {
            const float angle=.8f*std::sin(i*.02f);const double now=.2+i/90.;
            const auto eye=Add(head,Sub(YawVector(neck,angle),neck));
            const auto l=Add(Sub(head,neck),YawVector(Add(Sub(left,head),neck),angle));
            const auto r=Add(Sub(head,neck),YawVector(Add(Sub(right,head),neck),angle));
            sway.Update(i+2,now,eye,l,r,0,angle,true,false,angle);
            Check(sway.held && Length(Flat(Sub(sway.head,eye)))<.08f,
                  "whole-body rotation carries the held anchor around the neck without cumulative orbit error");
            if(sway.heading.sampleAccepted) Check(Length(Flat(Sub(sway.head,eye)))<.015f,
                  "filtered rigid controller motion carries the body anchor along the head arc within 15 mm");
        }
    }
    // Compare identical controller streams with and without head movement,
    // immediately after turns stop. A settled-idle-only test missed both stale
    // movement checkpoints and the incremental/absolute double application.
    struct TurnRig {
        ControllerBodyAnchor anchor;BodyFollower body;double rate=90;
        void Step(int frame,Vec eye,Vec l,Vec r,float yaw) {
            const double now=frame/rate;
            anchor.Update(frame,now,eye,l,r,0,body.yaw,true,false,yaw);
            body.Update(frame,now,0,anchor.yaw,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,
                anchor.turnOutput,anchor.held,true,anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
        }
    };
    // Arm reach is not root travel. Reproduce the reported foot-sized jump
    // when a turn resumes with a different hand pose, including curved walking
    // where one controller nearly coincides with the instantaneous pivot.
    for(int mode=0;mode<4;++mode) for(float reach:{-.22f,.30f})
        for(float sign:{-1.f,1.f}) for(int direction=0;direction<4;++direction) {
        TurnRig rig;Vec previousEye{},previousBody{},eye{};
        for(int i=0;i<600;++i) {
            float angle=0,extension=0,walk=0;
            if(mode==0) {extension=reach*std::clamp((i-90)/60.f,0.f,1.f);angle=std::max(0,i-150)*.008f;}
            if(mode==1) {
                angle=std::min(std::max(0,i-40),130)*.008f+std::max(0,i-260)*.008f;
                extension=reach*std::clamp((i-180)/60.f,0.f,1.f);
            }
            if(mode==2) {
                angle=std::max(0,i-40)*.008f;extension=reach*std::clamp((i-150)/80.f,0.f,1.f);
                walk=std::clamp((i-250)/100.f,0.f,1.f)*.35f;
            }
            if(mode==3) {
                walk=std::clamp((i-40)/100.f,0.f,1.f)*.35f;
                extension=reach*std::clamp((i-150)/60.f,0.f,1.f);angle=std::max(0,i-220)*.008f;
            }
            angle*=sign;
            const Vec pivot{0,1.7f,-.12f},shift=YawVector(Vec{0,0,walk},direction*1.5707963f);
            auto pose=[&](Vec v) {return Add(Add(pivot,YawVector(Sub(v,pivot),angle)),shift);};
            eye=pose({0,1.7f,0});
            rig.Step(i+1,eye,pose({-.2f,1,.35f+extension}),pose({.2f,1,.35f+extension}),angle);
            if(i>0) {
                Check(Length(Flat(Sub(rig.anchor.head,previousBody)))-Length(Flat(Sub(eye,previousEye)))<.04f,
                    "changed arm reach cannot replay a stale hand offset as a mesh jump");
                Check(Length(Flat(Sub(rig.anchor.head,eye)))<.05f,
                    "changing hand pose during turns and steps cannot accumulate body-anchor drift");
            }
            previousEye=eye;previousBody=rig.anchor.head;
        }
        Check(Length(Flat(Sub(rig.anchor.head,eye)))<.02f,
            "body stays centered after changing arm reach and resuming a physical turn");
    }
    // Translational evidence during an articulated turn must not consume
    // angular progress that the placement gate has not applied yet.
    for(float radius:{0.f,.06f,.12f,.15f}) for(float sign:{-1.f,1.f}) {
        TurnRig rig;Vec eye{};
        for(int i=0;i<=400;++i) {
            const float angle=sign*std::min(i,360)*.01f;
            const float fade=i<=360 ? 1.f : std::max(0.f,1-(i-360)/15.f);
            eye={radius*std::sin(angle),1.7f,radius*(std::cos(angle)-1)};
            const Vec l=Add(eye,YawVector(Vec{-.2f,-.3f,.35f+fade*.07f*std::sin(i*.31f)},angle));
            const Vec r=Add(eye,YawVector(Vec{ .2f,-.3f,.35f+fade*.07f*std::sin(i*.27f)},angle));
            rig.Step(i+1,eye,l,r,angle);
            Check(Length(Flat(Sub(rig.anchor.head,eye)))<.09f,
                "articulated turns around different physical pivots cannot accumulate anchor drift");
        }
        // The 25 mm position checkpoint can leave a small unconsumed arc.
        Check(Length(Flat(Sub(rig.anchor.head,eye)))<.035f,
            "translation fallback preserves pending rotation through arm articulation");
    }
    // A smooth millimetre hand drift can cross an OLD placement checkpoint
    // just after a turn stops. It must not spend the newer headset glance.
    for(double hz:{60.,90.}) for(float tempo:{.1f,1.f}) for(float noise:{.002f,.0065f}) for(float glance:{.03f,.15f,.30f})
        for(float speed:{.002f,.008f,.02f}) for(int stop=30;stop<500;stop+=7) {
        TurnRig rig;rig.rate=hz;Vec eye{},l{},r{};float angle=0;
        for(int i=1;i<=stop;++i) {
            angle=i*speed;const Vec neck{0,1.7f,-.15f};
            eye=Add(neck,YawVector(Vec{0,0,.15f},angle));
            l=Add(neck,YawVector(Vec{-.2f,-.7f,.5f},angle));
            r=Add(neck,YawVector(Vec{ .2f,-.7f,.5f},angle));
            rig.Step(i,eye,l,r,angle);
        }
        const Vec stopped=rig.anchor.head;
        for(int i=1;i<=180;++i) {
            rig.Step(stop+i,Add(eye,Vec{glance*std::sin(i*.08f*tempo),0,glance*.3f*std::sin(i*.04f*tempo)}),
                Add(l,Vec{noise*std::sin(i*.1f),0,noise*(tempo<1 ? std::sin(i*.07f) : std::cos(i*.07f))}),
                Add(r,Vec{noise*(tempo<1 ? std::sin(i*.06f) : std::cos(i*.06f)),0,noise*std::sin(i*.09f)}),angle+.7f*std::sin(i*.1f*tempo));
            Check(Length(Flat(Sub(rig.anchor.head,stopped)))<.00001f,
                "smooth tracked hand drift after a turn cannot spend a newer holster-side head glance");
        }
    }
    // A glance immediately after stopping must not spend the last turn/step's
    // unconsumed controller movement. Include small positional tracking noise.
    for(float speed:{.002f,.008f,.02f}) for(float step:{0.f,.002f})
        for(float noise:{0.f,.002f}) for(int stop=30;stop<500;stop+=7) {
        TurnRig rig;Vec eye{},l{},r{};float angle=0;
        for(int i=1;i<=stop;++i) {
            angle=i*speed;
            const Vec neck{step*i,1.7f,-.15f};
            eye=Add(neck,YawVector(Vec{0,0,.15f},angle));
            l=Add(neck,YawVector(Vec{-.2f,-.7f,.5f},angle));
            r=Add(neck,YawVector(Vec{ .2f,-.7f,.5f},angle));
            rig.Step(i,eye,l,r,angle);
        }
        const Vec stopped=rig.anchor.head;Vec settled{};
        for(int i=1;i<=90;++i) {
            const Vec glance=Add(eye,Vec{.03f*std::sin(i*.12f),0,.03f*std::sin(i*.05f)});
            rig.Step(stop+i,glance,
                Add(l,Vec{noise*std::sin(i*.8f),0,noise*std::cos(i*.7f)}),
                Add(r,Vec{noise*std::cos(i*.6f),0,noise*std::sin(i*.9f)}),angle+.3f*std::sin(i*.1f));
            Check(Length(Flat(Sub(rig.anchor.head,stopped)))<(noise==0 ? .00001f : .009f),
                "head-only glance cannot reuse stale turn or walking evidence after hands stop");
            if(i==12) settled=rig.anchor.head;
            if(i>12) Check(Length(Flat(Sub(rig.anchor.head,settled)))<.00001f,
                "settled controller noise cannot revive pending motion during sideways glances");
        }
    }
    // Straight and curved walking must survive natural opposing arm swing.
    // Rotation samples must not consume the separate movement observation.
    for(float speed:{0.f,.002f,.008f}) for(float swing:{0.f,.01f,.025f}) for(int direction=0;direction<4;++direction) {
        TurnRig rig;int permitted=0;float previousStep=0;
        for(int i=0;i<480;++i) {
            const float angle=i*speed;
            const Vec shift=YawVector(Vec{0,0,i*.002f},direction*1.5707963f),neck{0,1.7f,-.15f};
            const Vec eye=Add(Add(neck,YawVector(Vec{0,0,.15f},angle)),shift);
            const Vec l=Add(Add(neck,YawVector(Vec{-.2f,-.7f,.45f+swing*std::sin(i*.13f)},angle)),shift);
            const Vec r=Add(Add(neck,YawVector(Vec{ .2f,-.7f,.45f-swing*std::sin(i*.13f)},angle)),shift);
            const float before=rig.body.yaw;
            rig.Step(i+1,eye,l,r,angle);
            const float step=BodyAngle(rig.body.yaw-before);
            if(i>120 && rig.anchor.translating) ++permitted;
            if(i>220) {
                Check(Length(Flat(Sub(rig.anchor.head,eye)))<.055f,
                    "combined turn and translation cannot double-count travel or leave the mesh far from the player");
                if(speed==.008f && swing==.01f) Check(std::fabs(step-previousStep)<.011f,
                    "natural small arm swings cannot turn sustained body rotation into stop-start angular bursts");
            }
            previousStep=step;
        }
        Check(permitted>340,"forward backward left and right walking remains responsive during turns and modest arm swing");
    }
    // Reproduce the in-session stall after hand spacing/reach changes, plus
    // the stop/start cadence of an otherwise perfectly steady physical turn.
    for(float speed:{.002f,.008f,.02f}) for(float sign:{-1.f,1.f}) for(int scenario=0;scenario<4;++scenario) {
        TurnRig rig;
        for(int i=0;i<=720;++i) {
            const float angle=sign*i*speed;
            const float span=scenario==1 || (scenario==2 && i>100) ? .075f : .4f;
            const float reach=.3f+(scenario==3 && i>100 ? .16f : 0.f);
            const auto l=YawVector(Vec{-span*.5f,1,reach},angle);
            const auto r=YawVector(Vec{ span*.5f,1,reach},angle);
            const float before=rig.body.yaw;
            rig.Step(i+1,head,l,r,angle);
            if(i>400) {
                Check(std::fabs(BodyAngle(rig.body.yaw-angle))<.12f,
                    "physical turn reacquires changed arm geometry and close hands without becoming stuck facing backward");
                const float step=sign*BodyAngle(rig.body.yaw-before);
                Check(step>speed*.65f && step<speed*1.35f,
                    "steady physical turns render continuously instead of stopping between controller checkpoints");
            }
        }
    }
    for(float span:{.075f,.12f}) {
        TurnRig rig;const Vec l{-span*.5f,1,.3f},r{span*.5f,1,.3f};
        rig.Step(1,head,l,r,0);
        for(int i=1;i<=720;++i) {
            const float glance=1.5f*std::sin(i*.03f);
            const auto eye=Add(head,{.12f*std::sin(i*.027f),0,.08f*std::sin(i*.019f)});
            rig.Step(i+1,eye,Add(l,{.0065f*std::sin(i*1.7f),0,.0065f*std::cos(i*1.3f)}),
                Add(r,{.0065f*std::cos(i*1.1f),0,.0065f*std::sin(i*1.9f)}),glance);
            Check(std::fabs(rig.body.yaw)<.00001f && Length(Flat(Sub(rig.anchor.head,head)))<.00001f,
                "closely spaced controller noise cannot authorize head-only mesh movement");
        }
    }
    for(float sign:{-1.f,1.f}) for(int stop=20;stop<=400;stop+=10) for(float noise:{0.f,.002f,.0065f}) {
        TurnRig glance;
        Vec l=left,r=right;
        for(int frame=1;frame<=stop;++frame) {
            const float yaw=sign*std::max(0,frame-20)*.01f;
            l=Add(head,YawVector(Add(Sub(left,head),{0,0,.07f*std::sin(frame*.31f)}),yaw));
            r=Add(head,YawVector(Add(Sub(right,head),{0,0,.07f*std::cos(frame*.27f)}),yaw));
            glance.Step(frame,head,l,r,yaw);
        }
        auto still=glance;const float start=sign*(stop-20)*.01f;
        float settled=0,movedGlance=0,movedStill=0,previousGlance=glance.body.yaw,previousStill=still.body.yaw;
        for(int i=1;i<=360;++i) {
            const auto eye=Add(head,{.03f*std::sin(i*.025f),0,.03f*std::sin(i*.019f)});
            const auto nl=Add(l,{noise*std::sin(i*1.7f),0,noise*std::cos(i*1.3f)});
            const auto nr=Add(r,{noise*std::cos(i*1.1f),0,noise*std::sin(i*1.9f)});
            glance.Step(stop+i,eye,nl,nr,start+.2f*std::sin(i*.035f));
            still.Step(stop+i,head,nl,nr,start);
            movedGlance+=std::fabs(BodyAngle(glance.body.yaw-previousGlance));
            movedStill+=std::fabs(BodyAngle(still.body.yaw-previousStill));
            previousGlance=glance.body.yaw;previousStill=still.body.yaw;
            Check(movedGlance<=movedStill+.00001f,
                "a head glance may replant sooner but cannot add mesh rotation to the same controller stream");
            if(i==90) settled=glance.body.yaw;
            if(i>90) Check(std::fabs(BodyAngle(glance.body.yaw-settled))<.00001f,
                "stationary controller noise cannot keep rotating the body after the accepted turn settles");
        }
    }
    // A glance plus small deliberate hand adjustments is still a planted
    // stance. Validate that separately from zero-motion and tracking-noise cases.
    // Real turns also carry the held anchor around nonzero physical pivots.
    for(float sign:{-1.f,1.f}) for(float radius:{.08f,.12f,.22f}) for(float adjustment:{.06f,.12f,.18f}) {
        TurnRig rig;int frame=0;
        const Vec pivot{-.05f,0,-radius};
        auto rotate=[&](Vec v,float angle) {return Add(pivot,YawVector(Sub(v,pivot),angle));};
        for(int i=0;i<=720;++i) {
            const float angle=sign*i*6.28318530718f/720;
            const auto eye=rotate(head,angle);
            rig.Step(++frame,eye,rotate(left,angle),rotate(right,angle),angle);
            if(i>=90) {
                Check(Length(Flat(Sub(rig.anchor.head,eye)))<.03f,
                    "roomscale quarter and half turns keep the body centered without requiring a full revolution");
                Check(std::fabs(BodyAngle(rig.body.yaw-angle))<.12f,
                    "sustained physical turns retain facing after the planted-stance decision");
            }
        }
        for(int i=0;i<90;++i) rig.Step(++frame,head,left,right,0);
        const float facing=rig.body.yaw;const Vec planted=rig.anchor.head;
        for(int i=1;i<=720;++i) {
            const float glance=.7f*std::sin(i*.025f),hands=adjustment*std::sin(i*.025f);
            rig.Step(++frame,rotate(head,glance),rotate(left,hands),rotate(right,hands),glance);
            Check(std::fabs(BodyAngle(rig.body.yaw-facing))<.00001f && Length(Flat(Sub(rig.anchor.head,planted)))<.00001f,
                "head glances with centimetres of hand adjustment retain the planted mesh position and facing");
        }
    }
    // The native/IK footprint must follow the accepted moving pivot, not
    // accumulate an orbit around a sequence of different pivot positions.
    for(float sign:{-1.f,1.f}) {
        ControllerBodyAnchor anchor;BodyFollower body;
        const Vec pivot{0,0,-.12f},local{.035f,0,.14f};
        for(int i=0;i<=1080;++i) {
            const float angle=sign*i*6.2831853f/720;const double now=i/90.;
            auto rotate=[&](Vec v) {return Add(pivot,YawVector(Sub(v,pivot),angle));};
            anchor.Update(i+1,now,rotate(head),rotate(left),rotate(right),0,body.yaw,true,false,angle);
            const auto goal=Sub(anchor.head,YawVector(local,body.yaw));
            body.Update(i+1,now,0,anchor.yaw,0,goal,{},false,tr::firstperson::BodyFollowSettings(),anchor.head,false,true,0,anchor.held,true,
                anchor.heading.active ? anchor.heading.target : NAN,anchor.rotating);
            if(anchor.held) Check(Length(Sub(body.root,Sub(Flat(anchor.head),YawVector(local,body.yaw))))<.00001f,
                "leg footprint and pelvis share the accepted pivot through repeated physical turns");
        }
    }
    for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor walk;walk.Update(1,0,head,left,right,0,0,true,false);
        walk.Update(2,.2,head,left,right,0,0,true,false);
        Check(!walk.translating,"settled tracking does not grant permission for gameplay movement");
        for(int i=1;i<=180;++i) {
            const float glance=.45f*std::sin(i*.04f);
            const auto step=YawVector({0,0,i*.002f},direction*1.5707963f);
            const auto eye=Add(Add(head,step),Sub(YawVector({0,0,.15f},glance),{0,0,.15f}));
            walk.Update(i+2,.2+i/90.,eye,Add(left,step),Add(right,step),0,0,true,false,glance);
            if(i>=15) Check(Length(Sub(Flat(walk.head),step))<.025f && !walk.held,
                "forward backward and sideways steps remain responsive while the head glances independently");
        }
    }
    // Recorded failure pattern: a settled sideways gaze with unequal hand
    // adjustments must not masquerade as a physical step. The short-window
    // head yaw can be zero even though the player is clearly looking aside.
    for(float gaze:{-1.2f,1.2f}) for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor planted;
        planted.Update(1,0,head,left,right,0,0,true,false,gaze);
        planted.Update(2,.2,head,left,right,0,0,true,false,gaze);
        for(int i=1;i<=20;++i) {
            const Vec axis=YawVector(Vec{i/20.f,0,0},direction*1.5707963f);
            planted.Update(i+2,.2+i/90.,Add(head,Scale(axis,.03f)),
                Add(left,Scale(axis,.041f)),Add(right,Scale(axis,.010f)),0,0,true,false,gaze);
            Check(planted.held && Length(Flat(Sub(planted.head,head)))<.00001f,
                "a settled sideways gaze and unequal small hand adjustments leave the inferred feet planted");
        }
    }
    ControllerBodyAnchor slow;slow.Update(1,0,head,left,right,0,0,true,false);
    for(int i=1;i<=90;++i) {
        const tr::motiongun::Vec shift{.002f*i,0,0};
        slow.Update(i+1,i/90.,Add(head,shift),Add(left,shift),Add(right,shift),0,0,true,false);
        if(i>=15) Check(!slow.held,"slow cumulative controller translation is not mistaken for stationary tracking noise");
    }
    a.Update(106,2.82,head,left,right,.4f,0,false,false);
    Check(!a.valid && !a.held && Length(a.offset)==0,"tracking loss cannot retain a stale body anchor");
}


int main() {TestControllerBodyAnchor();std::printf("PASS: %d anchor checks\n",checks);}
