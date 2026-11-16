// Test oracle for the requested shoulder-anchored reach contract.
tr::motiongun::Frame ExpectedConnectedWrist(const tr::motiongun::Frame (&chain)[3],tr::motiongun::Frame target) {
    using namespace tr::motiongun;using tr::firstperson::Length;
    const float a=Length(Sub(chain[1].origin,chain[0].origin)),b=Length(Sub(chain[2].origin,chain[1].origin));
    tr::motiongun::Vec delta=Sub(target.origin,chain[0].origin);float distance=Length(delta);
    if(distance<.00001f) {delta=Sub(chain[2].origin,chain[0].origin);distance=Length(delta);}
    const float margin=std::min(a,b)*.001f;
    const float requested=Length(Sub(target.origin,chain[0].origin));
    const float radius=std::clamp(requested,std::fabs(a-b)+margin,a+b-margin);
    if(requested!=radius) target.origin=Add(chain[0].origin,Scale(delta,radius/distance));
    return target;
}

tr::motiongun::Frame ExpectedPublishedWrist(int hand,tr::motiongun::Frame target) {
    Check(tr::g_connectedWeaponReach[hand].valid,"successful arm draw publishes connected reach");
    return ExpectedConnectedWrist(tr::g_connectedWeaponReach[hand].chain,target);
}

