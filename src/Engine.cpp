#include "Engine.h"
#include "Log.h"

#include <windows.h>

namespace tr {
namespace {

uint64_t g_base = 0;

// Sanity-check a few things we know must be true of the right binary before we
// start writing into its .data. Cheap, and it turns "wrong build" from a
// mysterious crash into a log line.
bool LooksLikeTomb456(uint64_t base) {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;

    // The distinctive feature of this build: a single ~232 MB .data section.
    // Every global we touch lives inside it, so if it is not there we would be
    // writing into unmapped memory.
    const DWORD needed = drva::FBO_custom + 0x1000;
    if (nt->OptionalHeader.SizeOfImage < needed) {
        LogF("engine: SizeOfImage 0x%X is too small for our RVAs (need 0x%X)",
             nt->OptionalHeader.SizeOfImage, needed);
        return false;
    }
    return true;
}

} // namespace

bool Bind() {
    if (g_base) return true;

    HMODULE h = GetModuleHandleW(L"tomb456.exe");
    if (!h) h = GetModuleHandleW(nullptr);   // injected into the exe itself
    if (!h) {
        Log("engine: GetModuleHandle failed");
        return false;
    }

    const uint64_t base = reinterpret_cast<uint64_t>(h);
    if (!LooksLikeTomb456(base)) {
        Log("engine: host module does not match the reference build; refusing to patch");
        return false;
    }

    g_base = base;
    LogF("engine: bound to %p (reference base %p, slide %+lld)",
         reinterpret_cast<void*>(base),
         reinterpret_cast<void*>(kReferenceImageBase),
         static_cast<long long>(base - kReferenceImageBase));
    return true;
}

uint64_t Base() { return g_base; }

RenderState& VidState()     { return *reinterpret_cast<RenderState*>(Var(drva::vid_state)); }
RenderState& VidStatePrev() { return *reinterpret_cast<RenderState*>(Var(drva::vid_state_prev)); }
mat4*        Proj()         { return  reinterpret_cast<mat4*>(Var(drva::mProj)); }
mat4&        ViewPacked()   { return *reinterpret_cast<mat4*>(Var(drva::mView_packed)); }
Shader*      Shaders()      { return  reinterpret_cast<Shader*>(Var(drva::shaders)); }
uint32_t*    OglTextures()  { return  reinterpret_cast<uint32_t*>(Var(drva::ogl_textures)); }
uint32_t&    FboCustom()    { return *reinterpret_cast<uint32_t*>(Var(drva::FBO_custom)); }
uint32_t&    FboDefault()   { return *reinterpret_cast<uint32_t*>(Var(drva::FBO_default)); }
OglRenderTarget& Rt()       { return *reinterpret_cast<OglRenderTarget*>(Var(drva::ogl_rt)); }
int32_t&     ScreenWidth()  { return *reinterpret_cast<int32_t*>(Var(drva::gWidth)); }
int32_t&     ScreenHeight() { return *reinterpret_cast<int32_t*>(Var(drva::gHeight)); }
int32_t&     TargetWidth()  { return *reinterpret_cast<int32_t*>(Var(drva::gTargetWidth)); }
int32_t&     TargetHeight() { return *reinterpret_cast<int32_t*>(Var(drva::gTargetHeight)); }

bool IsWorldPass() {
    return VidState().proj == &Proj()[1];
}

} // namespace tr
