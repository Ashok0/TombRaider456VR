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
    in vec3 vCamera;
    flat in uint vTex0Id;
    flat in uint vTex1Id;
    flat in uint vTex2Id;
    in vec3 vAxisU;
    in vec3 vAxisV;
    in vec3 vNormal;
	in vec4 vScreenPos;
	in vec3 vCameraTangent;
vec3 GetTextureNormal( const vec4 texNormal, const vec3 vAxisUBase, const vec3 vAxisVBase)
{
	vec3 vNorm;
    vNorm = vec3(
		dot(texNormal.xy, vec2(vAxisUBase.x, vAxisVBase.x)),
		dot(texNormal.xy, vec2(vAxisUBase.y, vAxisVBase.y)),
		dot(texNormal.xy, vec2(vAxisUBase.z, vAxisVBase.z))
	);
	return( vNorm);
}
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
	normalVec.xyz = mix(normalVec.xyz, vec3(0.0, 0.0, 1.0), uParams.z);
	vec3 normalVecDot2 = GetTextureNormal( normalVec, normalize(vAxisU), normalize(vAxisV));
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
	{
		vec3 normalGeomVec = vNormal;
		if (!gl_FrontFacing)
		{
			normalGeomVec.xyz = -normalGeomVec.xyz;
		}
		vec2 texCoordRef = vScreenPos.xy / vScreenPos.w * 0.5 + vec2(0.5, 0.5);
		texCoordRef += ((uProjMatrix) * (vec4(dot(uViewMatrix[0], vec4(normalVecDot2, 0.0)), dot(uViewMatrix[1], vec4(normalVecDot2, 0.0)), dot(uViewMatrix[2], vec4(normalVecDot2, 0.0)), (vec4(normalVecDot2, 0.0)).w))).xy * 0.2;
		vec3 cReflection = texture(sEnvMap, vec3(texCoordRef.xy, 0)).rgb;
		cReflection *= clamp(normalGeomVec.z, 0.0, 1.0);
		float reflectionCoef = 0.5;
		float F0 = 0.04;
    	float NdotV = clamp(dot(normalVec.xyz, normalize(vCameraTangent)), 0.0, 1.0);
    	reflectionCoef = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
		reflectionCoef = sqrt(reflectionCoef); 
		cSurface = reflectionCoef * min(normalVec.w + uParams.w, 1.0) * cReflection.rgb + cSurface;
	}
	cSurface = cDiffuse.rgb * cLighting + cSurface;
	vec4 cFog = GetFog( vPosition);
	fragColor = mix( vec4(cSurface.rgb, cDiffuse.a), vec4(uParams.x * cFog.rgb, cDiffuse.a), cFog.a);
	if (fragColor.a <= 0.5) discard;
}
