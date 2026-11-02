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
vec4 GetTexArrays( const vec2 uv, const uint layer)
{
	return texture(sTex0, vec3(uv.xy, layer));
}
bool CheckCustomTex( const uint layer)
{
    return layer != 0u;
}
vec4 GetFog( const vec3 vPosition)
{
    vec4 pos = vec4(vec3(1.0, 1.0, 1.0) * length(vPosition.xyz) / 3.0f, 1.0);
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
    in vec4 vTexCoord;
    in vec3 vColor;
    in vec2 vCamera;
    flat in uint vTex0Id;
    flat in uint vTex1Id;
    flat in uint vTex2Id;
out vec4 fragColor; void main() {
	vec4 cDiffuse = vec4(1.0, 1.0, 1.0, 1.0);
	if (CheckCustomTex(vTex0Id))
	{
		cDiffuse = GetTexArrays(vTexCoord.xy, vTex0Id);
		cDiffuse.rgb = pow(cDiffuse.rgb, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));
	}
	vec3 cLighting = vColor;
	vec3 cSurface = vec3(0.0, 0.0, 0.0);
	vec4 normalVec = vec4(0.0, 0.0, 1.0, 0.0);
	if (CheckCustomTex(vTex2Id))
	{
		normalVec = GetTexArrays(vTexCoord.xy, vTex2Id);
		normalVec.xyz = pow(normalVec.rgb, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));
		normalVec.xyz = normalize(2.0 * normalVec.xyz - 1.0);
	}
	vec2 normalVecDot2 = normalVec.xy;
	{
		vec3 lightCol = vec3(1.0, 1.0, 1.0);
		cLighting = dot( normalVecDot2, vCamera) * lightCol + cLighting;
	}
	{
		cSurface = uParams.y * normalVec.w * cDiffuse.rgb;
	}
	if (CheckCustomTex(vTex1Id))
	{
		vec3 cLightmap = GetTexArrays(vTexCoord.zw, vTex1Id).rgb;
		cLightmap = pow(cLightmap, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));
		cLighting = cLighting * cLightmap;
	}
	cSurface = cDiffuse.rgb * cLighting + cSurface;
	vec4 cFog = GetFog( vPosition);
	fragColor = mix( vec4(cSurface.rgb, cDiffuse.a), vec4(uParams.x * cFog.rgb, cDiffuse.a), cFog.a);
}
