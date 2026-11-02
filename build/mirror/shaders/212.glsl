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
vec3 getWorldCameraPos()
{
	vec3 pos = vec3(uViewMatrix[0].w, uViewMatrix[1].w, uViewMatrix[2].w);
	vec3 i_inv = vec3(uViewMatrix[0].x, uViewMatrix[1].x, uViewMatrix[2].x);
	vec3 j_inv = vec3(uViewMatrix[0].y, uViewMatrix[1].y, uViewMatrix[2].y);
	vec3 k_inv = vec3(uViewMatrix[0].z, uViewMatrix[1].z, uViewMatrix[2].z);
	return vec3(-dot(i_inv, pos), -dot(j_inv, pos), -dot(k_inv, pos));
}
    in vec3 aCoord; 
    in vec3 aCoord1;   
    in vec3 aTexCoord;   
    in vec4 aNormal;   
    in vec4 aNormal1;   
    in vec4 aAxisU;   
    in vec4 aAxisU1;   
    in vec4 aAxisV;   
    in vec4 aAxisV1;   
    in vec2 aMatIdx;   
void main() {
	uint imat0 = uint(aMatIdx.x);
	uint imat1 = uint(aMatIdx.y);
	float blend = aTexCoord.z;
	vec4 pos0 = vec4(dot(uJoints[imat0 * 3u + 0u], vec4(aCoord, 1.0f)), dot(uJoints[imat0 * 3u + 1u], vec4(aCoord, 1.0f)), dot(uJoints[imat0 * 3u + 2u], vec4(aCoord, 1.0f)), (vec4(aCoord, 1.0f)).w);
	vec4 pos1 = vec4(dot(uJoints[imat1 * 3u + 0u], vec4(aCoord1, 1.0f)), dot(uJoints[imat1 * 3u + 1u], vec4(aCoord1, 1.0f)), dot(uJoints[imat1 * 3u + 2u], vec4(aCoord1, 1.0f)), (vec4(aCoord1, 1.0f)).w);
	vec4 pos = mix(pos1, pos0, blend);
    gl_Position = ((uProjMatrix) * (vec4(dot(uViewMatrix[0], pos), dot(uViewMatrix[1], pos), dot(uViewMatrix[2], pos), (pos).w)));
}
