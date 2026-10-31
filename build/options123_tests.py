from pathlib import Path
p=Path('C:/dev/TombRaider456VR/tools/vr_native_menu_tests.cpp');s=p.read_text()
a=s.index('    alignas(8) uint8_t items');b=s.index('    alignas(8) uint8_t menu',a)
s=s[:a]+'''    vrmenu::Options123Selection parent;
    for(int choice=0;choice<13;++choice) {
        uint32_t input=0x100000;
        check(!parent.Input(choice,input) && input==0x100000,"all thirteen original Options actions remain native");
    }
    uint32_t input=0x4000;parent.Input(12,input);
    check(parent.vr && input==0,"native inventory Down enters VR Options after the last original row");
    input=0x18000;parent.Input(12,input);check(input==0,"VR row cannot adjust the underlying original setting");
    input=0x100000;check(parent.Input(12,input) && input==0,"native Select opens the nested page");
    input=0x2000;parent.Input(12,input);check(!parent.vr && input==0,"Up returns to the final original row without skipping it");
    input=0x104000;parent.Input(12,input);check(parent.vr && input==0,"Down plus Select cannot trigger an original action");
    input=0x300000;check(!parent.Input(12,input) && !parent.vr && input==0x300000,"Back takes precedence and stays native at parent level");
    check(vrmenu::Options123Buttons(0x2000)==0x10000000,"TR123 Up maps to child navigation");
    check(vrmenu::Options123Buttons(0x4000)==0x20000000,"TR123 Down maps to child navigation");
    check(vrmenu::Options123Buttons(0x8000)==0x40000000,"TR123 Left maps to child change");
    check(vrmenu::Options123Buttons(0x10000)==0x80000000,"TR123 Right maps to child change");
    check(vrmenu::Options123Buttons(0x300000)==0x300000,"native Confirm and Back retain their codes");
    for(const auto& r:Rows) if(r.game<=3) {
        for(const auto& e:Extras) if(r.stamp==e.stamp) {
            Slot addressSlot;addressSlot.row=&r;addressSlot.extra=&e;
            check(e.options && HookRva(addressSlot)!=r.title,"TR123 hooks the existing Options handler, never RingInit");
        }
    }
'''+s[b:]
s=s.replace('uint8_t memory[512]{}','uint8_t memory[65536]{}')
a=s.index('    row.game=1;extra.textCount=300;');b=s.index('    std::printf(',a)
s=s[:a]+'''    row.game=1;row.transition=0x800;extra.textCount=300;
    extra.textPool=0x1000;extra.textBuffers=0x3000;extra.nativeTexts=512;extra.firstVisible=480;
    At<int16_t>(slot,300)=2;
    auto record=[&](int index,const char* label) {
        auto* text=&At<uint8_t>(slot,extra.textPool+index*88);
        auto* string=&At<char>(slot,extra.textBuffers+index*512);
        Field<uint32_t>(text,0)=1;Field<char*>(text,72)=string;strcpy_s(string,512,label);return text;
    };
    auto* text=record(0,"VR Options");auto* label=Field<char*>(text,72);slot.texts[0]=text;
    strcpy_s(slot.textValues[0].data(),128,"VR Options");ClearTexts(slot);
    check(!(Field<uint32_t>(text,0)&1) && At<int16_t>(slot,300)==1,"closing page releases its persistent native text");
    record(0,"Another menu");slot.texts[0]=text;ClearTexts(slot);
    check((Field<uint32_t>(text,0)&1) && At<int16_t>(slot,300)==1,"cleanup cannot remove a reused native text slot");
    check(!ValidText123(slot,reinterpret_cast<void*>(1)),"out-of-pool pointers are rejected before dereferencing");
    check(!ValidText123(slot,static_cast<uint8_t*>(text)+1),"misaligned pool pointers are rejected");
    Field<char*>(text,72)=label+1;check(!ValidText123(slot,text),"text records must own the expected bounded string buffer");
    Field<char*>(text,72)=label;
    for(int i=0;i<20;++i) At<void*>(slot,extra.nativeTexts+i*8)=record(i+1,"Native option");
    At<int16_t>(slot,300)=21;ClearNativeTexts123(slot);
    for(int i=0;i<20;++i) check(!At<void*>(slot,extra.nativeTexts+i*8),"opening child releases all native parent text references");
    check(At<int16_t>(slot,300)==1 && (Field<uint32_t>(text,0)&1),"parent cleanup preserves unrelated ring text");
    for(int i=0;i<5;++i) { slot.texts[i]=record(i+27,"old");strcpy_s(slot.textValues[i].data(),128,"old"); }
    At<int16_t>(slot,300)=6;slot.page.Open(true,false);
    PageInput(slot,vrmenu::Options123Buttons(0x4000));
    check(slot.page.selected==1,"TR123 native Down selects Color Scheme");
    PageInput(slot,vrmenu::Options123Buttons(0x10000));DrawPage123(slot);
    check(slot.page.bright && std::strstr(Field<char*>(slot.texts[2],72),"> Color Scheme: < Brighter >"),"TR123 Right saves and draws the selected bright scheme");
    const auto savedBeforeBack=vrmenu::saved;
    check(!PageInput(slot,vrmenu::Options123Buttons(0x200000)) && vrmenu::saved==savedBeforeBack,"child Back closes without changing saved values");
    uint8_t ringBefore[96];std::memset(&At<uint8_t>(slot,row.transition),0xa5,96);std::memcpy(ringBefore,&At<uint8_t>(slot,row.transition),96);
    Restore(slot);
    check(std::memcmp(ringBefore,&At<uint8_t>(slot,row.transition),96)==0,"cleanup never changes original ring objects, count or selection");
    check(At<int16_t>(slot,300)==1,"all five child text records are released on exit");
    for(int repeat=0;repeat<100;++repeat) {
        for(int i=0;i<5;++i) { slot.texts[i]=record(i+27,"old");strcpy_s(slot.textValues[i].data(),128,"old"); }
        At<int16_t>(slot,300)=6;DrawPage123(slot);ClearTexts(slot);
    }
    check(At<int16_t>(slot,300)==1,"repeated page visits do not exhaust the 64-slot native text pool");
    auto* background=record(13,"background");auto* highlight=record(15,"highlight");
    At<void*>(slot,extra.nativeTexts+13*8)=background;At<void*>(slot,extra.nativeTexts+15*8)=highlight;
    At<void*>(slot,extra.nativeTexts+16*8)=record(16,"left");At<void*>(slot,extra.nativeTexts+17*8)=record(17,"right");
    slot.texts[0]=record(27,"VR Options");strcpy_s(slot.textValues[0].data(),128,"VR Options");
    At<int16_t>(slot,300)=6;At<int32_t>(slot,extra.firstVisible)=3;slot.options123.vr=true;DrawOptionsRow123(slot);
    check(Field<int16_t>(background,58)==206 && Field<float>(highlight,16)==90.f,"extra row fits the native panel and receives its native highlight");
    check(!At<void*>(slot,extra.nativeTexts+16*8) && !At<void*>(slot,extra.nativeTexts+17*8),"virtual selection hides underlying native value arrows");
    At<int32_t>(slot,extra.firstVisible)=0;DrawOptionsRow123(slot);
    check(!slot.texts[0] && Field<int16_t>(background,58)==190,"earlier scrolling pages retain their original layout");
'''+s[b:];p.write_text(s)
