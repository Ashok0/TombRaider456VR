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
    in vec2 vTexCoord;
    in vec3 vColor;
    in vec3 vNormal;
    in vec3 vLight;
    in vec3 vCamera;
vec4 Ambient(vec3 normal)
{
    vec4 color = uAmbient[0];
    color += uLightCol[1] * clamp(dot( uLightDir[1].xyz, normal), 0.0, 1.0);
    color += uLightCol[2] * clamp(dot( uLightDir[2].xyz, normal), 0.0, 1.0);
    color += uLightCol[3] * clamp(dot( uLightDir[3].xyz, normal), 0.0, 1.0);
    color += uLightCol[4] * clamp(dot( uLightDir[4].xyz, normal), 0.0, 1.0);
    color += uLightCol[5] * clamp(dot( uLightDir[5].xyz, normal), 0.0, 1.0);
    return color;
}
vec4 GetTextureNormal( const vec2 uvBase, const vec3 vAxisUBase, const vec3 vAxisVBase, const vec3 normalSurface)
{
	vec4 cNormal = texture(sTex2, vec3(uvBase.xy, uParams.z));
    {
		cNormal.xyz = 2.0 * (cNormal.xyz - 0.5);
    }
	vec4 vNorm;
    vNorm = vec4(
		dot( cNormal.xyz, vec3(vAxisUBase.x, vAxisVBase.x, normalSurface.x)),
		dot( cNormal.xyz, vec3(vAxisUBase.y, vAxisVBase.y, normalSurface.y)),
		dot( cNormal.xyz, vec3(vAxisUBase.z, vAxisVBase.z, normalSurface.z)),
		cNormal.w);
	return( vNorm);
}
out vec4 fragColor; void main() {
	vec4 cDiffuse = texture(sTex0_wrap, vec3(vTexCoord.xy, uParams.x));
	{
		cDiffuse.rgb *= cDiffuse.a; 
	}
	vec4 normalVec = vec4( normalize( vNormal), texture(sTex2, vec3(vTexCoord.xy, uParams.z)).w);
	vec3 cColour = cDiffuse.rgb;
	vec3 cLight = uLightCol[0].rgb;
	vec3 lightVec = normalize( vLight);
	{
		cColour *= vColor;
	}
	{
		vec3 halfVec = normalize( 0.5 * (vCamera + vLight));
		vec2 uvDiffSpec;
		uvDiffSpec.x = clamp(dot( lightVec, normalVec.xyz), 0.0, 1.0);
		uvDiffSpec.y = clamp(dot( halfVec, normalVec.xyz), 0.0, 1.0);
		vec4 cDiffSpec = texture(sTex1, vec3(uvDiffSpec.xy, 0));
		float sDiffuse = cDiffSpec.a;
		vec3 cSpecular = cDiffSpec.rgb;
		cSpecular *= texture(sTex3, vec3(vTexCoord.xy, uParams.w)).rgb;
		float sSpecular = normalVec.w;
		cColour += cLight * sSpecular * cSpecular;
		cColour += cLight * sDiffuse * cDiffuse.rgb;
	}
	vec4 cFog = GetFog( vPosition);
	if (uParams.y > 0.0)
		fragColor = mix( vec4(cColour, cDiffuse.a), vec4(0.0, 0.0, 0.0, cDiffuse.a), cFog.a);
	else
		fragColor = mix( vec4(cColour, cDiffuse.a), vec4(cFog.rgb * cDiffuse.a, cDiffuse.a), cFog.a);
	if (fragColor.a <= 0.5) discard;
}
