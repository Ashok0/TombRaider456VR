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
    in vec4 vColor;
out vec4 fragColor; void main() {
	vec4 Intensity = vec4(0.0, 0.0, 0.0, 0.0);
	vec2 uv = vTexCoord.xy;
	float x0 = uv.x;
	float x1 = (uv.x - 0.5) * uEffectOffsets[0].x + 0.5;
  	float dx = (x1 - x0) / 31.0;
	vec4 result = vec4(0.0, 0.0, 0.0, 0.0);
	for (float i = 0.0; i < 31.5; i += 1.0)
	{
		vec4 tex = vec4(texture(sTex0, vec3(uv + vec2(dx * i, 0.0).xy, 0)).rgb, 1.0);
		result = result * (1.0 - vColor.a * 0.6) + 2.0 * tex * pow(vColor * 2.0, vec4(i, i, i, i) / 6.0);
	}
	Intensity = vec4(result.rgb, 1.0);
	fragColor = Intensity;
}
