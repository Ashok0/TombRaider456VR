#pragma once
#include <string>

namespace tr::keypad {
// The joint IDs select the authored raised/depressed copies of one button.
// A zero uniform leaves all other objects and the native menu unchanged.
inline bool PatchLight(std::string& vertex,std::string& fragment) {
    const std::string weights="vec4 w = aColor;",finish="fragColor = d;";
    if(vertex.find("uKeypadLight")!=std::string::npos ||
       vertex.find("ivec3 index = ivec3(j.xyz);")==std::string::npos ||
       vertex.find(weights)==std::string::npos || fragment.find(finish)==std::string::npos ||
       fragment.find("texture(sTex0, vec3(vTexCoord.xy.xy, vLayer))")==std::string::npos ||
       fragment.find("in float vFog;")==std::string::npos) return false;
    const auto vm=vertex.find("void main()"),fm=fragment.find("void main()");
    if(vm==std::string::npos || fm==std::string::npos) return false;
    vertex.insert(vm,R"GLSL(uniform vec4 uKeypadLight;
out float vKeypadLight;
float keypadJoint(float joint) {
    return (abs(joint-uKeypadLight.x)<0.25 || abs(joint-uKeypadLight.y)<0.25) ? 1.0 : 0.0;
}
)GLSL");
    vertex.insert(vertex.find(weights)+weights.size(),R"GLSL(
    vKeypadLight=uKeypadLight.z*clamp(dot(w.xyz,vec3(keypadJoint(j.x),keypadJoint(j.y),keypadJoint(j.z))),0.0,1.0);
)GLSL");
    fragment.insert(fm,"in float vKeypadLight;\n");
    fragment.insert(fragment.find(finish),R"GLSL(
    if(vKeypadLight>0.0) {
        // Retain the original textured lettering and alpha. Emission remains
        // visible in dark rooms, respects fog and never darkens a lit key.
        vec3 keyEmission=texture(sTex0,vec3(vTexCoord.xy.xy,vLayer)).rgb*vec3(0.35,1.0,0.55)*uColor.rgb;
        d.rgb=mix(d.rgb,max(d.rgb,keyEmission),clamp(vKeypadLight*vFog,0.0,1.0));
    }
)GLSL");
    return true;
}
}
