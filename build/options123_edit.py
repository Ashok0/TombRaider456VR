from pathlib import Path
p=Path('C:/dev/TombRaider456VR/src/VRNativeMenu.cpp');s=p.read_text()
s=s.replace('    uint8_t bytes[5];','    uint8_t bytes[5];\n    uint32_t nativeTexts=0,firstVisible=0,textPool=0,textBuffers=0;')
s=s.replace('    // First member shares Slot\'s natural pointer alignment.\n    std::array<uint8_t,3272> item{};\n','')
s=s.replace('    bool scope=false,attempted=false,ringOpen=false;\n    std::array<void*,6> list{};','    bool scope=false,attempted=false;\n    vrmenu::Options123Selection options123;')
a=s.index('void PrepareRing(');b=s.index('bool InsertOption6(',a)
s=s[:a]+'''void ClearNativeTexts123(Slot& s) {
    // These are the native Options page's twenty owned text records. Releasing
    // them mirrors its own close path, without closing the inventory object.
    for(int i=0;i<20;++i) {
        auto& p=At<void*>(s,s.extra->nativeTexts+i*8);
        ReleaseText123(s,p);p=nullptr;
    }
}
void DrawOptionsRow123(Slot& s) {
    const bool visible=At<int32_t>(s,s.extra->firstVisible)==3;
    auto* background=At<void*>(s,s.extra->nativeTexts+13*8);
    if(ValidText123(s,background)) Field<int16_t>(background,58)=visible ? 206 : 190;
    if(!visible) { ClearTexts(s);return; }
    WriteText123(s,0,Label,90,0);
    if(s.options123.vr) {
        auto* highlight=At<void*>(s,s.extra->nativeTexts+15*8);
        if(ValidText123(s,highlight)) Field<float>(highlight,16)=90.f;
        // The original selection stays at row 12. Hide that row's value arrows
        // while the extra row is selected, and consume its change inputs.
        for(int i:{16,17}) {
            auto& p=At<void*>(s,s.extra->nativeTexts+i*8);ReleaseText123(s,p);p=nullptr;
        }
    }
}
template<int G> int Options123() {
    auto& s=g_slots[G];const auto& r=*s.row;const auto& e=*s.extra;
    auto original=s.title.Original<int(*)()>();
    if(!Available()) { ClearTexts(s);s.page={};s.options123={};return original(); }
    if(s.page.open) {
        PageInput(s,vrmenu::Options123Buttons(At<uint32_t>(s,r.dbinput)));
        At<uint32_t>(s,r.dbinput)=At<uint32_t>(s,r.input)=0;
        if(s.page.open) { DrawPage123(s);return 0; }
        // Consume the child Back. Rebuild the parent with VR Options selected;
        // a second Back uses the native parent close path.
        ClearTexts(s);
    }
    auto& input=At<uint32_t>(s,r.dbinput);
    if(s.options123.Input(At<int16_t>(s,e.selection),input)) {
        ClearTexts(s);ClearNativeTexts123(s);OpenPage(s);
        At<uint32_t>(s,r.input)=input=0;DrawPage123(s);return 0;
    }
    const int result=original();
    if(result || At<int32_t>(s,e.subpage)!=0) {
        ClearTexts(s);s.options123={};
    } else DrawOptionsRow123(s);
    return result;
}

'''+s[b:]
a=s.index('        void* ring=',s.index('void Restore('));b=s.index('    } else if(r.game==6',a)
s=s[:a]+s[b:]
s=s.replace('s.attempted=s.scope=s.ringOpen=false','s.attempted=s.scope=false').replace('s.selection={};s.page={};s.texts={};','s.selection={};s.options123={};s.page={};s.texts={};')
s=s.replace('e.active,e.optionsMenu}', 'e.active,e.optionsMenu,e.nativeTexts,e.firstVisible,e.textPool,e.textBuffers}')
s=s.replace('    const auto target=HookRva(s);','    const bool hasDraw=r.game==4 || r.game==5;\n    const auto target=HookRva(s);')
s=s.replace('(r.draw && std::memcmp(', '(hasDraw && std::memcmp(')
s=s.replace('&RingInit<0>','&Options123<0>').replace('&RingInit<1>','&Options123<1>').replace('&RingInit<2>','&Options123<2>')
s=s.replace('reinterpret_cast<void*>(&InventoryOptions<0>),reinterpret_cast<void*>(&InventoryOptions<1>),reinterpret_cast<void*>(&InventoryOptions<2>),','nullptr,nullptr,nullptr,')
s=s.replace('if(r.draw && !s.draw.Install','if(hasDraw && !s.draw.Install')
s=s.replace('            if(i<3) UpdateRing(s);\n','')
s=s.replace('s.selection={};s.page={};s.ringOpen=false;', 's.selection={};s.options123={};s.page={};')
s=s.replace('native Controller Type page installed','native Options submenu installed')
p.write_text(s)
print('Removed all extra ring-object hooks and mutations; inserted native Options child.')
