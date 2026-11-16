#pragma once
#include <string>

namespace tr::mirror {
// TR4/5 creature palettes are world-oriented, relative to the native camera.
// Keep lighting, textures and world positions intact; only the observer used
// for rim/Fresnel/environment highlights changes inside a reflected draw.
inline bool PatchObserver(const std::string& vertex,std::string& fragment) {
    if(vertex.find("vPos = p.xyz;")==std::string::npos ||
       vertex.find("vWorldPos = p.xyz + vec3(uViewMatrix[0].w, uViewMatrix[1].w, uViewMatrix[2].w);")==std::string::npos ||
       vertex.find("vec4 j = aLight;")==std::string::npos ||
       fragment.find("vec3 pos = vPos;")==std::string::npos ||
       fragment.find("uMirrorObserver")!=std::string::npos) return false;
    const auto main=fragment.find("void main()");
    if(main==std::string::npos || fragment.find("normalize(-pos)",main)==std::string::npos) return false;
    std::string patched=fragment;
    size_t at=main;
    const std::string replacement="normalize(uMirrorObserver.xyz - pos)";
    while((at=patched.find("normalize(-pos)",at))!=std::string::npos) {
        patched.replace(at,15,replacement);at+=replacement.size();
    }
    patched.insert(main,"uniform vec4 uMirrorObserver;\n");
    fragment=std::move(patched);return true;
}
} // namespace tr::mirror
