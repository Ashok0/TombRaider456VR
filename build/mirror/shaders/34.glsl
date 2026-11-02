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
float GetShade(vec3 N, vec3 L)
{
    float NdotL = dot(N, L);
    return max(0.0, NdotL);
}
float GetLight(int index, vec3 pos, vec3 normal)
{
    vec3 lv = uLightPos[index].xyz - pos;
    float att = ((1024.0 / 6.0) * (1024.0 / 6.0)) / dot(lv, lv);
    lv = normalize(lv);
    float NdotL = GetShade(normal, lv);
    return att * NdotL;
}
vec3 getAmbient(vec3 n)
{
    vec3 sqr = n * n;
    vec3 pos = step(vec3(0.0, 0.0, 0.0), n);
    return sqr.x * mix(uAmbient[1].xyz, uAmbient[0].xyz, pos.x) +
           sqr.y * mix(uAmbient[3].xyz, uAmbient[2].xyz, pos.y) +
           sqr.z * mix(uAmbient[5].xyz, uAmbient[4].xyz, pos.z);
}
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
    in vec3 vTexCoord;
    in float vLayer;
    in float vFog;
    in vec3 vNormal;
    in vec3 vPos;
out vec4 fragColor; void main() {
    vec4 d;
        d = texture(sTex0, vec3(vTexCoord.xy.xy, vLayer));
    vec3 normal = normalize(vNormal);
    vec3 pos = vPos;
    vec3 l = getAmbient(normal);
    l += uLightCol[0].xyz * GetLight(0, pos, normal);
    for (int i = 1; i < 8; i++)
    {
        l += uLightCol[i].xyz * GetLight(i, pos, normal);
    }
    l.xyz = (pow(l.xyz, uFogPlane.xxx) + l.xyz * (1.0 - uFogPlane.x) * 2.0);
    d.xyz *= l * 2.0;
    vec3 viewVec = normalize(-pos);
    vec3 viewRef = normalize(reflect(viewVec, normal));
    vec2 oct_uv = oct3to2(viewRef);
    vec4 oct = texture(sTex0_mirror, vec3(oct_uv.xy, uParams.x));
        oct.xyz *= getAmbient(viewRef);
            d.xyz = mix(d.xyz, oct.xyz * mix(dot(d.xyz, vec3(0.2126, 0.7152, 0.0722)) * uParams.y, 1.0, uParams.z) * 2.0, d.w);
        d.w = 1.0;
    {
        d.xyz = mix(uFogColor.xyz * d.w, d.xyz, vFog);
    }
    d *= uColor;
    fragColor = d;
}
