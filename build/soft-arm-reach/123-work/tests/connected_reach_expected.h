// Test oracle for the requested shoulder-anchored reach contract.
tr::motiongun::Frame ExpectedConnectedWrist(const tr::motiongun::Frame (&chain)[3],tr::motiongun::Frame target,float percent=100) {
    using namespace tr::motiongun;using tr::firstperson::Length;
    const float fit=percent/100;
    const float a=Length(Sub(chain[1].origin,chain[0].origin))*fit,b=Length(Sub(chain[2].origin,chain[1].origin))*fit;
    tr::motiongun::Vec delta=Sub(target.origin,chain[0].origin);float distance=Length(delta);
    if(distance<.00001f) {delta=Sub(chain[2].origin,chain[0].origin);distance=Length(delta);}
    const float margin=std::min(a,b)*.001f;
    const float requested=Length(Sub(target.origin,chain[0].origin));
    const double reach=a+b-margin,begin=1.04*reach,band=.08*reach;
    const float radius=requested>begin ? float(begin+band*(1-std::exp(-(requested-begin)/band))) : std::max(requested,std::fabs(a-b)+margin);
    if(requested!=radius) target.origin=Add(chain[0].origin,Scale(delta,radius/distance));
    return target;
}

tr::motiongun::Frame ExpectedPublishedWrist(int hand,tr::motiongun::Frame target) {
    Check(tr::g_connectedWeaponReach[hand].valid,"successful arm draw publishes connected reach");
    return ExpectedConnectedWrist(tr::g_connectedWeaponReach[hand].chain,target,tr::Cfg().firstPersonArmLengthPercent);
}

