#version 150 
uniform sampler3D sNoise;
	uniform sampler2DArray sTex0;
	uniform sampler2DArray sTex1;
	uniform sampler2DArray sTex2;
	uniform sampler2DArray sTex3;
	uniform sampler2DArrayShadow sShadow;
	uniform sampler2DArray sEnvMap;
	uniform sampler2DArray sTex0_wrap;
	uniform sampler2DArray sTex0_mirror;
	uniform mat4 uProjMatrix;
	uniform vec4 uViewMatrix[4];
	uniform mat4 uShadowMatrix[6];
	uniform vec4 uShadowFalloff[6];
	uniform vec4 uShadowPos[6];
	uniform vec4 uFogColor;
    uniform vec4 uFogPlane;
    uniform vec4 uDofPlane;
    uniform vec4 uEffectOffsets[3];
    uniform vec4 uModelMatrix[4];
    uniform vec4 uParams;
    uniform vec4 uColor;
    uniform vec4 uLightPos[8];
    uniform vec4 uLightCol[8];
    uniform vec4 uLightDir[8];
    uniform vec4 uAmbient[6];
    uniform vec4 uJoints[72 * 3];
vec2 oct3to2(vec3 p)
{
	p /= abs(p.x) + abs(p.y) + abs(p.z);
    if (p.y < 0.0)
		p.xz = (1.0 - abs(p.zx)) * sign(p.xz);
	return p.xz * 0.5 + 0.5;
}
vec3 fresnel(vec3 diffuse, vec3 viewVec, vec3 normal, float metalness)
{
    const float F0 = 0.04;
    float NdotV = dot(normal, viewVec);
    float Fd = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    vec3 Fm = mix(vec3(F0, F0, F0), diffuse, metalness);
    return mix(vec3(Fd, Fd, Fd), Fm, metalness);
}
    in vec3 vPos;
    in vec4 vFog;
out vec4 fragColor; void main() {
	vec4 color = vFog;
	color.xyz *= color.w;
    fragColor = color;
}
