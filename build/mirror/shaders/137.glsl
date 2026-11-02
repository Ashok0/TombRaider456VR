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
    in vec3 vNormal;
    in vec4 vTexCoord;
    in vec3 vColor;
    in vec3 vCamera;
    in vec3 vAxisU;
    in vec3 vAxisV;
    flat in uvec2 vTex0Id[2];
    flat in uvec2 vTex1Id;
out vec4 fragColor; void main() {
	float colorCoef = 1;
	bool isFirstLayer = true;
	vec3 Position = vPosition;
	vec3 PositionW = vec4(dot(uViewMatrix[0], vec4(Position, 1.0)), dot(uViewMatrix[1], vec4(Position, 1.0)), dot(uViewMatrix[2], vec4(Position, 1.0)), (vec4(Position, 1.0)).w).xyz;
	vec4 TexCoord = vTexCoord;
	vec4 Coord = ((uProjMatrix) * (vec4(PositionW, 1.0)));
	Coord /= Coord.w;
	vec3 dPosition = vec3(0, 0, 0);
	{
		vec3 rayToCamera = vCamera - Position;
		float cosNormalCamera = dot(rayToCamera, vNormal);
		float furCoef = 30 / length(vec3(dot(uViewMatrix[0].xyz, vNormal), dot(uViewMatrix[1].xyz, vNormal), dot(uViewMatrix[2].xyz, vNormal)));
		vec3 dPosition = abs(cosNormalCamera) > 0.001 ? furCoef / cosNormalCamera * rayToCamera : vec3(0, 0, 0);
	}
	vec4 dTexCoorddx = dFdx(TexCoord);
	vec4 dTexCoorddy = dFdy(TexCoord);
	vec4 cDiffuse = vec4(1, 1, 1, 1);
	for (int iLayer = 9 - 1; iLayer > 0; --iLayer) 
	{
		float fDelta = iLayer / float(9 - 1);
		colorCoef = 0.5f + 0.5f * fDelta;
		Position = vPosition + dPosition * fDelta;
		vec4 CoordLayerBase = vec4(Position - vNormal * 30 * fDelta, 1.0);
		CoordLayerBase = vec4(vec4(dot(uViewMatrix[0], CoordLayerBase), dot(uViewMatrix[1], CoordLayerBase), dot(uViewMatrix[2], CoordLayerBase), (CoordLayerBase).w).xyz, 1.0);
		CoordLayerBase = ((uProjMatrix) * (CoordLayerBase));
		CoordLayerBase /= CoordLayerBase.w;
		vec2 dxy = 0.5 * (CoordLayerBase.xy - Coord.xy) * uParams.zw;
		TexCoord = (vTexCoord + dTexCoorddx * dxy.x + dTexCoorddy * dxy.y);
		cDiffuse = GetTexArrays(TexCoord.xy, vTex0Id[1]);
		if (cDiffuse.a >= fDelta) 
		{
			isFirstLayer = false;
			break;
		}
	}
	float fadeCoef = 1.0f;
	if (!isFirstLayer) 
	{
		Coord = ((uProjMatrix) * (vec4(vec4(dot(uViewMatrix[0], vec4(Position, 1.0)), dot(uViewMatrix[1], vec4(Position, 1.0)), dot(uViewMatrix[2], vec4(Position, 1.0)), (vec4(Position, 1.0)).w).xyz, 1.0)));
		Coord /= Coord.w;
		fadeCoef = clamp((Coord.z - 0.96) / (0.99 - 0.96), 0.0, 1.0);
	}
	vec4 cDiffuseBase = GetTexArrays(vTexCoord.xy, vTex0Id[0]);
	colorCoef = mix(colorCoef, 1.0f, fadeCoef);
	Position = mix(Position, vPosition, fadeCoef);
	TexCoord = mix(TexCoord, vTexCoord, fadeCoef);
	cDiffuse = mix(cDiffuse, cDiffuseBase, fadeCoef);
	vec3 cLighting = vColor * colorCoef;
	vec3 cSurface = vec3(0.0, 0.0, 0.0);
	{
		vec2 normalVec = vec2(0.0, 0.0);
		if (!isFirstLayer)
		{
			normalVec = texture(sTex3, vec3(TexCoord.xy.xy, 0)).xy;
			normalVec = 2.0 * normalVec - 1.0;
			normalVec *= 1 - fadeCoef;
		}
		vec3 rayToCamera = normalize(vCamera - Position);
		vec2 Camera = normalize(vec3(dot(vAxisU, rayToCamera), dot(vAxisV, rayToCamera), dot(vNormal, rayToCamera))).xy;
		cLighting += dot( normalVec, Camera.xy) * cLighting;
	}
	if (CheckCustomTex(vTex1Id))
	{
		vec3 cLightmap = GetTexArrays(TexCoord.zw, vTex1Id).rgb;
		cLighting *= cLightmap;
	}
	cSurface += cDiffuse.rgb * cLighting;
	PositionW = vec4(dot(uViewMatrix[0], vec4(Position, 1.0)), dot(uViewMatrix[1], vec4(Position, 1.0)), dot(uViewMatrix[2], vec4(Position, 1.0)), (vec4(Position, 1.0)).w).xyz;
	vec4 cFog = GetFog( PositionW);
	vec4 color = isFirstLayer
		? mix( vec4(cSurface.rgb, 1.0), vec4(cFog.rgb, 1.0), cFog.a)
		: mix( vec4(cSurface.rgb, cDiffuse.a), vec4(cFog.rgb, cDiffuse.a), cFog.a);
	float aShadow = 1;	
	for (int i = 0; i < int(uParams.y); ++i)
	{
		vec3 dShadow = normalize( uShadowPos[i].xyz - Position.xyz);
		float sLighting = clamp(dot( vNormal, dShadow), 0.0, 1.0);
		float sFalloff = clamp(dot( vec4(Position, 1.0), uShadowFalloff[i]), 0.0, 1.0);
		vec4 UVProjectedDepth = ((uShadowMatrix[i]) * (vec4(Position, 1.0)));
	    vec3 uv_shadow = UVProjectedDepth.xyz;
		uv_shadow.z = max(uv_shadow.z, 0.0);
		uv_shadow /= UVProjectedDepth.w;
		float sShadowScale = 1.0 - texture(sShadow, vec4(uv_shadow.x, uv_shadow.y, i, uv_shadow.z));
		sShadowScale = sShadowScale * sLighting;
		sShadowScale = sShadowScale - sShadowScale * sFalloff;
		sShadowScale = sShadowScale - sShadowScale * GetFog(PositionW).a;
		aShadow *= 1 - sShadowScale;
	}
	fragColor = color * aShadow + vec4(0, 0, 0, 1 - aShadow);
}
