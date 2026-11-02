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
    in vec3 vTexCoord;
    in float vLayer;
    in float vFog;
    in vec3 vNormal;
    in vec3 vPos;
out vec4 fragColor; void main() {
    vec4 d;
        d = texture(sTex0_wrap, vec3(vTexCoord.xy.xy, vLayer));
    vec3 normal = normalize(vNormal);
    vec3 pos = vPos;
    vec3 l = getAmbient(normal);
    l += uLightCol[0].xyz * GetLight(0, pos, normal);
    for (int i = 1; i < 8; i++)
    {
        l += uLightCol[i].xyz * GetLight(i, pos, normal);
    }
    float NdotV = max(0.0, dot(normal.xyz, normalize(-pos)));
	l.xyz *= NdotV;
    l.xyz = (pow(l.xyz, uFogPlane.xxx) + l.xyz * (1.0 - uFogPlane.x) * 2.0);
    d.xyz *= l * 2.0;
    {
        d.xyz = mix(uFogColor.xyz * d.w, d.xyz, vFog);
    }
    d *= uColor;
    fragColor = d;
}
