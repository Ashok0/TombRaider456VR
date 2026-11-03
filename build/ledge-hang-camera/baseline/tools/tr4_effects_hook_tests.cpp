// Exercise the actual loader/update detours using native-call substitutes.
#include "../src/TR4Effects.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
int checks=0;
void Check(bool value,const char* what) {++checks;if(!value){std::printf("FAIL %s\n",what);std::exit(1);}}
void Log(const char*) {}
void LogF(const char*,...) {}
namespace hook {
InlineHook::~InlineHook() {}
bool InlineHook::Install(void* target,void*,size_t,const uint8_t*,size_t,const char*,const int*,size_t) {
    m_trampoline=target;m_installed=true;return true;
}
void InlineHook::Remove() {m_installed=false;m_trampoline=nullptr;}
}
namespace tr {
Config testConfig;
Layout testLayout{};
uint8_t appMemory[64]{};
uint32_t textures[38]{};
const Config& Cfg() {return testConfig;}
const Layout& L() {return testLayout;}
uint64_t Base() {return reinterpret_cast<uint64_t>(appMemory);}
uint32_t* OglTextures() {return textures;}
int CurrentGame() {Check(false,"loader must not consult stale current game");return 1;}
}
char resourceGame='4';
int loadCalls=0,updateCalls=0;
bool nested=false;
tr::effects::Texture expectedKind;
const void* inputData=reinterpret_cast<void*>(uintptr_t(0x1234));
void __cdecl GetPath(const char* name,char* out) {out[0]=resourceGame;out[1]='\\';std::strcpy(out+2,name);}
void __cdecl NativeUpdate(int32_t index,int32_t layer,int32_t mip,const void* data) {
    ++updateCalls;Check(index==6 && layer==1 && mip==0 && data==inputData,"original upload arguments retained without GL");
}
int32_t __cdecl NativeLoad(const char*,int32_t index,int32_t layer) {
    ++loadCalls;
    Check(tr::g_loading && tr::g_loading->kind==expectedKind,"resolved resource game controls scope");
    auto* scope=tr::g_loading;
    if(nested) {
        nested=false;auto saved=expectedKind;expectedKind=tr::effects::Texture::None;
        Check(tr::Detour_EffectTextureLoad("TEX\\OTHER.DDS",6,1)==42,"nested native return preserved");
        expectedKind=saved;Check(tr::g_loading==scope && scope->kind==saved,"nested scope restored");
    }
    tr::Detour_EffectTextureUpdate(index,layer,0,inputData);return 42;
}
int main() {
    using namespace tr;
    *reinterpret_cast<void (**)(const char*,char*)>(appMemory)=GetPath;
    g_effectTextureLoad.Install(reinterpret_cast<void*>(&NativeLoad),nullptr,0,nullptr,0,"");
    g_effectTextureUpdate.Install(reinterpret_cast<void*>(&NativeUpdate),nullptr,0,nullptr,0,"");
    for(char game:{'4','5','6'}) for(bool enabled:{false,true}) for(bool effects:{false,true}) {
        resourceGame=game;testConfig.enabled=enabled;testConfig.tr4Effects=effects;
        for(const char* name:{"TEX\\3115.DDS","tex\\effect.dds","TEX\\OTHER.DDS","TEX\\3115.DDS.bak"}) {
            expectedKind=enabled && effects && game=='4' ? effects::Identify((std::string("4\\")+name).c_str()):effects::Texture::None;
            Check(Detour_EffectTextureLoad(name,6,1)==42,"native load result preserved");
            Check(g_loading==nullptr,"scope cleared after load");
        }
    }
    resourceGame='4';testConfig.enabled=testConfig.tr4Effects=true;expectedKind=effects::Texture::Atlas;nested=true;
    Detour_EffectTextureLoad("TEX\\EFFECT.DDS",6,1);
    Check(loadCalls==50 && updateCalls==50,"one native load and upload per call, including nested");
    Detour_EffectTextureUpdate(6,1,0,inputData);Check(updateCalls==51,"unscoped upload forwarded once");
    Check(g_loading==nullptr,"no stale scope after nested load");
    std::printf("PASS: %d loader routing checks\n",checks);
}
