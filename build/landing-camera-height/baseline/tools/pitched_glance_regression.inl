// A downward glance rotates the eyes through a shorter horizontal arc.
// Ordinary small hand drift must not validate the fictitious translation
// produced by subtracting a level-head neck offset from that pitched pose.
void TestPitchedGlanceTracking() {
    using namespace tr::firstperson;
    auto update=[](ControllerBodyAnchor& a,int frame,double now,tr::motiongun::Vec eye,
            tr::motiongun::Vec left,tr::motiongun::Vec right,float yaw,tr::motiongun::Vec forward) {
#ifdef LEGACY_NECK_PROJECTION
        a.Update(frame,now,eye,left,right,0,0,true,false,yaw,.15f);
#else
        a.Update(frame,now,eye,left,right,0,0,true,false,yaw,.15f,forward);
#endif
    };
    for(float hz:{60.f,90.f}) for(float pitch:{.35f,.7f,1.05f,1.4f})
        for(float drift:{.01f,.02f,.04f,.06f}) for(float reach:{.15f,.5f}) {
        ControllerBodyAnchor a;tr::motiongun::Vec initial{};
        for(int i=0;i<int(8*hz);++i) {
            const float t=std::max(0.f,i/hz-1)*2.5f,yaw=1.1f*std::sin(t);
            const auto forward=YawVector({0,-std::sin(pitch),std::cos(pitch)},yaw);
            const auto eye=Add({0,1.7f,-.15f},Scale(forward,.15f));
            const tr::motiongun::Vec sway{-drift*std::sin(t),0,.003f*std::sin(t*.7f)};
            update(a,i+1,i/double(hz),eye,Add({-.2f,1.3f,reach},sway),Add({.2f,1.3f,reach},sway),yaw,forward);
            if(i==0) initial=a.head;
            Check(Length(Flat(Sub(a.head,initial)))<.00001f,
                "pitched planted glances cannot invent neck translation when raised hands drift");
        }
    }
    // The same pitched glance during actual walking still follows the shared
    // physical displacement; no additional roomscale deadzone is introduced.
    for(float pitch:{0.f,.7f,1.4f}) for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor a;tr::motiongun::Vec initial{},travel{};int permitted=0;
        for(int i=0;i<600;++i) {
            const float t=std::max(0,i-90)/90.f,yaw=.8f*std::sin(t*1.5f);
            const auto forward=YawVector({0,-std::sin(pitch),std::cos(pitch)},yaw);
            travel=YawVector({0,0,t*.2f},direction*1.5707963f);
            const auto eye=Add(travel,Add({0,1.7f,-.15f},Scale(forward,.15f)));
            update(a,i+1,i/90.,eye,Add(travel,{-.2f,1.3f,.5f}),Add(travel,{.2f,1.3f,.5f}),yaw,forward);
            if(i==0) initial=a.head;
            if(i>120 && a.translating) ++permitted;
        }
        Check(permitted>400,"looking down preserves responsive physical walking in all four directions");
        Check(Length(Flat(Sub(Sub(a.head,initial),travel)))<.04f,
            "pitched roomscale walking keeps the body centered on accepted travel");
    }
}
