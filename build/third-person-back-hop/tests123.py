from pathlib import Path
src=Path('C:/dev/TombRaider123VR/tests/back_hop_test.cpp').read_text()
def change(a,b):
    global src
    assert src.count(a)==1,(a[:70],src.count(a))
    src=src.replace(a,b)
change('''        if(NewControls() && *Ptr<int16_t>(dll.lara+2)==4) {
            const auto* axes=Ptr<int16_t>(dll.analogInput);
            Check(!axes[0] && !axes[1],"armed hop cannot acquire analog turn bits");
        }
        state=5;goal=2;Field<int16_t>(24)=42;''','''        const auto* axes=Ptr<int16_t>(dll.analogInput);
        Check(!axes[0] && !axes[1] && axes[2]==Field<PHD_3DPOS>(88).y_rot,
            "hop bypasses analog turning and uses Lara facing, armed or unarmed");
        state=5;goal=2;Field<int16_t>(24)=88;''')
change('Reset(clip==103 ? 2 : 16,{0,-1},smooth,head);EnableAnimations(game);',
       'Reset(clip==103 ? 2 : clip==40 ? 16 : clip==12 ? 6 : clip==13 ? 7 : 20,{0,-1},smooth,head);EnableAnimations(game);')
change('    seenHopInput=0;captureOnly=false;', '''    seenHopInput=0;captureOnly=false;
    g_active=g_runtimeEnabled=g_haveHeading=false;g_headingItem=nullptr;
    Field<PHD_3DPOS>(88).y_rot=Angle(head);
    Ptr<int16_t>(dll.analogInput)[2]=Ptr<int16_t>(dll.analogInput)[3]=Angle(head+1.2f);''')
change('for(int clip:{103,40})','for(int clip:{103,40,12,13,44,69})')
change('hop moves backward relative to HMD','third-person hop moves backward relative to Lara')
change('Check(Ptr<int16_t>(dll.analogInput)[1]!=0,"analog stick restored after native simulation");',
       'Check(Ptr<int16_t>(dll.analogInput)[1]!=0 && Ptr<int16_t>(dll.analogInput)[2]==Angle(head+1.2f) && *Ptr<uint32_t>(dll.input)==(Back|Walk),"native input and chase-camera axes restored after simulation");')
change('''        HopSetup(game,true,false,true,0,103);HopInput(0,-1,false);Detour_LaraAboveWater(item,nullptr);
        Check(Field<int16_t>(18)==16 && (seenHopInput&Walk),"LS-back without RG still walks");''','''        for(bool grip:{false,true}) {
            HopSetup(game,true,false,true,0,103);
            g_active=g_runtimeEnabled=g_haveHeading=true;g_headingItem=item;
            HopInput(0,-1,grip);Detour_LaraAboveWater(item,nullptr);
            Check(Field<int16_t>(18)==16 && (seenHopInput&Walk),"first-person movement is unchanged with RG held or released");
        }
        if(game==3) {
            HopSetup(game,true,false,true,0,103);HopInput(0,-1,true);Field<int16_t>(22)=71;
            Detour_LaraAboveWater(item,nullptr);
            Check(Field<int16_t>(18)==5 && Field<int16_t>(22)==71,"standing queued DUCK permits hop without clearing native queue");
        }
        for(const Vec stick:{Vec{},Vec{0,1},Vec{1,0},Vec{-1,0}}) {
            HopSetup(game,true,false,true,0,103);captureOnly=true;HopInput(stick.x,stick.z,true);
            Detour_LaraAboveWater(item,nullptr);
            Check(seenHopInput==(Back|Walk),"neutral, forward and sideways sticks do not activate hop");
        }''')
change('for(int guard=0;guard<13;++guard)', 'for(int guard=0;guard<16;++guard)')
change('if(guard==0) g_active=false;', 'if(guard==0) g_active=true;')
change('if(guard==3) g_heightCrouch.active=true;', 'if(guard==3) g_runtimeEnabled=true;')
change('HopInput(0,-1,true,guard==12);', '''if(guard==13) testConfig.gamepadEnabled=false;
            if(guard==14) *Ptr<uint16_t>(dll.lara+60)|=0x2000;
            HopInput(0,-1,guard!=15,guard==12);''')
change('third person, vents, posture, jump, Action, roll and shifted input keep Walk',
       'first person (including suspended), vents, jump, Action, roll and shifted input are untouched')
change('focused back-hop checks (TR1/2/3)','third-person hop and first-person preservation checks (TR1/2/3)')
Path('C:/dev/TombRaider456VR/build/third-person-back-hop/back_hop123_test.cpp').write_text(src,encoding='utf-8',newline='\r\n')
print('Prepared corrected TR1-3 scope regression fixture.')
