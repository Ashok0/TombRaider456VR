#pragma once
#include <string>

namespace tr::motionhandskin {
// Native MaskJoints zeroes the hidden forearm matrix. Blending against it
// shrinks wrist vertices toward the render origin (p.w remains 1). For the
// scoped tracked-hand draw, use the existing corrected wrist palette rigidly
// and trim the hidden side of the seam in the fragment shader instead.
inline bool Patch(std::string& vertex,std::string& fragment) {
    const char* anchor="vec4 w = aColor;";
    const auto at=vertex.find(anchor),main=vertex.find("void main()"),fm=fragment.find("void main()");
    if (at==std::string::npos || main==std::string::npos || fm==std::string::npos ||
        vertex.find(anchor,at+1)!=std::string::npos ||
        vertex.find("vec4 j = aLight;")==std::string::npos ||
        vertex.find("p.z += dot(uJoints[index[2] + 2], coord) * weight;")==std::string::npos ||
        vertex.find("uTrackedHandJoint")!=std::string::npos ||
        fragment.find("vTrackedHandWeight")!=std::string::npos) return false;
    const auto brace=fragment.find('{',fm);
    if (brace==std::string::npos) return false;
    vertex.insert(at+std::char_traits<char>::length(anchor),
        "\n    vTrackedHandWeight = 1.0;\n"
        "    if (uTrackedHandJoint > 0) {\n"
        "        int handJoint = uTrackedHandJoint - 1;\n"
        "        ivec3 handIndices = ivec3(j.xyz);\n"
        "        vTrackedHandWeight = 0.0;\n"
        "        for (int k=0;k<3;++k) if (handIndices[k] == handJoint) vTrackedHandWeight += w[k];\n"
        "        j.xyz = vec3(float(handJoint));\n"
        "        w.xyz = vec3(1.0,0.0,0.0);\n"
        "    }\n");
    vertex.insert(main,"uniform int uTrackedHandJoint;\nout float vTrackedHandWeight;\n");
    fragment.insert(brace+1,"\n    if (vTrackedHandWeight < 0.5) discard;\n");
    fragment.insert(fm,"in float vTrackedHandWeight;\n");
    return true;
}
} // namespace tr::motionhandskin
