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
    in vec2 vTexCoord;
    in vec3 vrgb0;
    in vec3 vrgb1;
    in vec3 vrgb2;
    in vec3 vrgb3;
    in vec3 vrgb4;
    in vec3 vrgb5;
out vec4 fragColor; void main() {
    vec2 uv = vTexCoord.xy;
    vec2 d = vec2((1.0 / 128.0), 0.0);
    float t = uModelMatrix[3].x * 0.1;
    float hC = texture(sNoise, vec3(uv, t)).x;
    float hX = texture(sNoise, vec3(uv + d.xy, t)).x;
    float hY = texture(sNoise, vec3(uv + d.yx, t)).x;
    hX -= hC;
    hY -= hC;
    vec3 n = cross(vec3(0.0, hY * (1.0 / 16.0), (1.0 / 128.0)), vec3((1.0 / 128.0), hX * (1.0 / 16.0), 0.0));
    n = -normalize(n);
    if (!gl_FrontFacing)
    {
        n = -n;
    }
    vec3 pos = vPos;
    vec3 vv = normalize(-pos);
    float NdotV = max(0.0, dot(n, -vv));
    vec3 r = normalize(reflect(-vv, n));
    vec3 sqr = r * r;
    vec3 p = step(vec3(0.0, 0.0, 0.0), r);
    vec3 ref =
           sqr.x * mix(vrgb1, vrgb0, p.x) +
           sqr.y * mix(vrgb3, vrgb2, p.y) +
           sqr.z * mix(vrgb5, vrgb4, p.z);
    float F0 = 0.04;
    float Fd = F0 + (1.0 - F0) * pow(1.0 - NdotV, 3.0);
    vec4 color = vec4(ref * Fd * 2.0, 0.0);
    fragColor = color;
}
