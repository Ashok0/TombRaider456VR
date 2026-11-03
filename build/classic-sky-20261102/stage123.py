from pathlib import Path
import json,hashlib,struct,sys
import pefile
root=Path(r'C:\dev\TombRaider123VR');out=Path(__file__).parent
records=[];audit=[]
def stage(relative,text):
    path=root/relative;original=path.read_bytes() if path.exists() else None
    if original is not None:
        before=out/'123-before'/relative;before.parent.mkdir(parents=True,exist_ok=True);before.write_bytes(original)
    dest=out/'123-staged'/relative;dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_bytes(text.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))
    records.append(dict(path=relative,sha256=hashlib.sha256(original).hexdigest() if original is not None else None))

rows=[(2,'PDB',0x6a4b4915,0x9be00,0x148dc,0x8f740),
      (3,'PDB',0x6a4b490d,0xe60f0,0x29b9e,0xd3aa0),
      (2,'build/current-gold-verify',0x6a4b7c3f,0x9bc80,0x148ec,0x8f210),
      (3,'build/current-gold-verify',0x6a4b7c37,0xe7dc0,0x29c5e,0xd5340),
      (2,'build/current-october-verify',0x6aa15696,0x9bae0,0x143bc,0x8f080),
      (3,'build/current-october-verify',0x6aa1568d,0xe8aa0,0x29d6e,0xd6010)]
for game,folder,stamp,entry,ret,flush in rows:
    path=root/folder/f'tomb{game}.dll';pe=pefile.PE(str(path),fast_load=True);data=pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp==stamp
    assert data[ret-5]==0xe8 and ret+struct.unpack_from('<i',data,ret-4)[0]==entry
    assert data[entry:entry+5]==bytes.fromhex('48 89 5c 24 18')
    audit.append(dict(game=game,path=str(path),stamp=hex(stamp),entry=hex(entry),skyReturn=hex(ret),flush=hex(flush),prologue=data[entry:entry+5].hex(' ')))
header='''#pragma once
#include <cstdint>
namespace tr::sky {
// ins_mesh called from the Classic horizon branch of DrawRooms, plus S_CmdFlush.
// RVAs verified against each supported DLL, including the exact E8 call site.
// TR1 has no separate Classic horizon mesh; its DrawSkyHD hook remains sufficient.
struct Binding { uint32_t timestamp,entry,skyReturn,flush; };
constexpr Binding Bindings[] = {
'''
for _,_,stamp,entry,ret,flush in rows:header+=f'    {{0x{stamp:08X},0x{entry:X},0x{ret:X},0x{flush:X}}},\n'
header+='''};
inline const Binding* Find(uint32_t timestamp) {
    for(const auto& binding:Bindings) if(binding.timestamp==timestamp) return &binding;
    return nullptr;
}
} // namespace tr::sky
'''
stage('src/SkyBindings.h',header)
text=(root/'src/Sky.cpp').read_text()
text=text.replace('#include "Sky.h"','#include "Sky.h"\n#include "SkyBindings.h"\n#include <intrin.h>\n#include <cstring>')
text=text.replace('hook::InlineHook g_hDrawSkyHD;','hook::InlineHook g_hDrawSkyHD,g_hClassicMesh;\nconst sky::Binding* g_classic=nullptr;')
old='''void __cdecl Detour_DrawSkyHD() {
    g_inSky = true;
    g_hDrawSkyHD.Original<Fn_DrawSkyHD>()();
    g_inSky = false;
}'''
new='''struct SkyScope {
    bool previous=g_inSky;
    explicit SkyScope() { g_inSky=true; }
    ~SkyScope() { g_inSky=previous; }
};
void __cdecl Detour_DrawSkyHD() {
    const SkyScope scope;
    g_hDrawSkyHD.Original<Fn_DrawSkyHD>()();
}
using Fn_InsMesh=void(__cdecl*)(void*,int32_t);
void DrawClassicMesh(void* mesh,int32_t clip,uint64_t caller) {
    const auto original=g_hClassicMesh.Original<Fn_InsMesh>();
    if(caller!=g_boundBase+g_classic->skyReturn) { original(mesh,clip);return; }
    // ins_mesh queues geometry: flush preceding world work outside the scope,
    // then the horizon inside it. No ordinary object can inherit infinity depth.
    const auto flush=reinterpret_cast<Fn_DrawSkyHD>(g_boundBase+g_classic->flush);
    flush();
    const SkyScope scope;
    original(mesh,clip);
    flush();
}
void __cdecl Detour_InsMesh(void* mesh,int32_t clip) {
    DrawClassicMesh(mesh,clip,reinterpret_cast<uint64_t>(_ReturnAddress()));
}
bool InstallClassic(const sky::Binding& b,uint64_t base) {
    const auto* call=reinterpret_cast<const uint8_t*>(base+b.skyReturn-5);
    int32_t displacement=0;std::memcpy(&displacement,call+1,sizeof(displacement));
    if(*call!=0xE8 || base+b.skyReturn+displacement!=base+b.entry) {
        Log("sky: Classic horizon call-site mismatch; keeping native drawing");return false;
    }
    const uint8_t prologue[]={0x48,0x89,0x5C,0x24,0x18};
    return g_hClassicMesh.Install(reinterpret_cast<void*>(base+b.entry),
        reinterpret_cast<void*>(&Detour_InsMesh),5,prologue,sizeof(prologue),"Classic horizon mesh");
}'''
assert old in text;text=text.replace(old,new)
text=text.replace('    g_hDrawSkyHD.Remove();','    g_hClassicMesh.Remove();g_classic=nullptr;\n    g_hDrawSkyHD.Remove();')
text=text.replace('    if (!g_loggedFirst) {','''    g_classic=sky::Find(d->timestamp);
    if(g_classic) {
        if(InstallClassic(*g_classic,base))
            LogF("sky: Classic horizon in %S uses optical infinity",d->module);
        else g_classic=nullptr;
    }
    if (!g_loggedFirst) {''')
stage('src/Sky.cpp',text)
text=(root/'src/Sky.h').read_text().replace("draw the remaster's HD sky",'draw Classic and Remastered skies')
text=text.replace('// The hook wraps DrawSkyHD and SkyPassActive() is true for every draw it\n// issues.', '// DrawSkyHD is scoped directly. TR2/3 Classic horizons use the verified\n// DrawRooms -> ins_mesh call site, with command flushes bracketing that mesh.\n// TR1 has no separate Classic horizon. SkyPassActive() stays true through\n// actual sky submission, including deferred commands.')
text=text.replace('DrawSkyHD hook','sky hooks').replace('// True while the original DrawSkyHD is on the stack.','// True only during a verified sky draw/command flush.')
stage('src/Sky.h',text)
text=(root/'src/Hooks.cpp').read_text().replace('sky: first DrawSkyHD draw','sky: first sky draw')
stage('src/Hooks.cpp',text)
(out/'123-source-manifest.json').write_text(json.dumps(records,indent=2))
(out/'123-address-audit.json').write_text(json.dumps(audit,indent=2))
print('Verified six Classic call sites; staged four TR1-3 source files.')
