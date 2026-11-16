from pathlib import Path
import shutil
exec(Path('build/soft-arm-reach/continue.py').read_text().split('for base in [r,stage]:')[0])
for p in [r/'tools/unarmed_ik_regression.inl',stage/'tests/connected_reach_expected.h']:
 edit(p,'tr::motiongun::Frame target) {','tr::motiongun::Frame target,float percent=100) {')
 edit(p,'const float a=Length(Sub(chain[1].origin,chain[0].origin)),b=Length(Sub(chain[2].origin,chain[1].origin));','const float fit=percent/100;\n    const float a=Length(Sub(chain[1].origin,chain[0].origin))*fit,b=Length(Sub(chain[2].origin,chain[1].origin))*fit;')
 edit(p,'const float radius=std::clamp(requested,std::fabs(a-b)+margin,a+b-margin);','const double reach=a+b-margin,begin=1.04*reach,band=.08*reach;\n    const float radius=requested>begin ? float(begin+band*(1-std::exp(-(requested-begin)/band))) : std::max(requested,std::fabs(a-b)+margin);')
 edit(p,'return ExpectedConnectedWrist(tr::g_connectedWeaponReach[hand].chain,target);','return ExpectedConnectedWrist(tr::g_connectedWeaponReach[hand].chain,target,tr::Cfg().firstPersonArmLengthPercent);')
 # Only ExpectedConnectedWrist takes percent; do not add unused parameter to publisher.
 edit(p,'ExpectedPublishedWrist(int hand,tr::motiongun::Frame target,float percent=100)', 'ExpectedPublishedWrist(int hand,tr::motiongun::Frame target)')
for p in [r/'tools/connected_reach_regression.inl',stage/'tests/connected_reach_selftest.h']:
 edit(p,'Sub(limited.origin,{100,-600,699.8f})','Sub(limited.origin,ExpectedConnectedWrist(chain,requested).origin)')
 edit(p,'weapon wrist stops at fixed shoulder reach','weapon wrist follows the soft limit from the unstretched reference')
 edit(p,'reason<11','reason<12');edit(p,'if(reason==10) ++reach.bodyOffset;','if(reason==10) ++reach.bodyOffset;\n            if(reason==11) ++reach.armLength;')
 # Prove manual fit applies in the real weapon limiter, not just standalone IK.
 edit(p,'        const auto original=g_connectedWeaponReach[hand];','''        const float savedFit=Cfg().firstPersonArmLengthPercent;
        for(float fit:{80.f,100.f,120.f}) {
            CONFIG.firstPersonArmLengthPercent=fit;
            PublishConnectedWeaponReach(ITEM,hand,chain,{});
            auto fitted=requested;LimitConnectedWeaponWrist(ITEM,hand,fitted);
            Check(firstperson::Length(Sub(fitted.origin,ExpectedConnectedWrist(chain,requested,fit).origin))<.03f,
                  "live arm fit affects weapon reach without compounding extension");
        }
        CONFIG.firstPersonArmLengthPercent=savedFit;PublishConnectedWeaponReach(ITEM,hand,chain,{});
        const auto original=g_connectedWeaponReach[hand];'''.replace('CONFIG','config' if base is None else 'config') if False else '''        const float savedFit=Cfg().firstPersonArmLengthPercent;
        for(float fit:{80.f,100.f,120.f}) {
            CONFIG.firstPersonArmLengthPercent=fit;
            PublishConnectedWeaponReach(ITEM,hand,chain,{});
            auto fitted=requested;LimitConnectedWeaponWrist(ITEM,hand,fitted);
            Check(firstperson::Length(Sub(fitted.origin,ExpectedConnectedWrist(chain,requested,fit).origin))<.03f,
                  "live arm fit affects weapon reach without compounding extension");
        }
        CONFIG.firstPersonArmLengthPercent=savedFit;PublishConnectedWeaponReach(ITEM,hand,chain,{});
        const auto original=g_connectedWeaponReach[hand];'''.replace('CONFIG','config' if p.parent.name=='tools' else 'testConfig').replace('ITEM','itemMemory' if p.parent.name=='tools' else 'item'))
p=r/'tools/upjump_arms_test.cpp';edit(p,'ExpectedConnectedWrist(chain,target)','ExpectedPublishedWrist(j==10 ? 1 : 0,target)');edit(p,'std::fabs(dot-(r==c?1.f:0.f))<.003f','std::fabs(dot-(r==c?1.f:0.f))<.323f');edit(p,'retains rigid arm geometry','retains bounded arm geometry')
p=r/'tools/unarmed_ik_regression.inl';edit(p,'std::fabs(dot-(r==c ? 1.f : 0.f))<.003f,\n                                  "production tracked reach preserves rigid arms, hands and weapon dimensions"','std::fabs(dot-(r==c ? 1.f : 0.f))<(joint==10 || joint==13 ? .003f : .323f),\n                                  "production reach bounds arm extension and preserves rigid hands and weapons"')
p=r/'tools/fullbody_ik_regression.inl';edit(p,'ExpectedConnectedWrist(reachChain,target)','ExpectedPublishedWrist(1,target)');edit(p,'ExpectedConnectedWrist(chain,target)','ExpectedPublishedWrist(joint==10 ? 1 : 0,target)');edit(p,'std::fabs(dot-(r==c ? 1.f : 0.f))<.003f,\n                              "full-body tracked arms retain rigid meshes with calibrated native-size body"','std::fabs(dot-(r==c ? 1.f : 0.f))<(joint==10 || joint==13 ? .003f : .323f),\n                              "full-body arms allow bounded stretch while hand meshes remain rigid"')
p=r/'tools/connected_arm_tests.cpp';s=read(p)
s=s.replace('for(float scale:', 'for(float fit:{80.f,100.f,120.f}) for(float scale:')
s=s.replace('arm,corrections)','arm,corrections,true,nullptr,fit)').replace('arm,repeat)','arm,repeat,true,nullptr,fit)')
s=s.replace('std::fabs(v-(r==c?1.f:0.f))<.003f','std::fabs(v-(r==c?1.f:0.f))<(i==2 ? .003f : 1.f)')
s=s.replace('connected limbs and hands never scale or shear','limb scale is bounded and hands never scale')
a=s.index('    Check(std::fabs(Length(Sub(solved[1]');b=s.index('    for(int r=0;r<3;++r)for(int c=0;c<3;++c)Check',a)
s=s[:a]+'''    const float a=300*scale*fit/100,b=200*scale*fit/100,natural=a+b-std::min(a,b)*.001f;
    const float requested=distance*scale,begin=natural*1.04f,band=natural*.08f;
    const float radius=requested>begin ? begin+band*(1-std::exp(-(requested-begin)/band)) : std::max(requested,std::fabs(a-b)+std::min(a,b)*.001f);
    Check(std::fabs(Length(Sub(solved[2].origin,arm[0].origin))-radius)<.05f,"independent soft reach envelope matches wrist");
    for(int i=0;i<2;++i) {
        const float fitted=i==0 ? a : b;
        const float actual=Length(Sub(solved[i+1].origin,solved[i].origin));
        Check(actual>=fitted-.05f && actual<=fitted*1.15f+.05f,"extension never exceeds fifteen percent of fitted bone length");
        Vec bone{},width{};Unit(Sub(arm[i+1].origin,arm[i].origin),bone);Unit(Cross(bone,{0,1,0}),width);
        const auto across=Cross(bone,width);
        Check(std::fabs(Length(Transform(corrections[i].basis,width))-1)<.003f &&
              std::fabs(Length(Transform(corrections[i].basis,across))-1)<.003f,"axial extension preserves limb thickness in both perpendicular directions");
    }
'''+s[b:];write(p,s)
# Exercise menu fit save/reload, errors, navigation and defaults.
p=r/'tools/vr_menu_tests.cpp';s=read(p).replace('SettingCount==4','SettingCount==5').replace('all four settings','all five settings')
s=s.replace('    ControllerPage page;', '''    for(float fit:{80.f,101.f,120.f,100.f}) {
        original=ReadSettings();next=original;next[4]=fit;
        check(SaveSettings(next,original,error) && tr::Cfg().firstPersonArmLengthPercent==fit,"arm length applies immediately");
        tr::LoadConfig(ini.c_str());check(ReadSettings()[4]==fit && tr::Cfg().firstPersonArmLengthPercent==fit,"arm fit survives reload");
    }
    for(float invalid:{79.f,121.f,100.5f,std::numeric_limits<float>::quiet_NaN()}) {
        original=ReadSettings();next=original;next[4]=invalid;
        check(!SaveSettings(next,original,error) && tr::Cfg().firstPersonArmLengthPercent==100,"invalid arm fit cannot change live setting");
    }
    ControllerPage page;''')
s=s.replace('page.selected==3,"Up from first row wraps to Body Position");','page.selected==4,"Up from first row wraps to Arm Length");\n    check(page.ChangedValue(0x80000000)==101 && page.ChangedValue(0x40000000)==99 && page.ChangedValue(0x100000)==100,"arm fit has steps and reset");\n    page.armLengthPercent=120;check(page.ChangedValue(0x80000000)==120,"arm fit upper bound");\n    page.armLengthPercent=80;check(page.ChangedValue(0x40000000)==80,"arm fit lower bound");\n    check(page.Input(0x10000000)==Action::None && page.selected==3,"Up from arm length visits body position");')
s=s.replace('page.selected==0,"Down wraps from the body setting");','page.selected==4,"Down from body visits Arm Length");\n    check(page.Input(0x20000000)==Action::None && page.selected==0,"Down wraps from Arm Length");')
write(p,s)
# Existing focused integration uses a shortened fullbody suite, before the known
# unrelated walking fixture failure. Update its included source, keeping cutoff.
old=r/'build/connected-arms'
for n in ['integration.cpp','first_person_tests.cpp','integration.cmd']:
 write(w/n,read(old/n).replace('connected-arms','soft-arm-reach').replace('/Od','/O2'))
full=read(r/'tools/fullbody_ik_regression.inl');oldpart=read(old/'integration-fullbody.inl')
(w/'fullbody-cutoff.txt').write_text(oldpart[-1100:])
print('Tests updated; inspect integration cutoff before running.')
