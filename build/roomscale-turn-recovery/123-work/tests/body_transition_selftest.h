void TestHiddenNeckSkin() {
    using namespace tr::motiongun;
    for(int count:{15,32}) for(float yaw:{0.f,.8f,3.14f}) for(float size:{.85f,1.f}) {
        int32_t mapping[33]{};for(int i=0;i<count;++i) mapping[i]=i<15?i:14;
        std::swap(mapping[7],mapping[3]); // Palette order is not joint order.
        const auto visible=tr::firstperson::SkinJointMask(0x3fff,mapping,count);
        float palette[33*12]{},before[33*12]{};
        for(int frame=0;frame<=100;++frame) {
            auto basis=tr::firstperson::RotationMatrix(tr::firstperson::AxisRotation({0,1,0},yaw));
            for(auto& row:basis.r) for(auto& value:row) value*=size;
            const Frame chest{basis,{10,100.f-frame,30}};
            const Frame head{tr::firstperson::IdentityBasis(),{-30,-200-float(frame*3),-100}};
            for(int i=0;i<count;++i) WriteRows(mapping[i]==14?head:chest,palette+i*12);
            std::memcpy(before,palette,sizeof(before));
            Check(tr::firstperson::StabilizeHiddenNeck(palette,count,mapping,visible),"hidden collar correction handles mapped body palettes");
            const tr::motiongun::Vec vertex{12,-20,17};
            for(int i=0;i<count;++i) {
                if(mapping[i]!=14) Check(!std::memcmp(palette+i*12,before+i*12,12*sizeof(float)),"neck correction preserves every visible bone");
                else for(float weight:{.01f,.25f,.49f}) {
                    const auto mixed=Add(Scale(Transform(ReadRows(palette+i*12),vertex),weight),Scale(Transform(chest,vertex),1-weight));
                    Check(tr::firstperson::Length(Sub(mixed,Transform(chest,vertex)))<.001f,"mixed collar vertices cannot spike toward the independently moving hidden head");
                }
            }
            float repeat[33*12];std::memcpy(repeat,palette,sizeof(palette));
            tr::firstperson::StabilizeHiddenNeck(palette,count,mapping,visible);
            Check(!std::memcmp(repeat,palette,sizeof(palette)),"hidden collar correction is idempotent across stereo draws");
            for(auto mask:{tr::firstperson::SkinJointMask(0x7fff,mapping,count),tr::firstperson::SkinJointMask(0x3f00,mapping,count)}) {
                std::memcpy(palette,before,sizeof(palette));
                Check(!tr::firstperson::StabilizeHiddenNeck(palette,count,mapping,mask) && !std::memcmp(palette,before,sizeof(palette)),
                      "visible heads and arms-only attachments keep their original palettes");
            }
        }
    }
}

namespace {
#include "sidearm_animation_steps.inl"
void TestBodyCenterReference() {
    using namespace tr;
    for(float base:{0.f,.7f,1.5707963f,-2.6f}) for(float capturedSide:{-44.f,44.f})
    for(float anchorX:{0.f,13.f}) {
        stabilization::GroundEye reference;
        const float bodyYaw=base+.6f;
        const auto animatedFlat=locomotion::Rotate({capturedSide,133},bodyYaw);
        const stabilization::Point root{100,500,300};
        const stabilization::Point animated{root.x+animatedFlat.x,root.y-700,root.z+animatedFlat.z};
        const auto eye=reference.Apply(root,base,animated,bodyYaw,false,anchorX);
        Check(std::fabs(reference.bodyLocal.x-anchorX)<.001f && std::fabs(reference.bodyLocal.z-133)<.001f,
              "first-frame neck sway cannot become a permanent sideways body offset");
        Check(std::fabs(eye.x-animated.x)<.001f && std::fabs(eye.y-animated.y)<.001f && std::fabs(eye.z-animated.z)<.001f,
              "body centering does not move the calibrated scene camera");
        for(float turn:{0.f,.8f,3.1415926f,6.2831853f}) {
            const float yaw=bodyYaw+turn;
            const auto fromRoot=locomotion::Rotate({reference.local.x,reference.local.z},base+turn);
            const auto bodyOffset=reference.BodyOffsetAtEye(base+turn,yaw,{},{},423,fromRoot);
            const auto relative=locomotion::Rotate(fromRoot-bodyOffset,-yaw);
            Check(std::fabs(relative.x-anchorX)<.001f && std::fabs(relative.z-133)<.001f,
                  "centered reference survives stick and physical turns without requiring a full revolution");
        }
        const auto previous=reference.local;
        reference.Resume(true,base,base+1.1f,0);
        reference.Apply(root,base+1.1f,{animated.x+80,animated.y+20,animated.z-60},bodyYaw,false,anchorX);
        Check(reference.bodyLocal.x==anchorX && reference.local.y==previous.y,
              "runtime recenter preserves centered lateral fit and calibrated height");
    }
}

void TestControllerBodyAnchor() {
    using namespace tr::firstperson;
    ControllerBodyAnchor a;
    const tr::motiongun::Vec left{-.2f,1,.3f},right{.2f,1,.3f},head{0,1.7f,0};
    a.Update(1,1,head,left,right,0,0,true,false);
    a.Update(2,1.15,head,left,right,0,0,true,false);
    Check(a.held,"stationary controller positions arm body hold after settling");
    for(int frame=3;frame<100;++frame) {
        const tr::motiongun::Vec eye{.12f*std::sin(frame*.1f),1.7f,.10f*std::cos(frame*.1f)};
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
        release.Update(4,.22,Add(head,step),Add(left,step),Add(right,step),0,0,true,false);
        BodyFollower facing;
        facing.Update(1,0,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings());
        facing.Update(2,.02,0,1.5f,0,step,{},false,tr::firstperson::BodyFollowSettings(),{},false,true,0,false,true);
        Check(std::fabs(facing.yaw)<.00001f,"translation release cannot snap the held body toward a head-only glance");
        facing.Update(3,.03,0,1.6f,0,step,{},false,tr::firstperson::BodyFollowSettings(),{},false,true,.1f,false,true);
        Check(std::fabs(facing.yaw-.1f)<.00001f,"controller-owned facing still accepts corroborated physical turns");
        const auto footprint=Sub(Add(head,step),release.offset);
        Check(Length(Flat(Sub(footprint,Add(head,step))))<.00001f,
              "physical release carries only controller movement, never accumulated head lean");
        for(int frame=5;frame<20;++frame) {
            const auto eye=Add(head,{.15f+frame*.01f,0,0});
            release.Update(frame,.22+(frame-4)/90.,eye,Add(left,step),Add(right,step),0,0,true,false);
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
    a.Update(101,2.41,Add(head,{.04f,0,0}),Add(left,{.04f,0,0}),Add(right,{.04f,0,0}),0,0,true,false);
    Check(!a.held,"coordinated physical head and controller translation releases the hold");
    a.Update(102,2.6,Add(head,{.04f,0,0}),Add(left,{.04f,0,0}),Add(right,{.04f,0,0}),0,0,true,false);
    Check(a.held,"controllers can settle again after motion");
    a.Update(103,2.61,head,left,right,.4f,0,true,false);
    Check(!a.held && Length(a.offset)==0,"stick turning discards the physical hold without stale correction");
    a.Update(104,2.8,head,left,right,.4f,0,true,false);
    a.Update(105,2.81,head,left,right,.4f,0,true,true);
    Check(!a.held,"stick travel remains available with physically stationary controllers");
    ControllerBodyAnchor turning;BodyFollower follower;PhysicalBodyTurn signal;
    turning.Update(1,0,head,left,right,0,0,true,false);
    turning.Update(2,.3,head,left,right,0,0,true,false);
    signal.Observe(2,.3,0,Sub(left,head),Sub(right,head),true);
    follower.Update(2,.3,0,0,0,{}, {},false,tr::firstperson::BodyFollowSettings{});
    bool released=false;
    for(int i=1;i<=100;++i) {
        const float angle=i*.005f;const double now=.3+i/90.;
        const auto l=Add(head,YawVector(Sub(left,head),angle)),r=Add(head,YawVector(Sub(right,head),angle));
        turning.Update(i+2,now,head,l,r,0,follower.yaw,true,false,angle);
        const auto turn=signal.Observe(i+2,now,angle,Sub(l,head),Sub(r,head),true);
        follower.Update(i+2,now,0,angle,0,{}, {},false,tr::firstperson::BodyFollowSettings{},{},false,true,turn,turning.held);
        released=released || !turning.held;
        Check(std::fabs(follower.yaw-(turning.held ? 0.f : angle))<.0001f,
              "physical turns retain corroborated yaw across the stationary noise threshold");
    }
    Check(released,"deliberate physical turning releases the controller hold");
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
            follower.Update(i+2,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,turn,jitter.held,true);
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
            follower.Update(i+2,now,0,angle,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,turn,hold.held,true);
            Check(std::fabs(BodyAngle(follower.yaw-angle))<(lag ? .35f : .12f),
                  "noisy real roomscale turns retain their rotation through the stationary-controller hold");
        }
        const float final=sign*speed*360,bodyBefore=follower.yaw;
        Check(std::fabs(BodyAngle(bodyBefore-final))<.04f,"roomscale turn ends without accumulating missing degrees");
        for(int i=1;i<=90;++i) {
            const double now=4.2+i/90.;const float glance=final+i*.015f;
            hold.Update(362+i,now,head,l,r,0,follower.yaw,true,false,glance);
            const float turn=signal.Observe(362+i,now,glance,Sub(l,head),Sub(r,head),true);
            follower.Update(362+i,now,0,glance,0,{}, {},false,tr::firstperson::BodyFollowSettings(),{},false,true,turn,hold.held,true);
            Check(std::fabs(BodyAngle(follower.yaw-bodyBefore))<.02f,
                  "a head glance after a real body turn cannot continue rotating the body");
        }
    }
    ControllerBodyAnchor slow;slow.Update(1,0,head,left,right,0,0,true,false);
    for(int i=1;i<=90;++i) {
        const tr::motiongun::Vec shift{.002f*i,0,0};
        slow.Update(i+1,i/90.,Add(head,shift),Add(left,shift),Add(right,shift),0,0,true,false);
        Check(!slow.held,"slow cumulative controller translation is not mistaken for stationary tracking noise");
    }
    a.Update(106,2.82,head,left,right,.4f,0,false,false);
    Check(!a.valid && !a.held && Length(a.offset)==0,"tracking loss cannot retain a stale body anchor");
}

void TestPhysicalBodyTurn() {
    TestControllerBodyAnchor();
    using namespace tr::firstperson;
    const tr::firstperson::Vec left{-.2f,0,.35f},right{.2f,0,.35f},foot{0,0,-.15f};
    tr::firstperson::BodyFollowSettings settings;
    for(float direction:{-1.f,1.f}) for(float glance:{0.f,.4f}) {
        PhysicalBodyTurn signal;BodyFollower physical,analog,mixed;
        physical.Update(1,1,0,0,0,foot,{},false,settings,{},false,true,0,false);
        analog=physical;mixed=physical;
        signal.Observe(1,1,glance,left,right,true);
        float angle=0;
        for(int frame=2;frame<1400;++frame) {
            const float step=(frame<700 ? direction : -direction)*.012f;
            angle+=step;const double now=1+double(frame-1)/90;
            const float observed=signal.Observe(frame,now,BodyAngle(angle+glance),YawVector(left,angle),YawVector(right,angle),true);
            Check(std::fabs(observed-step)<.00001f,"coherent physical turn crosses wrap and reverses without losing heading");
            physical.Update(frame,now,0,BodyAngle(angle+glance),0,YawVector(foot,physical.yaw),{},false,settings,{},false,true,observed,false);
            analog.Update(frame,now,angle,BodyAngle(angle+glance),0,YawVector(foot,analog.yaw),{},false,settings,{},false,true,0,false);
            mixed.Update(frame,now,angle*.5f,BodyAngle(angle*1.5f+glance),0,YawVector(foot,mixed.yaw),{},false,settings,{},false,true,observed,false);
            Check(std::fabs(BodyAngle(physical.yaw-analog.yaw))<.00005f && Length(Sub(physical.root,analog.root))<.00005f,
                  "roomscale and analog turns keep identical body heading and centered footprint");
            Check(std::fabs(BodyAngle(physical.yaw-angle))<.00005f,"physical turn retains the existing head-only glance");
            Check(std::fabs(BodyAngle(mixed.yaw-angle*1.5f))<.0001f,"simultaneous physical and analog turns each apply once");
            const auto root=physical.root;const float yaw=physical.yaw;
            physical.Update(frame,now,0,angle+glance,0,root,{},false,settings,{},false,true,observed,false);
            Check(physical.yaw==yaw && Length(Sub(root,physical.root))==0,"stereo and accessory passes cannot repeat a physical turn");
        }
    }
    for(int mode=0;mode<8;++mode) {
        PhysicalBodyTurn signal;signal.Observe(1,1,0,left,right,true);
        float head=.1f;tr::firstperson::Vec a=YawVector(left,.1f),b=YawVector(right,.1f);double now=1.01;bool tracked=true;
        if(mode==0) {a=left;b=right;} // Head-only glance.
        if(mode==1) head=0; // Hands-only gesture.
        if(mode==2) b=right; // One hand follows the head.
        if(mode==3) tracked=false;
        if(mode==4) now=1.5;
        if(mode==5) {head=1;a=YawVector(left,1);b=YawVector(right,1);}
        if(mode==6) a={0,0,.01f};
        if(mode==7) a=Scale(a,2);
        Check(signal.Observe(2,now,head,a,b,tracked)==0,"glances, gestures and tracking discontinuities do not drag the body");
    }
    PhysicalBodyTurn lost;lost.Observe(1,1,0,left,right,true);
    lost.Observe(2,1.01,0,left,right,false);
    Check(lost.Observe(3,1.02,1,YawVector(left,1),YawVector(right,1),true)==0,"tracking recovery seeds without replaying a turn");
    Check(std::fabs(lost.Observe(4,1.03,1.1f,YawVector(left,1.1f),YawVector(right,1.1f),true)-.1f)<.00001f,"physical following resumes after tracking recovery");
    BodyFollower moving;moving.Update(1,1,0,0,0,{}, {},false,settings);
    moving.Update(2,1.01,0,.1f,.2f,{}, {},true,settings,{},false,true,.1f);
    Check(std::fabs(moving.yaw-.2f)<.00001f,"travel heading remains authoritative while moving");
}

void TestForwardJumpTorso() {
    using namespace tr::firstperson;using Vec=tr::motiongun::Vec;
    const Vec points[15]={{0,-380,0},{-45,-380,0},{-45,-200,50},{-45,0,0},
        {45,-380,0},{45,-200,50},{45,0,0},{0,-428,14},
        {90,-580,0},{125,-460,20},{90,-320,50},{-90,-580,0},{-125,-460,20},{-90,-320,50},{0,-650,0}};
    Frame standing[15]{};for(int j=0;j<15;++j) standing[j]={IdentityBasis(),points[j]};
    int model=0;GroundTorso reference;
    Check(reference.Apply(&model,standing,IdentityBasis(),{0,-682,140},423,0,true),"capture grounded torso for forward-jump collar protection");
    for(float yaw:{0.f,.7f,-2.4f}) for(float pitch:{-.9f,-.3f,-.1f,0.f,.1f,.3f,.7f,.9f}) {
        const Basis turn=RotationMatrix(AxisRotation({0,1,0},yaw));
        Frame pose[15]{},before[15]{};
        for(int j=0;j<15;++j) pose[j]=Multiply({turn,{100,0,200}},standing[j]);
        const auto bow=TorsoClearance::Bend(pose[0].origin,{std::cos(yaw),0,-std::sin(yaw)},pitch);
        for(int j=7;j<15;++j) pose[j]=Multiply(bow,pose[j]);
        std::memcpy(before,pose,sizeof(pose));
        Check(reference.HoldForwardJump(&model,pose,turn,423),"forward-jump upright reference accepts live animation");
        Frame inverse{};Inverse({turn,{}},inverse);
        const auto q=RotationOf(Multiply(inverse.basis,pose[7].basis));
        const float angle=2*std::atan2(Length(q.v),std::fabs(q.w));
        Check(angle<=.0001f,"forward jump cannot bow the exposed collar far away from the upright reference");
        for(int j:{0,1,2,3,4,5,6,14}) Check(!std::memcmp(&pose[j],&before[j],sizeof(Frame)),"collar protection leaves native legs and hidden head unchanged");
        Check(std::fabs(Length(Sub(pose[7].origin,pose[0].origin))-Length(Sub(before[7].origin,before[0].origin)))<.001f,
              "jump torso protection preserves spine length");
        for(int shoulder:{8,11}) Check(std::fabs(Length(Sub(pose[shoulder].origin,pose[7].origin))-Length(Sub(before[shoulder].origin,before[7].origin)))<.001f,
              "forward-jump shoulder sockets follow the same rigid torso correction");
        if(pitch==0) for(int j=7;j<14;++j) Check(Length(Sub(pose[j].origin,before[j].origin))<.001f,
              "an already upright jump torso remains unmodified");
    }
}

void TestUprightTorsoFit() {
    TestHiddenNeckSkin();
    TestPhysicalBodyTurn();
    TestForwardJumpTorso();
    using namespace tr::firstperson;using Vec=tr::motiongun::Vec;
    const Vec points[15]={{0,-380,0},{-45,-380,0},{-45,-200,50},{-45,0,0},
        {45,-380,0},{45,-200,50},{45,0,0},{0,-428,14},
        {90,-580,0},{125,-460,20},{90,-320,50},{-90,-580,0},{-125,-460,20},{-90,-320,50},{0,-650,0}};
    Frame native[15]{};for(int j=0;j<15;++j) native[j]={IdentityBasis(),points[j]};
    const Vec eye{0,-682,140};const float units=423;
    BodySizeCalibration size;
    Check(size.Capture(native,eye,eye.y,units),"upright torso fixture captures eye/floor height");
    const float original=size.factor;
    Check(size.FitTorso(native,{-67,-199,-92},{69,-33,64},eye.y),"visible torso refines the upright body fit");
    Check(size.factor<original && size.factor>=original*.85f && size.eyeGap>0,
          "visual body is proportionally fitted within a fifteen-percent refinement");
    const float fitted=size.factor,reserve=size.eyeGap;
    Check(!size.FitTorso(native,{-67,-500,-92},{69,-33,64},eye.y+100) && size.factor==fitted && size.eyeGap==reserve,
          "looking down cannot repeatedly shrink the calibrated body");
    auto solve=[&](float factor,float gap,Vec target) {
        Frame scaled[15]{},posed[15]{};ScaleBody(native,factor,scaled);
        Frame inverse{};Inverse(native[14],inverse);
        const Vec anchor=Transform(scaled[14],Transform(inverse,eye));
        Check(SolveFollowingBody(scaled,anchor,target,0,IdentityBasis(),{0,0,1},units,{},0,true,0,0,posed,1,gap),
              "fitted upright pose solves with the actual eye target");
        std::array<Frame,15> out{};for(int j=0;j<15;++j) out[j]=posed[j];return out;
    };
    const auto old=solve(original,0,eye),upright=solve(fitted,reserve,eye);
    Check(upright[7].origin.y>old[7].origin.y+units*.05f,
          "the corrected standing chest remains lower instead of lifting toward the headset");
    for(float drop:{0.f,.05f,.2f}) {
        const Vec target=Add(eye,{0,drop*units,0});const auto pose=solve(fitted,reserve,target);
        for(int first:{1,4}) {
            Check(std::fabs(pose[first+2].origin.y)<.1f,"upright fitting leaves both feet on the original floor");
            const float reach=Length(Sub(pose[first+1].origin,pose[first].origin))+Length(Sub(pose[first+2].origin,pose[first+1].origin));
            Check(Length(Sub(pose[first+2].origin,pose[first].origin))>reach*.999f,
                  "lower torso fit retains nearly straight standing legs");
        }
        Check(std::fabs(pose[7].origin.y-upright[7].origin.y)<.1f,
              "the standing chest does not rise as the tracked eyes move down");
        Check(std::fabs(pose[14].origin.y-(target.y+(682-650)*fitted))<.1f,
              "visual fit does not move the tracked head target");
    }
}

void TestStandingCalibration() {
    TestUprightTorsoFit();
    TestBodyCenterReference();
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) {
        BodyRig(game,false);testConfig.firstPersonFullBodyIK=true;testConfig.firstPersonBodyScale=0;
        testConfig.firstPersonMovementStabilization=true;testConfig.firstPersonBodyForwardCm=11;
        g_holsters.active=true;g_holsters.weapon=1;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_autoBodyCalibration={};g_heading.base=0;g_haveManualInput=false;g_bodySkinScope=true;
        auto* obj=Ptr<uint8_t>(dll.objects);
        auto render=[&] {
            float palette[384];std::memcpy(palette,ikNative,sizeof(palette));++testVR.m_poseSerial;
            Check(ApplyFullBodyIK(item,palette,15),"standing calibration renders full body");
            std::array<Frame,15> pose{};
            for(int j=0;j<15;++j) ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,pose[j]);
            return pose;
        };
        {
            const float savedYaw=testHeadYaw;
            const auto savedLeft=testControllerOffset[0],savedRight=testControllerOffset[1];
            auto setTracking=[&](float yaw) {
                testHeadYaw=yaw;
                for(int hand=0;hand<2;++hand) {
                    const auto v=firstperson::YawVector({hand ? .2f : -.2f,0,.35f},yaw);
                    testControllerOffset[hand]={testViewRight+v.x,.3f,testViewForward+v.z};
                }
            };
        const float savedHandWeight=testConfig.firstPersonBodyHandYawWeight;
        for(float weight:{0.f,.15f}) {
            testConfig.firstPersonBodyHandYawWeight=weight;
            setTracking(0);ResetBodyFollower();render();
            for(int frame=1;frame<=60;++frame) {
                const float yaw=frame*.005f;setTracking(yaw);
                g_bodyFollower.physicalTurn.time=TurnTime()-1./90;
                const auto camera=g_scenePose;
                render();
                Check(std::fabs(firstperson::BodyAngle(g_bodyFollower.yaw-(weight>0 ? yaw : 0.f)))<.0001f,
                      "production mesh follows physical turns and respects disabled controller support");
                Check(!std::memcmp(&camera,&g_scenePose,sizeof(camera)),"physical mesh heading never rotates or translates the camera");
            }
        }


        testConfig.firstPersonBodyHandYawWeight=.15f;
        for(int holdGunStatus:{0,4}) {
        *Ptr<int16_t>(dll.lara+2)=int16_t(holdGunStatus);
        setTracking(0);ResetBodyFollower();render();
        g_controllerBodyAnchor.moved=TurnTime()-.2;
        const auto heldPose=render();
        Check(BodyControllersStationary(),"render fixture holds actual stationary controller positions");
        const auto heldLeftPose=testControllerPose[0],heldRightPose=testControllerPose[1];
        const auto sceneBefore=g_scenePose;
        for(int frame=0;frame<180;++frame) {
            const float yaw=1.5f*std::sin(frame*.4f),dx=.35f*std::sin(frame*.04f),dz=.25f*std::cos(frame*.04f);
            testViewRise=-.25f;testPostureNeckDrop=.137752f;testHeadYaw=yaw;testViewRight=dx;testViewForward=dz;testVR.m_rawHeadPose.m[0][3]=dx;testVR.m_rawHeadPose.m[2][3]=-dz;
            testControllerPose[0]=heldLeftPose;testControllerPose[1]=heldRightPose;
            testControllerPose[0].m[0][3]+=(frame%2 ? .0065f : -.0065f);
            testControllerPose[1].m[0][3]-=(frame%2 ? .0065f : -.0065f);
            if(frame==10) g_bodyFollower.Reset(); // Native jump/landing can restart the stance solver.
            g_torsoPose.time=TurnTime()-1./90;
            const auto pose=render();
            Check(BodyControllersStationary(),"head movement does not release fixed controller positions");
            Check(locomotion::Length(DragBody(item))==0,"stationary controllers prevent head-only roomscale root movement");
            for(int joint:{0,7}) Check(firstperson::Length(firstperson::Flat(Sub(pose[joint].origin,heldPose[joint].origin)))<.1f,
                  "head translation and rotation cannot slide pelvis or chest while controllers remain fixed in space");
            Check(std::fabs(g_bodyFollower.yaw)<.0001f,"stationary controllers freeze physical body yaw");
            for(int hand=0;hand<2;++hand) {
                Frame wrist{};Basis controller{};Check(BuildControllerWrist(hand,wrist,controller),"body hold retains controller targets");
                Check(firstperson::Length(Sub(pose[hand ? 10 : 13].origin,wrist.origin))<.1f,
                      "stationary body retains exact tracked hands");
            }
            Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)),"body hold never alters the camera anchor");
        }
        testHeadYaw=0;testViewRight=testViewForward=0;testVR.m_rawHeadPose.m[0][3]=testVR.m_rawHeadPose.m[2][3]=0;
        testViewRise=0;testPostureNeckDrop=0;ResetControllerBodyAnchor();
        testControllerPose[0]=heldLeftPose;testControllerPose[1]=heldRightPose;
        }
        *Ptr<int16_t>(dll.lara+2)=0;
        testConfig.firstPersonBodyHandYawWeight=savedHandWeight;
            testHeadYaw=savedYaw;testControllerOffset[0]=savedLeft;testControllerOffset[1]=savedRight;
        }
        {
            const auto savedBits=Field<uint32_t>(off::item_mesh_bits);
            Field<uint32_t>(off::item_mesh_bits)=0x3fff;
            const float savedHeadDelta=ikNative[14*12+3];
            for(int frame=0;frame<=40;++frame) {
                testViewRise=-.4f+frame*.01f;g_torsoPose.time=TurnTime()-1./90;render();
                ikNative[14*12+3]=123;
                const auto sceneBefore=g_scenePose;
                Detour_GetJoints(item);uint32_t mask=0;
                const auto shown=FirstPersonBodySkin(mask);
                const auto native=FirstPersonNativeBodySkin();
                Check(shown && !std::memcmp(shown+14*12,shown+7*12,12*sizeof(float)),
                      "production crouch recovery uploads a rigid hidden collar");
                Check(native && native[14*12+3]==123,"hidden collar correction cannot enter the physics palette");
                Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)),"hidden collar correction cannot move the view");
            }
            ikNative[14*12+3]=savedHeadDelta;testViewRise=0;
            Field<uint32_t>(off::item_mesh_bits)=savedBits;
        }
        ResetBodyFollower();const auto upright=render();

        for(float drop:{.03f,.06f,-.04f,0.f}) {
            testViewRise=-drop;const auto pose=render();
            Check(std::fabs(pose[0].origin.y-upright[0].origin.y)<.1f,
                  "small headset-height changes cannot bend previously straight legs");
            for(int first:{1,4}) {
                const float reach=firstperson::Length(Sub(pose[first+1].origin,pose[first].origin))+firstperson::Length(Sub(pose[first+2].origin,pose[first+1].origin));
                Check(firstperson::Length(Sub(pose[first+2].origin,pose[first].origin))>reach*.999f,
                      "standing knees remain nearly straight without System recenter");
            }
        }
        testViewRise=-.25f;g_torsoPose.Reset();const auto crouched=render();
        Check(crouched[0].origin.y>upright[0].origin.y+50,"intentional crouch still lowers hips");
        testViewRise=0;g_torsoPose.Reset();
        Check(std::fabs(render()[0].origin.y-upright[0].origin.y)<.1f,"standing restores upright hips");
        ResetBodyFollower();g_autoBodyCalibration.pending=true;render();
        Check(!g_bodySize.valid && g_groundTorso.valid,"uncalibrated render seeds temporary torso reference");
        const float oldChestOffset=g_groundTorso.chestOffset.y;
        g_autoBodyCalibration.pending=false;render();
        Check(g_bodySize.valid && std::fabs(g_groundTorso.chestOffset.y-oldChestOffset*g_bodySize.factor)<.1f,
              "late automatic scaling refreshes torso reference at new size");
        for(float cm:{0.f,11.f,20.f}) {
        testConfig.firstPersonBodyForwardCm=cm;testViewRise=0;g_torsoClearance.Reset();ResetControllerBodyAnchor();
        const auto neutral=render();float largest=0;
        for(float drop:{.05f,.08f,.09f}) for(int frame=0;frame<40;++frame) {
            testViewRise=-drop;g_torsoClearance.time-=1./90;
            // Released-body clearance is separate from the held-body fixture.
            ResetControllerBodyAnchor();
            const auto pose=render();largest=std::max(largest,std::fabs(g_torsoClearance.angle));
            for(int j:{0,3,6}) Check(firstperson::Length(Sub(pose[j].origin,neutral[j].origin))<.1f,
                  "live torso clearance preserves hip height, body offset and planted feet");
            for(int hand=0;hand<2;++hand) {
                Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
                Check(firstperson::Length(Sub(pose[hand?10:13].origin,wrist.origin))<.1f,
                      "production clearance keeps controller wrists exact while torso bends");
            }
        }
        if(cm<20) Check(largest>.001f,"production standing solver exercises upper-torso clearance");
        }
        // The real render path must consume measured geometry and keep its
        // calibration fixed after the first upright refinement.
        testViewRise=0;
        g_bodySize.Reset();g_torsoPose.Reset();g_groundTorso.Reset();g_torsoClearance.Reset();g_torsoSurface.Reset();
        const auto unfitted=render();const float oldFactor=g_bodySize.factor;
        const auto scene=g_scenePose;
        g_torsoSurface.Add(obj,998,{-140,-190,-80},{140,100,40},200);
        const auto fittedPose=render();
        Check(g_bodySize.torsoFitted && g_bodySize.factor<oldFactor && g_bodySize.eyeGap>0,
              "production body render refines automatic size from the measured torso");
        Check(fittedPose[7].origin.y>unfitted[7].origin.y+5,
              "production standing torso is visibly lower after the fit");
        Check(!std::memcmp(&scene,&g_scenePose,sizeof(scene)),"visual torso fitting cannot move the scene camera");
        for(int first:{1,4}) {
            Check(std::fabs(fittedPose[first+2].origin.y-unfitted[first+2].origin.y)<.1f,
                  "production refinement leaves feet planted at their original floor height");
            const float reach=firstperson::Length(Sub(fittedPose[first+1].origin,fittedPose[first].origin))+firstperson::Length(Sub(fittedPose[first+2].origin,fittedPose[first+1].origin));
            Check(firstperson::Length(Sub(fittedPose[first+2].origin,fittedPose[first].origin))>reach*.999f,
                  "production fit does not restore bent standing knees");
        }
        for(int hand=0;hand<2;++hand) {
            Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
            Check(firstperson::Length(Sub(fittedPose[hand?10:13].origin,wrist.origin))<.1f,
                  "smaller visual body retains exact tracked wrists");
        }
        const float savedPitch=testHeadPitch;
        auto aimed=[&](float pitch,float crouch) {
            testHeadPitch=pitch;
            testPostureNeckDrop=.08f*(1-std::cos(pitch))-.12f*std::sin(pitch);
            testViewRise=-testPostureNeckDrop-crouch;
            g_torsoPose.Reset();return render();
        };
        for(int state:{1,16,2}) {
            Field<int16_t>(off::item_anim_state)=int16_t(state);
            const auto forward=aimed(0,0),down=aimed(-1.3f,0);
            Check(std::fabs(down[7].origin.y-forward[7].origin.y)<LiveWorldUnitsPerMetre()*.03f,
                  "neck arc cannot drop the running or standing torso into a squat");
            const auto bent=aimed(-1.3f,.25f);
            const float hipDrop=bent[0].origin.y-down[0].origin.y;
            Check(hipDrop>LiveWorldUnitsPerMetre()*.15f && hipDrop<LiveWorldUnitsPerMetre()*.35f,
                  "physical crouch lowers the fitted body once, without adding the neck arc again");
        }
        testViewRise=0;testHeadPitch=savedPitch;testPostureNeckDrop=0;
        // Jump rendering must retain the successful standing size without running
        // standing leg IK, and must keep physics on the untouched native palette.
        const auto savedFlags=Field<uint16_t>(off::item_flags);
        const float savedForward=testConfig.firstPersonBodyForwardCm;
        const float fixedSize=g_bodySize.factor;
        bool correctedAir=false;
        testConfig.firstPersonBodyForwardCm=11;
        Field<uint16_t>(off::item_flags)|=8;
        for(int jump:{3,9,15,28,25,26,27}) for(float drop:{0.f,.1f,.2f}) for(bool gravity:{true,false}) {
            if(!gravity && jump!=3 && jump!=9 && jump!=28) continue;
            Field<uint16_t>(off::item_flags)=gravity ? 8 : 0;
            Field<int16_t>(off::item_anim_state)=int16_t(jump);
            testViewRise=-drop;
            ++testVR.m_poseSerial;
            Check(CanFitAirborneBody(item),"jump fixture enters airborne visual fitting");
            float nativeBefore[180]{};std::memcpy(nativeBefore,ikNative,sizeof(nativeBefore));
            if(jump==3 || jump==9 || jump==15 || jump==28) {
                Frame hip{};ReadIKJoint(obj,ikNative,15,0,nullptr,nullptr,0,hip);
                const auto bow=firstperson::TorsoClearance::Bend(hip.origin,{1,0,0},.9f);
                for(int j=7;j<15;++j) WriteRows(Multiply(bow,ReadRows(ikNative+j*12)),ikNative+j*12);
            }
            const auto sceneBefore=g_scenePose;
            const auto itemBefore=std::string(reinterpret_cast<const char*>(item),sizeof(item));
            Detour_GetJoints(item);
            uint32_t visible=0;const float* palette=FirstPersonBodySkin(visible);
            Check(palette!=nullptr,"jump publishes its rendered palette");
            Check(!std::memcmp(&sceneBefore,&g_scenePose,sizeof(sceneBefore)) &&
                  itemBefore==std::string(reinterpret_cast<const char*>(item),sizeof(item)),
                  "jump visual correction cannot move the camera or change native gameplay state");
            const float* physics=FirstPersonNativeBodySkin();
            Check(physics!=nullptr,"airborne physics capture remains available");
            Frame native[15]{},shown[15]{};
            for(int j=0;j<15;++j) {
                ReadIKJoint(obj,physics,15,0,nullptr,nullptr,j,native[j]);
                ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,shown[j]);
            }
            const auto camera=BodyPaletteCameraOrigin(item,native[14]);
            const auto& current=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
            const auto& previous=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos_prev);
            const float t=std::clamp(*Ptr<int32_t>(dll.frameFrac),0,256)/256.f;
            const motiongun::Vec pivot{native[0].origin.x,previous.y_pos+(current.y_pos-previous.y_pos)*t-camera.y,native[0].origin.z};
            const auto offset=BodyMeshOffset(item);
            for(int j=0;j<7;++j) {
                const motiongun::Vec expected=Add(Add(pivot,Scale(Sub(native[j].origin,pivot),fixedSize)),{offset.x,0,offset.z});
                Check(firstperson::Length(Sub(shown[j].origin,expected))<.1f,
                      "jump hips and legs retain scaled native articulation without foot planting or eye anchoring");
                Check(std::fabs(firstperson::Length(Transform(shown[j].basis,{1,0,0}))-fixedSize)<.001f,
                      "airborne body does not revert to the larger native mesh");
                Check(std::fabs(firstperson::Length(Transform(native[j].basis,{1,0,0}))-1)<.001f,
                      "chest physics keeps the unscaled native animation palette");
            }
            correctedAir=correctedAir || g_torsoClearance.before<1;
            Check(g_torsoClearance.valid && g_torsoClearance.measured && g_torsoClearance.after>=.999f,
                  "measured torso clearance stays active and clears the downward jump view");
            for(int hand=0;hand<2;++hand) {
                Frame wrist{};Basis basis{};BuildControllerWrist(hand,wrist,basis);
                Check(firstperson::Length(Sub(Add(shown[hand?10:13].origin,camera),wrist.origin))<.1f,
                      "airborne visual fit retains exact tracked wrists");
            }
            Frame inverseChest{};Inverse(native[7],inverseChest);
            const auto chestChange=Multiply(shown[7],inverseChest);
            for(int shoulder:{8,11}) {
            Frame reference[3]{},actual[3]{};
            for(int j=0;j<3;++j) { reference[j]=Multiply(chestChange,native[shoulder+j]);actual[j]=shown[shoulder+j]; }
            CheckRigidTrackedReach(reference,actual);
        }
            Check(g_bodySize.factor==fixedSize,"jump never recalibrates visual size");
            if(jump==3 || jump==9 || jump==15 || jump==28) {
                Frame inverse{};Inverse({firstperson::RotationMatrix(firstperson::RotationOf(g_groundTorso.chest)),{}},inverse);
                const auto delta=firstperson::RotationOf(Multiply(inverse.basis,shown[7].basis));
                Check(2*std::atan2(firstperson::Length(delta.v),std::fabs(delta.w))<.21f,
                      "forward and straight-up jump render bounds collar tilt plus the existing clearance correction");
            }
            std::memcpy(ikNative,nativeBefore,sizeof(nativeBefore));
        }
    
    // Temporary hand tracking loss or a hands-busy flag must not resize a
    // body that is already calibrated and flying through the same jump.
    {
        auto& status=*Ptr<int16_t>(dll.lara+2);const auto oldStatus=status;
        const auto oldMissing=testMissingHand;
        Field<int16_t>(off::item_anim_state)=3;Field<uint16_t>(off::item_flags)|=8;
        for(int condition:{0,1,2,3,0}) {
            status=condition==1 ? 1 : 0;
            testMissingHand=condition==2 ? 0 : condition==3 ? 1 : -1;
            ++testVR.m_poseSerial;
            Detour_GetJoints(item);
            Frame chest{};
            Check(ReadIKJoint(obj,g_bodyRenderPalette,15,0,nullptr,nullptr,7,chest),"airborne tracking fixture resolves chest");
            Check(std::fabs(firstperson::Length(Transform(chest.basis,{1,0,0}))-fixedSize)<.001f,
                  "mid-flight controller loss or hands-busy state cannot restore the larger native torso");
            float sphere[4]{};uint64_t visible=0;
            Check(FirstPersonBodyClearance(sphere,visible) && sphere[3]<LiveWorldUnitsPerMetre()*.031f,
                  "temporary hand loss cannot expand the body clipping sphere from three to twenty centimeters");
        }
        status=oldStatus;testMissingHand=oldMissing;
    }
    Check(correctedAir,"airborne fixture actually exercises a chest intrusion correction");
        Field<uint16_t>(off::item_flags)=savedFlags;
        Field<int16_t>(off::item_anim_state)=2;
        testViewRise=0;testConfig.firstPersonBodyForwardCm=savedForward;
        const float heldFactor=g_bodySize.factor,heldReserve=g_bodySize.eyeGap;
        testViewRise=-.05f;render();
        Check(g_bodySize.factor==heldFactor && g_bodySize.eyeGap==heldReserve,
              "head movement cannot change the captured visual fit");
        g_holsters={};g_bodySkinScope=false;
    }
}
void TestBodyTransitions() {
    TestStandingCalibration();
    using namespace tr;using namespace tr::motiongun;
    for(int game:{1,2,3}) for(bool fullBody:{false,true})
    for(float yaw:{0.f,1.5707963f,-1.5707963f}) for(float cm:{0.f,5.f,11.f,20.f}) {
        BodyRig(game,false);g_holsters.active=true;g_holsters.weapon=1;testConfig.firstPersonFullBodyIK=fullBody;testConfig.firstPersonBodyScale=1;
        testConfig.firstPersonBodyForwardCm=cm;g_groundEye.valid=true;g_groundEye.bodyLocal={};
        g_haveManualInput=false;g_heading.base=yaw;
        Field<PHD_3DPOS>(off::item_pos).y_rot=Field<PHD_3DPOS>(off::item_pos_prev).y_rot=locomotion::Angle(yaw);
        auto* obj=Ptr<uint8_t>(dll.objects);
        int step=0;Frame standingChest{};
        for(int status:{0,2,4,3,0,0}) {
            *Ptr<int16_t>(dll.lara+2)=int16_t(status);
            Field<int16_t>(off::item_anim_state)=step<2 ? 1 : 2;
            Field<uint32_t>(off::item_mesh_bits)=0x3fff;g_bodySkinScope=true;
            for(int j=0;j<15;++j) {
                const auto* bind=Ptr<float>(dll.objects+192)+j*16;
                const motiongun::Vec point{-bind[12],-bind[13],-bind[14]};
                Frame animated{firstperson::IdentityBasis(),Add(point,{step*13.f,0,-step*39.f})};
                if(j>=7) {
                    const Basis pitch=firstperson::RotationMatrix(firstperson::AxisRotation({1,0,0},step*.055f));
                    const motiongun::Vec pivot{step*13.f,-650,-step*39.f};
                    animated=Multiply({pitch,Sub(Add(pivot,{0,0,-step*23.f}),Transform(pitch,pivot))},animated);
                }
                WriteRows(Multiply(animated,{firstperson::IdentityBasis(),Scale(point,-1)}),ikNative+j*12);
                for(auto offset:{off::item_joints_cur,off::item_joints_prev}) {
                    auto* joint=reinterpret_cast<int32_t*>(item+offset+j*48);
                    float rows[12];WriteRows(animated,rows);
                    for(int k=0;k<12;++k) joint[k]=int32_t(rows[k]*16384);
                }
            }
            ++testVR.m_poseSerial;
            if(fullBody) {
                float palette[384];std::memcpy(palette,ikNative,sizeof(palette));
                Check(ApplyFullBodyIK(item,palette,15),"transition regression exercises complete body IK");
            }
            Detour_GetJoints(item);uint32_t mask=0;const auto* render=FirstPersonBodySkin(mask);
            Frame hip{};Check(render && ReadIKJoint(obj,render,15,0,nullptr,nullptr,0,hip),"transition pelvis is readable");
            Check(std::fabs(hip.origin.x-std::sin(yaw)*cm*10)<.05f && std::fabs(hip.origin.z-std::cos(yaw)*cm*10)<.05f,
                  "run-to-stop animation cannot move body from its configured position");
            Frame chest{};ReadIKJoint(obj,render,15,0,nullptr,nullptr,7,chest);
            if(step==0) standingChest=chest;
            Check(std::fabs(chest.origin.x-standingChest.origin.x)<.06f && std::fabs(chest.origin.z-standingChest.origin.z)<.06f && (fullBody || std::fabs(chest.origin.y-standingChest.origin.y)<.06f),
                  "native running chest and stop blend cannot move the visible torso backwards");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) Check(std::fabs(chest.basis.r[r][c]-standingChest.basis.r[r][c])<.001f,
                  "chest mesh cannot pitch back into the headset on stopping");
            float sphere[4]{};uint64_t clearanceMask=0;
            Check(FirstPersonBodyClearance(sphere,clearanceMask) && std::fabs(sphere[3]-(fullBody ? 30.f : 200.f))<.001f && clearanceMask==0x40ff,
                  "clearance affects body and excludes tracked arms/hands");
            if(fullBody) {
                float inverse[12]{};uint64_t torsoMask=0;bool measured=false;
                Check(FirstPersonTorsoMeasureInfo(inverse,torsoMask,measured) && torsoMask==(1ull<<7),
                      "production torso measurement selects the rendered chest palette slot");
                const float empty[3]={};FirstPersonTorsoMeasure(999,empty,empty,0);
                Check(FirstPersonTorsoBufferKnown(999) && !g_torsoSurface.valid,
                      "arm-only buffers are cached without inventing torso bounds");
            }
            Ptr<int32_t>(dll.w2vMatrix)[3]=32;
            float otherEye[4]{};uint64_t otherMask=0;FirstPersonBodyClearance(otherEye,otherMask);
            Check(std::fabs(otherEye[0]+32-sphere[0])<.01f && otherMask==clearanceMask,"clearance is stable between stereo origins");
            Ptr<int32_t>(dll.w2vMatrix)[3]=0;
            for(int excluded=0;excluded<4;++excluded) {
                const bool activeBefore=g_active,scopeBefore=g_bodySkinScope;const int armBefore=g_renderArm;
                if(excluded==0) g_active=false;
                if(excluded==1) g_bodySkinScope=false;
                if(excluded==2) g_renderArm=1;
                if(excluded==3) *Ptr<int32_t>(dll.renderPass)=4;
                Check(!FirstPersonBodyClearance(sphere,clearanceMask) && sphere[3]==0 && clearanceMask==0,
                      "third-person, non-body, tracked hand and shadow draws cannot inherit clearance");
                g_active=activeBefore;g_bodySkinScope=scopeBefore;g_renderArm=armBefore;*Ptr<int32_t>(dll.renderPass)=0;
            }
            for(int hand=0;hand<2;++hand) {
                Frame wrist{},shown{};Basis controller{};BuildControllerWrist(hand,wrist,controller);
                Check(ReadIKJoint(obj,render,15,0,nullptr,nullptr,hand?10:13,shown) && firstperson::Length(Sub(shown.origin,wrist.origin))<.05f,
                      "draw and holster preserve live controller wrists");
            }
            Check(!std::memcmp(FirstPersonNativeBodySkin(),ikNative,15*12*sizeof(float)),"transition physics stays native");
            ++step;
        }
    }
    for(int game:{1,2,3}) for(bool fullBody:{false,true}) for(int weapon:{1,2,3}) {
        BodyRig(game,false);testConfig.firstPersonFullBodyIK=fullBody;testConfig.firstPersonBodyScale=1;
        testConfig.firstPersonBodyForwardCm=11;testConfig.firstPersonBodyHandYawWeight=0;
        g_groundEye.valid=true;g_groundEye.bodyLocal={};g_haveManualInput=false;
        *Ptr<int16_t>(dll.lara+4)=int16_t(weapon);g_holsters.active=false;
        auto* obj=Ptr<uint8_t>(dll.objects);
        auto render=[&](int status,bool repeat) {
            *Ptr<int16_t>(dll.lara+2)=int16_t(status);Field<uint32_t>(off::item_mesh_bits)=0x3fff;
            g_pistolBodyScope=g_bodySkinScope=true;g_pistolForearmScope=false;
            if(!repeat) ++testVR.m_poseSerial;
            Detour_GetJoints(item);uint32_t mask=0;const auto* palette=FirstPersonBodySkin(mask);
            std::array<Frame,15> result{};
            for(int j=0;j<15;++j) Check(palette && ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,result[j]),"sidearm body joints are readable");
            if(PistolBodyPoseMatches(item,0)) {
                Field<uint32_t>(off::item_mesh_bits)=0x3600;g_pistolBodyScope=false;g_pistolForearmScope=true;
                Detour_GetJoints(item);palette=FirstPersonBodySkin(mask);
                for(int j=8;j<14;++j) {
                    Frame shown{};ReadIKJoint(obj,palette,15,0,nullptr,nullptr,j,shown);
                    Check(firstperson::Length(Sub(shown.origin,result[j].origin))<.05f,"attachments share the animated sidearm wrists");
                }
            }
            g_pistolBodyScope=g_pistolForearmScope=false;
            return result;
        };
        ExerciseSidearmAnimation(render,[&] {
            for(int hand=0;hand<2;++hand) testControllerOffset[hand].x+=.05f;
        },[&] {
            for(int j=8;j<14;++j) {
                ikNative[j*12+11]+=30;
                for(auto offset:{off::item_joints_cur,off::item_joints_prev})
                    reinterpret_cast<int32_t*>(item+offset+j*48)[11]+=30*16384;
            }
        });
    }
    for(int game:{1,2,3}) for(bool armed:{false,true}) {
        PhysicalBodySetup(game,armed);
        for(int state:{23,45,74}) {
            Field<int16_t>(off::item_anim_state)=int16_t(state);
            const bool roll=state!=74 || game==3;
            Check(IsRollState(item)==roll,"sprint somersault exists in TR3; ordinary rolls are shared");
            SetMeshVisibility(true,roll);
            Check(g_rollHidden==roll,"tracked controllers do not cancel somersault hiding");
            physicalDrawCalls=0;Detour_DrawCreatureHD(item,0);
            if(roll) Check(physicalDrawCalls==0,"somersault suppresses HD body even with weapon tracking");
        }
        SetMeshVisibility(false,false);
        for(int restIndex:{1,6}) for(int status:{2,3}) {
            IKSetup(game);*Ptr<int16_t>(dll.lara+2)=int16_t(status);Field<uint32_t>(off::item_mesh_bits)=0x3600;
            auto* rest=Ptr<uint8_t>(dll.laraHands)+restIndex*104;auto* open=rest+3*104;
            auto* geom=Ptr<uint8_t>(dll.objects+88);
            std::memset(rest,1,104);std::memset(open,4,104);
            *reinterpret_cast<void**>(rest+16)=reinterpret_cast<void*>(0x1234);
            std::memcpy(geom,open,104);
            { UnarmedRestHandScope scope(item,1);Check(!std::memcmp(geom,rest,104),"draw/holster keeps relaxed fingers instead of open hands"); }
            Check(!std::memcmp(geom,open,104),"native hand descriptor is restored after rendering");
        }
    }
}
}
