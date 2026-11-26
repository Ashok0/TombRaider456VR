#include "Sky.h"
#include "SkyBindings.h"
#include "Engine.h"
#include "GameDll.h"
#include "Config.h"
#include "InlineHook.h"
#include "Log.h"

#include <cstdint>
#include <windows.h>

namespace tr {
namespace {

hook::InlineHook g_hDrawSkyHD;
hook::InlineHook g_hClassicSky,g_hTr6Sky;
const sky::Binding* g_classic=nullptr;
const sky::Binding* g_tr6=nullptr;
uint64_t g_tr6Base=0,g_tr6FailedBase=0;

typedef void (__cdecl* Fn_DrawSkyHD)(void);

// The expected prologue travels with the row: `mov [rsp+N], rbx`, 5 bytes, PIC,
// instruction-aligned, but N is 8 in the debug DLLs and 0x18 in retail.
constexpr size_t kDrawSkyHDPrologueLen = 5;

const GameDllLayout* g_boundDll  = nullptr;
uint64_t             g_boundBase = 0;
uint64_t             g_failedBase = 0;

bool g_inSky       = false;
bool g_loggedFirst = false;

struct SkyScope {
    bool previous=g_inSky;
    explicit SkyScope(bool enable=true) { g_inSky=previous || enable; }
    ~SkyScope() { g_inSky=previous; }
};

void __cdecl Detour_DrawSkyHD() {
    const SkyScope scope;
    g_hDrawSkyHD.Original<Fn_DrawSkyHD>()();
}

void __cdecl Detour_ClassicSky() {
    // Classic sky geometry is queued. Drain earlier world commands BEFORE
    // entering the scope, then flush the sky before returning to world draws.
    const auto flush=reinterpret_cast<Fn_DrawSkyHD>(g_boundBase+g_classic->flush);
    flush();
    const SkyScope scope;
    g_hClassicSky.Original<Fn_DrawSkyHD>()();
    flush();
}

using Fn_Tr6DrawRoom=void(__cdecl*)(void*,uint32_t,uint32_t,uint8_t);
void __cdecl Detour_Tr6DrawRoom(void* mesh,uint32_t flags,uint32_t pass,uint8_t mirror) {
    // The PDB identifies this pointer as SYS_DRAW_BUFFER::pSkyRoom (+0x420).
    // Rooms, characters and reflections share DrawRoom: only this exact mesh
    // is sky, in Classic as well as Remastered graphics.
    const void* skyMesh=*reinterpret_cast<void**>(g_tr6Base+g_tr6->skyRoom);
    const SkyScope scope(mesh && mesh==skyMesh);
    g_hTr6Sky.Original<Fn_Tr6DrawRoom>()(mesh,flags,pass,mirror);
}

void Tr6Update() {
    const auto base=reinterpret_cast<uint64_t>(GetModuleHandleW(L"tomb6.dll"));
    const bool want=Cfg().enabled && Cfg().skyAtInfinity && CurrentGame()==2 && base;
    if(g_tr6Base && (!want || base!=g_tr6Base)) {
        g_hTr6Sky.Remove();g_tr6Base=0;g_tr6=nullptr;
    }
    if(!want || g_tr6Base || base==g_tr6FailedBase) return;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const auto* binding=sky::Find(nt->FileHeader.TimeDateStamp);
    if(!binding || !binding->skyRoom) { g_tr6FailedBase=base;return; }
    g_tr6=binding;g_tr6Base=base;
    if(!g_hTr6Sky.Install(reinterpret_cast<void*>(base+binding->entry),
        reinterpret_cast<void*>(&Detour_Tr6DrawRoom),binding->stolen,
        binding->prologue,binding->stolen,"TR6 sky mesh")) {
        g_tr6Base=0;g_tr6=nullptr;g_tr6FailedBase=base;return;
    }
    Log("sky: TR6 Classic/Remastered sky mesh uses optical infinity; ordinary room depth retained");
}

bool Install(const GameDllLayout& d, uint64_t base) {
    if (d.drawSkyHD == 0 || !d.drawSkyHDPrologue) return false;
    return g_hDrawSkyHD.Install(
        reinterpret_cast<void*>(base + d.drawSkyHD),
        reinterpret_cast<void*>(&Detour_DrawSkyHD),
        5, d.drawSkyHDPrologue, kDrawSkyHDPrologueLen,
        "DrawSkyHD");
}

void Remove() {
    g_inSky = false;
    g_hClassicSky.Remove();g_classic=nullptr;
    g_hDrawSkyHD.Remove();
    g_boundDll  = nullptr;
    g_boundBase = 0;
}

} // namespace

void SkyUpdate() {
    Tr6Update();
    const GameDllLayout* d = GameDllBound();
    const uint64_t base = GameDllBase();

    const bool want = Cfg().enabled && Cfg().skyAtInfinity && d && base
                   && d->drawSkyHD != 0;

    // Rebind whenever the DLL or its base changes, same as PortalCullUpdate --
    // the player can switch games from the title screen without restarting.
    if (g_boundDll && (!want || d != g_boundDll || base != g_boundBase)) {
        LogF("sky: unhooking %S", g_boundDll->module);
        Remove();
    }
    if (!want || g_boundDll) return;
    if (base == g_failedBase) return;

    g_boundDll  = d;
    g_boundBase = base;
    if (!Install(*d, base)) {
        LogF("sky: DrawSkyHD could not be hooked in %S -- the sky keeps its "
             "mesh-radius stereo depth. A prologue mismatch here means the "
             "game was patched; re-run tools\\verify_addresses.py.", d->module);
        Remove();
        g_failedBase = base;
        return;
    }
    g_classic=sky::Find(d->timestamp);
    if(g_classic && g_classic->flush) {
        const auto& b=*g_classic;
        const int rip=b.rip;
        if(g_hClassicSky.Install(reinterpret_cast<void*>(base+b.entry),
            reinterpret_cast<void*>(&Detour_ClassicSky),b.stolen,b.prologue,b.stolen,
            "Classic SkyDrawPhase",rip>=0 ? &rip : nullptr,rip>=0 ? 1 : 0))
            LogF("sky: Classic horizon/cloud layers in %S use optical infinity",d->module);
        else g_classic=nullptr;
    }
    if (!g_loggedFirst) {
        g_loggedFirst = true;
        LogF("sky: hooked %S (%s) -- sky draws use rotation-only eye transform "
             "(optical infinity) and far-plane depth", d->module, d->name);
    } else {
        LogF("sky: hooked %S (%s)", d->module, d->name);
    }
}

void SkyShutdown() {
    if (g_boundDll) Remove();
    g_hTr6Sky.Remove();g_tr6Base=g_tr6FailedBase=0;g_tr6=nullptr;
    g_inSky=false;
    g_loggedFirst = false;
    g_failedBase  = 0;
}

bool SkyPassActive() {
    return g_inSky;
}

} // namespace tr
