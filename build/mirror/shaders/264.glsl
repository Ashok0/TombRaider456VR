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
    in vec3 vPosition;
    in vec3 vPositionW;
    in vec3 vNormal;
    in vec4 vUVProjectedDepth[5];
out vec4 fragColor; void main() {
	float a = 1;	
	for (int i = 0; i < 5; ++i)
	{
		vec3 dShadow = normalize( uShadowPos[i].xyz - vPosition.xyz);
		float sLighting = clamp(dot( vNormal, dShadow), 0.0, 1.0);
		float sFalloff = clamp(dot( vec4(vPosition, 1.0), uShadowFalloff[i]), 0.0, 1.0);
	    vec3 uv_shadow = vUVProjectedDepth[i].xyz;
		uv_shadow.z = max(uv_shadow.z, 0.0);
		uv_shadow /= vUVProjectedDepth[i].w;
		float sShadowScale = 1.0 - texture(sShadow, vec4(uv_shadow.x, uv_shadow.y, i, uv_shadow.z));
		sShadowScale = sShadowScale * sLighting;
		sShadowScale = sShadowScale - sShadowScale * sFalloff;
		sShadowScale = sShadowScale - sShadowScale * GetFog(vPositionW).a;
		a *= 1 - sShadowScale;
	}
    fragColor = vec4(0.0, 0.0, 0.0, 1 - a);
}
