#version 150 
uniform sampler3D sNoise;
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
uvec3 getTexIds(vec4 texIds)
{
	uint extra0 = uint(texIds.x);
	uint extra1 = uint(texIds.y);
	uint extra2 = uint(texIds.z);
	uint extra3 = uint(texIds.w);
	uint dif_id = extra0 | ((extra3 << 8) & 0x100u);
	uint lm_id = extra1 | ((extra3 << 7) & 0x100u);
	uint bump_id = extra2 | ((extra3 << 6) & 0x100u);
	return uvec3(dif_id, lm_id, bump_id);
}
uvec3 getMinLods(vec4 texIds0, vec4 texIds1)
{
	uint extra0 = uint(texIds0.w);
	uint extra1 = uint(texIds1.w);
	uint dif_lod = (extra0 >> 3) & 0x7u;
	uint lm_lod = ((extra0 >> 6) & 0x3u) | ((extra1 >> 4) & 0x4u);
	uint bump_lod = (extra1 >> 3) & 0x7u;
	return uvec3(dif_lod, lm_lod, bump_lod);
}
vec3 getWorldCameraPos()
{
	vec3 pos = vec3(uViewMatrix[0].w, uViewMatrix[1].w, uViewMatrix[2].w);
	vec3 i_inv = vec3(uViewMatrix[0].x, uViewMatrix[1].x, uViewMatrix[2].x);
	vec3 j_inv = vec3(uViewMatrix[0].y, uViewMatrix[1].y, uViewMatrix[2].y);
	vec3 k_inv = vec3(uViewMatrix[0].z, uViewMatrix[1].z, uViewMatrix[2].z);
	return vec3(-dot(i_inv, pos), -dot(j_inv, pos), -dot(k_inv, pos));
}
    in vec3 aCoord; 
    in vec4 aTexCoord;   
    in vec4 aAxisU;   
    in vec4 aAxisV;   
    in vec4 aNormal;   
    in vec4 aTexIds0;   
    in vec4 aTexIds1;   
void main() {
	vec4 pos = vec4(dot(uModelMatrix[0], vec4(aCoord, 1.0)), dot(uModelMatrix[1], vec4(aCoord, 1.0)), dot(uModelMatrix[2], vec4(aCoord, 1.0)), 1.0);
	vec4 Position = vec4(dot(uViewMatrix[0], pos), dot(uViewMatrix[1], pos), dot(uViewMatrix[2], pos), (pos).w);
	vec4 furPos = vec4(uParams.x, 0.0, 0.0, 1.0);
	furPos.xyz += vec3(dot(uViewMatrix[0].xyz, aNormal.xyz * 2.0 - 1.0), dot(uViewMatrix[1].xyz, aNormal.xyz * 2.0 - 1.0), dot(uViewMatrix[2].xyz, aNormal.xyz * 2.0 - 1.0));
	furPos.xyz = normalize( furPos.xyz);
	Position.xyz += uParams.w * furPos.xyz;
	gl_Position = ((uProjMatrix) * (Position));
}
