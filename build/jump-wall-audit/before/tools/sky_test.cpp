// Focused sky-hook regression. No game process or live patching is involved.
#include <cstdint>
#include <cstddef>
#define private public
#include "../src/InlineHook.h"
#undef private
#include "../src/Sky.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <utility>

namespace {
int checks=0;
void Check(bool ok,const char* label) {
    ++checks;if(!ok) { std::printf("FAIL: %s\n",label);std::exit(1); }
}
tr::Config settings;
const tr::GameDllLayout* activeDll=nullptr;
uint64_t activeBase=0;
std::vector<int> queued;
std::vector<std::pair<int,bool>> submitted;
void __cdecl Flush() {
    for(int mesh:queued) submitted.push_back({mesh,tr::SkyPassActive()});
    queued.clear();
}
void __cdecl NativeClassic() {
    Check(tr::SkyPassActive(),"Classic native scope active");
    queued.push_back(2);Flush();queued.push_back(3); // Early and deferred submissions.
}
void __cdecl NativeHD() { Check(tr::SkyPassActive(),"HD retains infinity scope"); }
int skyMesh,worldMesh;
void* nativeSky=&skyMesh;
void __cdecl NativeRoom(void* mesh,uint32_t flags,uint32_t pass,uint8_t mirror) {
    Check(flags==4 && pass==2 && mirror==1,"TR6 room arguments preserved");
    Check(tr::SkyPassActive()==(mesh==nativeSky && mesh),"only the registered TR6 sky mesh receives infinity depth");
}
}
void Log(const char*) {}
void LogF(const char*,...) {}
namespace tr {
const Config& Cfg() { return settings; }
const GameDllLayout* GameDllBound() { return activeDll; }
uint64_t GameDllBase() { return activeBase; }
int CurrentGame() { return -1; }
}
namespace hook {
InlineHook::~InlineHook() {}
void InlineHook::Remove() { m_trampoline=nullptr; }
bool InlineHook::Install(void*,void*,size_t,const uint8_t*,size_t,const char*,const int*,size_t) { return true; }
}
int main() {
    using namespace tr;
    const auto base=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[&](const void* p) { const auto n=reinterpret_cast<uint64_t>(p)-base;Check(n<=UINT32_MAX,"fixture RVA fits");return uint32_t(n); };
    Check(!sky::Find(0),"unknown builds rejected");
    for(const auto& b:sky::Bindings) Check(sky::Find(b.timestamp)==&b && b.entry && b.stolen>=5,"every supported build has an exact hook descriptor");
    sky::Binding binding{};binding.flush=rva(reinterpret_cast<void*>(&Flush));binding.skyRoom=rva(&nativeSky);
    g_boundBase=base;g_classic=&binding;g_hClassicSky.m_trampoline=reinterpret_cast<void*>(&NativeClassic);
    g_hDrawSkyHD.m_trampoline=reinterpret_cast<void*>(&NativeHD);
    for(int eye:{0,1}) {
        (void)eye;submitted.clear();queued={1};g_inSky=false;
        Detour_ClassicSky();queued.push_back(4);Flush();
        Check(submitted==std::vector<std::pair<int,bool>>{{1,false},{2,true},{3,true},{4,false}},
              "queued world draws keep stereo depth; all sky commands flush at infinity; following world draws recover");
        Check(!SkyPassActive(),"Classic scope cannot leak between eyes");
        Detour_DrawSkyHD();Check(!SkyPassActive(),"HD scope restores world rendering");
        g_inSky=true;Detour_DrawSkyHD();Check(SkyPassActive(),"nested HD scope retains the outer sky pass");g_inSky=false;
    }
    g_tr6Base=base;g_tr6=&binding;g_hTr6Sky.m_trampoline=reinterpret_cast<void*>(&NativeRoom);
    for(void* mesh:{static_cast<void*>(&worldMesh),static_cast<void*>(&skyMesh),static_cast<void*>(nullptr)}) {
        Detour_Tr6DrawRoom(mesh,4,2,1);Check(!SkyPassActive(),"TR6 sky state is draw-local");
    }
    nativeSky=nullptr;Detour_Tr6DrawRoom(&worldMesh,4,2,1);
    SkyShutdown();Check(!SkyPassActive() && !g_tr6 && !g_tr6Base,"shutdown clears TR6 binding and scope");
    GameDllLayout tr4{},tr5{};static const uint8_t prologue[]={0x48,0x89,0x5c,0x24,0x18};
    tr4.module=L"tomb4.dll";tr4.name="TR4 fixture";tr4.timestamp=0x68C12FDA;tr4.drawSkyHD=1;tr4.drawSkyHDPrologue=prologue;
    tr5=tr4;tr5.module=L"tomb5.dll";tr5.name="TR5 fixture";tr5.timestamp=0x68C12FE9;
    settings.enabled=settings.skyAtInfinity=true;activeBase=base;activeDll=&tr4;SkyUpdate();
    Check(g_boundDll==&tr4 && g_classic==sky::Find(tr4.timestamp),"TR4 binds its own Classic phase");
    activeDll=&tr5;SkyUpdate();
    Check(g_boundDll==&tr5 && g_classic==sky::Find(tr5.timestamp),"switching games replaces the Classic binding");
    settings.skyAtInfinity=false;SkyUpdate();
    Check(!g_boundDll && !g_classic && !SkyPassActive(),"INI opt-out unbinds both HD and Classic hooks");
    std::printf("TR4-6 sky hooks: %d checks passed\n",checks);
}
