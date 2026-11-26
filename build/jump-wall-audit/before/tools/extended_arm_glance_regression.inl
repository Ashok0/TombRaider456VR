// Raised pistols amplify small aiming rotations at the hands. A planted
// head glance plus this arm motion must not become shared body translation.
void TestExtendedArmGlances() {
    using namespace tr::firstperson;
    for(float hz:{60.f,90.f}) for(float reach:{.15f,.4f,.65f})
        for(float halfSpan:{.2f,.5f}) for(float aim:{.01f,.03f}) for(float drift:{.005f,.012f})
        for(float lean:{.02f,.05f}) for(float speed:{1.35f,5.4f}) for(float phase:{0.f,1.f}) {
        ControllerBodyAnchor anchor;
        for(int frame=0;frame<int(10*hz);++frame) {
            const float seconds=frame/hz,t=std::max(0.f,seconds-1)*speed;
            const float yaw=.8f*std::sin(t),shift=drift*(std::sin(t+phase)-std::sin(phase));
            const float handYaw=aim*(std::sin(t*.7f+phase)-std::sin(phase));
            const tr::motiongun::Vec eye=Add({lean*std::sin(t),1.7f,-.15f},YawVector({0,0,.15f},yaw));
            const auto left=Add({shift,0,0},YawVector({-halfSpan,1.3f,reach},handYaw));
            const auto right=Add({shift,0,0},YawVector({halfSpan,1.3f,reach},handYaw));
            anchor.Update(frame+1,seconds,eye,left,right,0,0,true,false,yaw);
            Check(Length(Flat(anchor.head))<.00001f,
                "small extended-arm aiming drift during a planted glance cannot translate the mesh");
            Check(!anchor.heading.sampleAccepted,
                "small extended-arm aiming drift does not establish a physical body turn");
        }
    }
    // Armed roomscale walking remains available in every direction while the
    // player looks aside; a head turn must not disable genuine shared travel.
    for(float reach:{.15f,.4f,.65f}) for(int direction=0;direction<4;++direction) {
        ControllerBodyAnchor anchor;int permitted=0;tr::motiongun::Vec travel{};
        for(int frame=0;frame<600;++frame) {
            const float t=std::max(0,frame-90)/90.f,yaw=.8f*std::sin(t*1.5f);
            travel=YawVector({0,0,t*.2f},direction*1.5707963f);
            const auto eye=Add(travel,Add({0,1.7f,-.15f},YawVector({0,0,.15f},yaw)));
            anchor.Update(frame+1,frame/90.,eye,Add(travel,{-.2f,1.3f,reach}),
                Add(travel,{.2f,1.3f,reach}),0,0,true,false,yaw);
            if(frame>120 && anchor.translating) ++permitted;
        }
        Check(permitted>400,"extended-arm walking remains responsive in all four directions during head glances");
        Check(Length(Flat(Sub(anchor.head,travel)))<.04f,
            "extended-arm walking does not leave the mesh behind the player");
    }
}
