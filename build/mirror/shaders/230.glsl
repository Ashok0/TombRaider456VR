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
    in vec3 vAxisU;
    in vec3 vAxisV;
    in vec3 vNormal;
    in vec3 vLight;
    in vec3 vCamera;
vec4 AmbientHalf(vec3 normal)
{
    const float wrapVal = 0.5;
    vec4 color = uAmbient[0];
    color += uLightCol[1] * pow(clamp(dot( uLightDir[1].xyz, normal), 0.0, 1.0) * wrapVal + (1.0 - wrapVal), 2.0);
    color += uLightCol[2] * pow(clamp(dot( uLightDir[2].xyz, normal), 0.0, 1.0) * wrapVal + (1.0 - wrapVal), 2.0);
    color += uLightCol[3] * pow(clamp(dot( uLightDir[3].xyz, normal), 0.0, 1.0) * wrapVal + (1.0 - wrapVal), 2.0);
    color += uLightCol[4] * pow(clamp(dot( uLightDir[4].xyz, normal), 0.0, 1.0) * wrapVal + (1.0 - wrapVal), 2.0);
    color += uLightCol[5] * pow(clamp(dot( uLightDir[5].xyz, normal), 0.0, 1.0) * wrapVal + (1.0 - wrapVal), 2.0);
    return color;
}
vec4 GetTextureNormal( const vec2 uvBase, const vec3 vAxisUBase, const vec3 vAxisVBase, const vec3 normalSurface)
{
    vec4 cNormal = texture(sTex2, vec3(uvBase.xy, uParams.z));
    cNormal.xyz = normalize( 2.0 * (cNormal.xyz - 0.5));
	vec4 vNorm;
    vNorm.x = dot( cNormal.xyz, vec3(vAxisUBase.x, vAxisVBase.x, normalSurface.x));
	vNorm.y = dot( cNormal.xyz, vec3(vAxisUBase.y, vAxisVBase.y, normalSurface.y));
	vNorm.z = dot( cNormal.xyz, vec3(vAxisUBase.z, vAxisVBase.z, normalSurface.z));
    vNorm.w = cNormal.w;
	return( vNorm);
}
float CalcLommelSeeliger( vec3 dCamera, vec3 dNormal, vec3 dLight)
{
	float sNL = clamp(float( dot( dNormal, dLight)), 0.0, 1.0);
	float sNC = clamp(float( dot( dNormal, dCamera)), 0.0, 1.0);
	float sNCpNL = sNC + sNL;
	float oosNCpNLParam = 1.5;
	float oosNCpNL = clamp(oosNCpNLParam / sNCpNL, 0.0, 1.0);
	float sShade = sNL * oosNCpNL;
	return( sShade);
}
float CalcPhong(vec3 dNormal, vec3 dLight, vec3 dReflectionCamera, float sPower)
{
	float sLR = clamp(dot( dLight.xyz, dReflectionCamera.xyz), 0.0, 1.0);
	float sShade = clamp(pow( sLR, sPower), 0.0, 1.0);
	float sLN = dot( dLight.xyz, dNormal.xyz);
	sShade = (sLN > 0.0) ? sShade : 0;
	return( sShade);
}
float CalcFresnel( vec3 dCamera, vec3 dNormal)
{
	float sNC = dot( dNormal, dCamera);
	float InvsNC = 1 - clamp(sNC, 0.0, 1.0);
	float sShade = pow( InvsNC, 2);
	return( clamp(sShade, 0.0, 1.0));
}
out vec4 fragColor; void main() {
	vec4 cDiffuse = texture(sTex0_wrap, vec3(vTexCoord.xy, uParams.x));
	cDiffuse.rgb *= cDiffuse.a; 
	vec4 normalVec = GetTextureNormal( vTexCoord, vAxisU, vAxisV, vNormal);
	vec3 cColour = cDiffuse.rgb;
	vec3 cLight = uLightCol[0].rgb;
	{
		cColour *= vColor;
	}
	vec3 lightVec = normalize( vLight);
	vec3 viewVec = normalize( vCamera);
	vec3 reflectVec = GetReflectionVec( normalVec.xyz, viewVec);
	float sLightingDiffuse = CalcLommelSeeliger( viewVec, normalVec.xyz, lightVec);
	float sLightingSpecular = CalcPhong( normalVec.xyz, lightVec, reflectVec, 32.0);
	vec3 cSpecular = texture(sTex3, vec3(vTexCoord.xy, uParams.w)).rgb;
	cColour += cLight * cDiffuse.rgb * sLightingDiffuse;
	cColour += cLight * normalVec.w * sLightingSpecular * cSpecular;
	vec4 cFog = GetFog( vPosition);
	if (uParams.y > 0.0)
		fragColor = mix( vec4(cColour, cDiffuse.a), vec4(0.0, 0.0, 0.0, cDiffuse.a), cFog.a);
	else
		fragColor = mix( vec4(cColour, cDiffuse.a), vec4(cFog.rgb * cDiffuse.a, cDiffuse.a), cFog.a);
}
