from pathlib import Path
root=Path('C:/dev/TombRaider456VR')
(root/'tools/vr_menu_tests.cpp').write_text(r'''// Isolated native menu input and INI persistence checks; no game/VR runtime.
#include "../src/Config.cpp"
#include "../src/VRMenuModel.h"
#include "../src/VRNativeMenuModel.h"
#include <fstream>
#include <limits>
#include <cstdio>
namespace tr {
uint64_t Base() { return 0; }
int AppFlag(uint32_t) { return 0; }
const Layout& L() { static Layout layout{};return layout; }
}
int main() {
    using namespace tr::vrmenu;
    int n=0;auto check=[&](bool yes,const char* why) { ++n;if(!yes) { std::printf("FAIL: %s\n",why);std::exit(1); } };
    wchar_t cwd[MAX_PATH]{};GetCurrentDirectoryW(MAX_PATH,cwd);
    const std::wstring ini=std::wstring(cwd)+L"\\vr-menu-test.ini";
    { std::ofstream f("vr-menu-test.ini");f<<"; keep comment\n[VR]\nValveIndexControllers=0\nValveIndexGripPress=0.63\nValveIndexGripRelease=0.31\nDpadShift=0\nFirstPersonTurnDegreesPerSecond=123\nFirstPersonMotionGunPitchDegrees=-17\n[Other]\nCustom=77\n"; }
    tr::LoadConfig(ini.c_str());
    check(SettingCount==1 && std::wstring(Settings()[0].label)==L"Controller Type","only Controller Type is exposed");
    auto original=ReadSettings(),next=original;std::wstring error;next[0]=1;
    check(SaveSettings(next,original,error),"Index selection saves");
    check(ReadSettings()[0]==1,"Index selection persists");
    check(GetFileAttributesW((ini+L".vr-options.bak").c_str())!=INVALID_FILE_ATTRIBUTES,"previous INI backed up");
    std::ifstream f("vr-menu-test.ini");std::string contents((std::istreambuf_iterator<char>(f)),{});f.close();
    for(const auto* value:{"keep comment","ValveIndexGripPress=0.63","ValveIndexGripRelease=0.31","DpadShift=0","FirstPersonTurnDegreesPerSecond=123","FirstPersonMotionGunPitchDegrees=-17","Custom=77"})
        check(contents.find(value)!=std::string::npos,"hidden and unrelated settings remain unchanged");
    original=ReadSettings();next[0]=0;check(SaveSettings(next,original,error) && ReadSettings()[0]==0,"Touch selection saves with an existing backup");
    original=ReadSettings();next[0]=.5f;check(!SaveSettings(next,original,error),"non-boolean controller type rejected");
    next[0]=std::numeric_limits<float>::quiet_NaN();check(!SaveSettings(next,original,error),"nonfinite controller type rejected");
    check(ReadSettings()==original,"rejected writes preserve INI");
    ControllerPage page;
    check(page.Input(0x100000)==Action::None,"closed page leaves native Select alone");
    page.Open(false);check(std::string(page.Value())=="Meta Touch","Touch name uses requested text");
    check(page.Input(0x40000000)==Action::Change,"native Left adjusts controller type");
    check(page.Input(0x80000000)==Action::Change,"native Right adjusts controller type");
    check(page.Input(0x100000)==Action::Change,"native Select adjusts controller type");
    check(page.Input(0x30000000)==Action::None,"one-setting page has no other selectable settings");
    check(page.Input(0x30100000)==Action::Change,"Select with vertical input changes only Controller Type");
    check(page.Input(0x300000)==Action::Back && !page.open,"Back takes precedence and returns one level");
    page.Open(true);check(std::string(page.Value())=="Valve Index","Index name uses requested text");
    OptionsSelection selection;uint64_t input=0x20000000;
    check(!selection.Input(12,input) && selection.vr && input==0,"Down after native setting 12 selects VR Options");
    input=0x40000000;check(!selection.Input(12,input) && input==0,"VR row cannot toggle underlying native setting");
    input=0x100000;check(selection.Input(12,input) && input==0,"VR Select opens child without native action");
    input=0x10000000;selection.Input(12,input);check(!selection.vr && input==0,"Up returns to native setting 12 without skipping it");
    for(int choice=1;choice<=12;++choice) { input=0x100000;check(!selection.Input(choice,input) && input==0x100000,"all original native actions remain available"); }
    input=0x20100000;selection.Input(12,input);check(selection.vr && input==0,"Down plus Select cannot activate hidden native action");
    input=0x200000;selection.Input(12,input);check(input==0x200000,"parent Back remains native");
    input=0;selection.Input(1,input);check(!selection.vr,"returning to a reset Options list clears virtual selection");
    std::printf("PASS: %d Controller Type checks\n",n);
}
''')
(root/'tools/vr_native_menu_tests.cpp').write_text(r'''// Tests native menu ownership, cleanup and save failures without loading games.
#include "../src/VRNativeMenu.cpp"
#include <cstdio>
#include <cstdlib>
namespace tr {
uint64_t Base() { return 0; }
int AppFlag(uint32_t) { return 0; }
const Layout& L() { static Layout layout{};return layout; }
const Config& Cfg() { static Config config{};return config; }
VRSystem& VR() { static VRSystem runtime;return runtime; }
namespace vrmenu {
bool saveOK=true;Values saved{};
Values ReadSettings() { return saved; }
bool SaveSettings(const Values& v,const Values&,std::wstring&) { if(saveOK) saved=v;return saveOK; }
}
}
int main() {
    using namespace tr;
    int n=0;auto check=[&](bool yes,const char* why) { ++n;if(!yes) { std::printf("FAIL: %s\n",why);std::exit(1); } };
    Slot slot;
    alignas(8) uint8_t items[5][3272]{};void* list[5];
    for(int i=0;i<5;++i) { list[i]=items[i];Field<int16_t>(items[i],8)=int16_t(95+i); }
    PrepareRing(slot,list);
    for(int i=0;i<5;++i) check(slot.list[i]==list[i],"native ring objects remain unchanged");
    check(slot.list[5]==slot.item.data(),"VR ring entry has independent storage");
    check(std::strcmp(Field<const char*>(slot.item.data(),0),"VR Options")==0,"ring label uses exact capitalization");
    check(Field<int32_t>(slot.item.data(),3264)==1,"new ring object recomputes its own transform");
    alignas(8) uint8_t menu[160]{};Option6 options[12]{};
    for(int i=0;i<12;++i) { options[i].next=i<11 ? &options[i+1] : nullptr;options[i].last=i ? &options[i-1] : nullptr; }
    Field<Option6*>(menu,120)=options;Field<uint8_t>(menu,106)=12;Field<uint8_t>(menu,107)=3;
    check(InsertOption6(slot,menu),"TR6 adds VR Options to native Options list");
    check(Field<uint8_t>(menu,106)==13 && Field<uint8_t>(menu,107)==3,"original Options selection is not shifted");
    check(options[11].next==&slot.option && slot.option.last==&options[11] && !slot.option.next,"VR Options follows existing options");
    check(std::strcmp(slot.option.text,"VR Options")==0 && slot.option.textId==-1,"literal label survives localization");
    check(InsertOption6(slot,menu) && Field<uint8_t>(menu,106)==13,"no duplicate row on repeated draws");
    alignas(8) uint8_t memory[512]{};Row row{};Extra extra{};row.game=6;extra.optionsMenu=256;
    slot.row=&row;slot.extra=&extra;slot.module=reinterpret_cast<HMODULE>(memory);At<void*>(slot,256)=menu;
    Field<uint8_t>(menu,107)=12;Restore(slot);
    check(!options[11].next && Field<uint8_t>(menu,106)==12,"cleanup restores native list");
    check(Field<uint8_t>(menu,107)==11,"cleanup leaves a valid native selection");
    OpenPage(slot);check(slot.page.open && !slot.page.index,"page opens from saved controller type");
    PageInput(slot,0x80000000);check(slot.page.index && vrmenu::saved[0]==1,"change saves before updating display");
    vrmenu::saveOK=false;PageInput(slot,0x40000000);
    check(slot.page.index && slot.page.saveFailed && vrmenu::saved[0]==1,"failed save retains previous value and shows native error");
    check(!PageInput(slot,0x200000),"Back closes native child page");
    row.game=1;extra.textCount=300;At<int16_t>(slot,300)=2;
    alignas(8) uint8_t text[88]{};char label[512]="VR Options";
    Field<uint32_t>(text,0)=1;Field<char*>(text,72)=label;slot.texts[0]=text;
    strcpy_s(slot.textValues[0].data(),128,"VR Options");ClearTexts(slot);
    check(!(Field<uint32_t>(text,0)&1) && At<int16_t>(slot,300)==1,"closing page releases its persistent native text");
    Field<uint32_t>(text,0)=1;strcpy_s(label,"Another menu");slot.texts[0]=text;ClearTexts(slot);
    check((Field<uint32_t>(text,0)&1) && At<int16_t>(slot,300)==1,"cleanup cannot remove a reused native text slot");
    std::printf("PASS: %d native submenu checks\n",n);
}
''')
