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
uint getTex1Layer(const uint layer)
{
	return layer - uint(367);
}
uint getTex2Layer(const uint layer)
{
	return layer - uint(367) - uint(86);
}
uint getTex3Layer(const uint layer)
{
	return layer - uint(367) - uint(86) - uint(7);
}
float calcLodManually( const vec2 uv, float texDim)
{
	vec2 dx_vtc = dFdx(uv * texDim);
	vec2 dy_vtc = dFdy(uv * texDim);
	float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
	return min( max( 0.0f, 0.5f * log2(delta_max_sqr)), 6.0 - 1.0);
}
float getTex0Lod( const vec2 uv)
{
	return calcLodManually(uv, textureSize(sTex0, 0).x);
}
float getTex1Lod( const vec2 uv)
{
	return calcLodManually(uv, textureSize(sTex1, 0).x);
}
float getTex2Lod( const vec2 uv)
{
	return calcLodManually(uv, textureSize(sTex2, 0).x);
}
float getTex3Lod( const vec2 uv)
{
	return calcLodManually(uv, textureSize(sTex3, 0).x);
}
vec4 GetTexArraysMinMaxLod( const vec2 uv, const uint tex_id, const float min_lod, const float max_lod)
{
    if (tex_id < uint(367))
	{
		float lod = clamp(getTex0Lod( uv), min_lod, max_lod);
        return textureLod(sTex0, vec3(uv.xy, tex_id), lod);
	}
    else if (tex_id < (uint(367) + uint(86)))
	{
		float lod = clamp(getTex1Lod( uv), min_lod, max_lod);
        return textureLod(sTex1, vec3(uv.xy, getTex1Layer(tex_id)), lod);
	}
    else if (tex_id < (uint(367) + uint(86) + uint(7)))
	{
		float lod = clamp(getTex2Lod( uv), min_lod, max_lod);
        return textureLod(sTex2, vec3(uv.xy, getTex2Layer(tex_id)), lod);
	}
    else
	{
		float lod = clamp(getTex3Lod( uv), min_lod, max_lod);
        return textureLod(sTex3, vec3(uv.xy, getTex3Layer(tex_id)), lod);
	}
}
vec4 GetTexArrays( const vec2 uv, const uvec2 tex_id)
{
    return GetTexArraysMinMaxLod(uv, tex_id.x, float(tex_id.y), 6.0 - 1.0);
}
bool CheckCustomTex( const uvec2 tex_id)
{
    return tex_id.y != 6u;
}
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
    in vec4 vTexCoord;
    in vec3 vColor;
    in vec3 vCamera;
    flat in uvec2 vTex0Id;
    flat in uvec2 vTex1Id;
    flat in uvec2 vTex2Id;
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
		cDiffuse.rgb *= cDiffuse.a; 
	}
	vec3 cLighting = vColor;
	vec3 cSurface = vec3(0.0, 0.0, 0.0);
	vec4 normalVec = vec4(0.0, 0.0, 1.0, 0.0);
	if (CheckCustomTex(vTex2Id))
	{
		normalVec = GetTexArrays(vTexCoord.xy, vTex2Id);
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
		vec3 snowNormalVec = GetTexArrays(vTexCoord.xy, vTex1Id).rgb;
		snowNormalVec = 2.0 * snowNormalVec - 1.0;
		reflectionCoef = dot( snowNormalVec, vCamera) > 0.9 ? reflectionCoef : 0.0;
		cSurface = reflectionCoef * min(normalVec.w + uParams.w, 1.0) * cReflection.rgb + cSurface;
	}
	cSurface = cDiffuse.rgb * cLighting + cSurface;
	vec4 cFog = GetFog( vPosition);
	fragColor = mix( vec4(cSurface.rgb, cDiffuse.a), vec4(uParams.x * cFog.rgb, cDiffuse.a), cFog.a);
	if (fragColor.a <= 0.5) discard;
}
