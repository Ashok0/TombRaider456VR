namespace {
void TestJumpLandingIntent() {
    using tr::firstperson::JumpLanding;
    int owner=0,other=0;
    for(int jumpState:{3,25,26,27,28}) for(int landingAnimation:{31,82,92,99,24})
    for(int drop:{-512,0,128,129,1024}) for(bool deliberate:{false,true}) {
        JumpLanding presentation;
        presentation.Observe(&owner,1,true,2,103,1000,false,true,false,deliberate);
        if(deliberate) presentation.Observe(&owner,1,true,15,26,1000,false,false,false,false);
        presentation.Observe(&owner,1,true,jumpState,0,700,true,false,false,false);
        const int landState=landingAnimation==92 ? 1 : 2;
        presentation.Observe(&owner,1,true,landState,landingAnimation,1000+drop,false,true,true,false);
        const bool wanted=deliberate && drop<=128 && landingAnimation!=24;
        Check(presentation.ordinary==wanted,"only intentional ground jumps with shallow landings suppress the visual squat");
        for(int repeat=0;repeat<3;++repeat) {
            presentation.Observe(&owner,1,true,landState,landingAnimation,1000+drop,false,true,true,false);
            Check(presentation.ordinary==wanted,"ordinary landing presentation survives the entire native landing clip");
        }
        presentation.Observe(&owner,1,true,2,103,1000+drop,false,true,false,false);
        Check(!presentation.ordinary,"standing idle releases ordinary landing presentation");
    }
    for(int reset=0;reset<5;++reset) {
        JumpLanding presentation;
        presentation.Observe(&owner,1,true,15,26,0,false,false,false,false);
        presentation.Observe(&owner,1,true,3,0,-400,true,false,false,false);
        if(reset==0) presentation.Observe(&owner,1,true,9,0,-100,true,false,false,false);
        if(reset==1) presentation.Observe(&other,1,true,3,0,-100,true,false,false,false);
        if(reset==2) presentation.Observe(&owner,2,true,3,0,-100,true,false,false,false);
        if(reset==3) presentation.Observe(&owner,1,false,3,0,-100,true,false,false,false);
        if(reset==4) presentation.Observe(&owner,1,true,10,0,-100,false,false,false,false);
        presentation.Observe(&owner,1,true,2,31,0,false,true,true,false);
        Check(!presentation.ordinary,"fast falls, changed owner/level, suspended FP and grabs retain native landing behavior");
    }
}
}
