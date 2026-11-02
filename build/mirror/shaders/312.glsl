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
vec4 GetFog( const vec3 vPosition)
{
    vec4 pos = vec4(vPosition, 1.0);
    float FogDistance = dot(pos, uFogPlane);
    vec4 cFog = uFogColor;
	cFog.a *= clamp( FogDistance, 0.0, 1.0);
	return( cFog);
}
vec3 GetReflectionVec( const vec3 vNorm, const vec3 vDirection)
{
	return( (2.0 * vNorm * dot( vNorm, vDirection)) - vDirection);
}
    in vec2 vTexCoord;
    in vec3 vNormal;
    in vec3 vCamera;
out vec4 fragColor; void main() {
    vec3 embm = texture(sTex1, vec3((vTexCoord).xy, 0)).xyz;
    vec3 n = normalize(embm * 2.0 - 1.0);
    n = (0.7 * n.xyz) + vNormal;
    n = normalize(n);
    if (!gl_FrontFacing)
    {
        n *= -1;
    }
    vec2 n_scale = 50.0f * n.xy;
	n_scale.y *= -1.0;
    vec2 uv_refract = (gl_FragCoord.xy - n_scale) * uAmbient[2].xy;
    float not_water = 1 - texture(sTex0, vec3(uv_refract.xy, 0)).w;
    uv_refract += n_scale * uAmbient[2].xy * not_water; 
    vec3 vv = normalize(vCamera);
    vec3 lightDir = (-uViewMatrix[2].xyz) * vec3(1.0, 1.0, -1.0);
    vec3 dLightReflect = reflect(-lightDir, n);
    float type_water = uAmbient[5].x;
    float has_refl = uAmbient[5].y;
    float refl_strength = min(1.0, uAmbient[5].z);
    vec3 refr = texture(sTex0, vec3(uv_refract.xy, 0)).xyz * uAmbient[0].xyz;
    vec3 spec = pow( clamp(dot(dLightReflect, vv), 0.0, 1.0), 32.0) * uAmbient[1].xyz * 0.5;
    vec2 uv_reflect = (gl_FragCoord.xy + n_scale) * uAmbient[2].xy;
    vec3 refl = texture(sEnvMap, vec3(uv_reflect.xy, 0)).rgb * uAmbient[0].xyz;
    refl = mix(refr, refl, uAmbient[5].w);
    refl = mix(refl, uAmbient[0].xyz, 0.1);
    refl = mix(uAmbient[0].xyz, refl, has_refl);
    float F0 = 0.04;
    float F = F0 + (1.0 - F0) * pow(1.0 - clamp(dot(n, vv), 0.0, 1.0), 5.0);
    F = sqrt(F); 
    F = mix(0.05 + F * 0.1, F, has_refl) * type_water;
    fragColor.rgba = vec4(mix(refr, refl + spec, F * refl_strength), 1.0); 
}
