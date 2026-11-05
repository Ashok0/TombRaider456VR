#pragma once
namespace tr::surfacecap {
inline constexpr const char* Vertex=R"GLSL(#version 150
in vec3 aPosition;
uniform mat4 uProjection;
uniform vec4 uView[3];
void main() {
    gl_Position=uProjection*vec4(dot(uView[0].xyz,aPosition),
        dot(uView[1].xyz,aPosition),dot(uView[2].xyz,aPosition),1.0);
})GLSL";
inline constexpr const char* Fragment=R"GLSL(#version 150
out vec4 color;
void main() { color=vec4(0.0,0.0,0.0,1.0); }
)GLSL";
}
