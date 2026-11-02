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
    in vec2 vTexCoord;
    flat in uvec2 vTex0Id;
out vec4 fragColor; void main() {
	float alpha = GetTexArrays(vTexCoord, vTex0Id).a;
    if (alpha <= 0.5f) discard;
}
