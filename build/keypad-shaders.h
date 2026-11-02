// Audited shader 9 used by PUZZLE_HOLE8_1; matches retail verbatim.
const char NativeKeypadVertex[]=R"KEYPAD(#version 150 
uniform sampler3D sNoise;
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
    out vec3 vTexCoord;
    out float vLayer;
    out float vFog;
    out vec3 vNormal;
    out vec3 vPos;
    out vec3 vWorldPos;
    in vec3 aCoord; 
    in vec4 aNormal;   
    in vec4 aLight;   
    in vec4 aColor;   
void main() {
    vec4 coord = vec4(aCoord, 1.0);
    vec4 normal = vec4(aNormal);
    vec4 j = aLight;
    vec4 w = aColor;
    vLayer = normal.w;
    vec4 p;
    float weight;
    float spec = 0.0;
    ivec3 index = ivec3(j.xyz);
    index *= 3;
    weight = w[0];
    p.x = dot(uJoints[index[0] + 0], coord) * weight;
    p.y = dot(uJoints[index[0] + 1], coord) * weight;
    p.z = dot(uJoints[index[0] + 2], coord) * weight;
    p.w = 1.0;
    normal.xyz = normalize(normal.xyz - 127.0);
    vec3 n;
    n.x = dot(uJoints[index[0] + 0].xyz, normal.xyz) * weight;
    n.y = dot(uJoints[index[0] + 1].xyz, normal.xyz) * weight;
    n.z = dot(uJoints[index[0] + 2].xyz, normal.xyz) * weight;
    weight = w[1];
    p.x += dot(uJoints[index[1] + 0], coord) * weight;
    p.y += dot(uJoints[index[1] + 1], coord) * weight;
    p.z += dot(uJoints[index[1] + 2], coord) * weight;
    n.x += dot(uJoints[index[1] + 0].xyz, normal.xyz) * weight;
    n.y += dot(uJoints[index[1] + 1].xyz, normal.xyz) * weight;
    n.z += dot(uJoints[index[1] + 2].xyz, normal.xyz) * weight;
    weight = w[2];
    p.x += dot(uJoints[index[2] + 0], coord) * weight;
    p.y += dot(uJoints[index[2] + 1], coord) * weight;
    p.z += dot(uJoints[index[2] + 2], coord) * weight;
    n.x += dot(uJoints[index[2] + 0].xyz, normal.xyz) * weight;
    n.y += dot(uJoints[index[2] + 1].xyz, normal.xyz) * weight;
    n.z += dot(uJoints[index[2] + 2].xyz, normal.xyz) * weight;
    vNormal = n.xyz;
    vFog = clamp(exp(-((length(p.xyz) / ((30.0 * 1000.0) / 2))*(length(p.xyz) / ((30.0 * 1000.0) / 2)))), 0.0, 1.0);
    vPos = p.xyz;
    vWorldPos = p.xyz + vec3(uViewMatrix[0].w, uViewMatrix[1].w, uViewMatrix[2].w);
    vec2 uv = vec2(j.w / 255.0, w.w);
    vTexCoord = vec3(uv, spec);
    gl_Position = ((uProjMatrix) * (vec4(dot(uViewMatrix[0].xyz, (p).xyz), dot(uViewMatrix[1].xyz, (p).xyz), dot(uViewMatrix[2].xyz, (p).xyz), (p).w)));
}
)KEYPAD";
const char NativeKeypadFragment[]=R"KEYPAD(#version 150 
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
vec4 shadeUW(vec4 d, vec3 p, vec3 wp)
{
    float h = wp.y - uModelMatrix[3].y;
    float k = texture(sNoise, wp.xzy / (16.0 * 1024.0) - vec3(0.0, 0.0, uModelMatrix[3].x * 0.1)).x;
    d.xyz *= 0.25 + k * 2.0;
    float dist = length(p);
    if (vec3(uViewMatrix[0].w, uViewMatrix[1].w, uViewMatrix[2].w).y < uModelMatrix[3].y)
    {
        dist *= h / p.y;
    }
    dist += min(h, 2048.0);
    d.xyz *= mix(vec3(1.0, 1.0, 1.0), vec3(0.6, 0.843, 0.901), clamp(sqrt(dist * (1.0 / 2048.0)), 0.0, 2.0));
    d.xyz = mix(vec3(0.0036, 0.0169, 0.0231) * d.w, d.xyz, clamp(exp(-((dist / (12.0 * 1024.0))*(dist / (12.0 * 1024.0)))), 0.0, 1.0));
    return d;
}
const vec2 POISSON_DISC[8] = vec2[8](
    (2.0 / 1024.0) * vec2( 0.000000f, 0.000000f),
    (2.0 / 1024.0) * vec2( 0.527837f,-0.085868f),
    (2.0 / 1024.0) * vec2(-0.040088f, 0.536087f),
    (2.0 / 1024.0) * vec2(-0.670445f,-0.179949f),
    (2.0 / 1024.0) * vec2(-0.419418f,-0.616039f),
    (2.0 / 1024.0) * vec2( 0.440453f,-0.639399f),
    (2.0 / 1024.0) * vec2(-0.757088f, 0.349334f),
    (2.0 / 1024.0) * vec2( 0.574619f, 0.685879f)
);
float getShadow(vec4 p, float index)
{
	float s = 0.0;
    p.z = clamp(p.z, 0.0, 1.0);
	for (int i = 0; i < 8; i++)
	{
		vec2 d = POISSON_DISC[i];
		d.xy += p.xy;
		s += texture(sShadow, vec4(vec3(d.x, d.y, p.z).x, vec3(d.x, d.y, p.z).y, index, vec3(d.x, d.y, p.z).z));
	}
    s /= 8;
	return s;
}
float GetPenumbra(vec3 lightVec)
{
    return smoothstep(0.25, 1.0, max(0.0, -lightVec.y));
}
float GetItemShadowLight(int index, vec3 pos, vec3 normal)
{
    vec4 p = ((uShadowMatrix[0]) * (vec4(pos + normal * 16.0, 1.0)));
    p.xyz /= p.w;
    float shadow = getShadow(p, 0);
    vec3 lv = uLightPos[index].xyz - pos * uLightPos[index].w;
    float att = mix(1.0, ((1024.0 / 6.0) * (1024.0 / 6.0)) / dot(lv, lv), uLightPos[index].w);
    lv = normalize(lv);
    float NdotL = GetShade(normal, lv);
    shadow = mix(1.0, shadow, GetPenumbra(lv));
    return att * NdotL * shadow;
}
float GetRoomShadowLight(int index, vec3 pos)
{
    vec4 p = ((uShadowMatrix[0]) * (vec4(pos, 1.0)));
    p.xyz /= p.w;
    float shadow = getShadow(p, 0);
    vec3 lv = uLightPos[index].xyz - pos * uLightPos[index].w;
    lv = normalize(lv);
    return mix(1.0, shadow, GetPenumbra(lv));
}
vec3 GetSpotLightIndex(int index, vec3 pos)
{
    if (uShadowPos[1][index] <= 0.0)
        return vec3(0.0, 0.0, 0.0);
    vec4 p = ((uShadowMatrix[1 + index]) * (vec4(pos, 1.0)));
    if (p.w <= uShadowPos[2][index])
        return vec3(0.0, 0.0, 0.0);
    p.xyz /= p.w;
    return texture(sTex1, vec3(p.xy.xy, uShadowPos[0][index])).xyz * uShadowPos[1][index];
}
vec3 GetSpotLight(vec3 pos)
{   
    return GetSpotLightIndex(0, pos) + GetSpotLightIndex(1, pos);
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
    in vec3 vWorldPos;
out vec4 fragColor; void main() {
    vec4 d;
        d = texture(sTex0, vec3(vTexCoord.xy.xy, vLayer));
    vec3 normal = normalize(vNormal);
    vec3 pos = vPos;
    vec3 l = getAmbient(normal);
        if (uModelMatrix[3].y != 0.0 && uModelMatrix[3].y < vWorldPos.y)
        {
            l *= vec3(0.6, 0.843, 0.901) * 2.0;
        }
    l += uLightCol[0].xyz * GetItemShadowLight(0, pos, normal);
    for (int i = 1; i < 8; i++)
    {
        l += uLightCol[i].xyz * GetLight(i, pos, normal);
    }
    l += GetSpotLight(pos);
    l.xyz = (pow(l.xyz, uFogPlane.xxx) + l.xyz * (1.0 - uFogPlane.x) * 2.0);
    d.xyz *= l * 2.0;
    if (uModelMatrix[3].y != 0.0 && uModelMatrix[3].y < vWorldPos.y)
    {
        d = shadeUW(d, vPos, vWorldPos);
    }
    else
    {
        d.xyz = mix(uFogColor.xyz * d.w, d.xyz, vFog);
    }
    d *= uColor;
    fragColor = d;
}
)KEYPAD";
