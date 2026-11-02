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
    out vec4 vTexCoord;
    out vec2 vTexCoordSprite;
    out vec4 vColor;
    in vec3 aCoord; 
    in vec4 aColor;   
    in vec4 aTexCoord;   
void main() {
	gl_Position = vec4(aCoord, 1.0);
	vec2 d = aTexCoord.xy * 2 - 1;
	float scale = aTexCoord.z * 32;
	float off = aTexCoord.w;
	gl_Position = vec4(dot(uViewMatrix[0], (gl_Position)), dot(uViewMatrix[1], (gl_Position)), dot(uViewMatrix[2], (gl_Position)), dot(uViewMatrix[3], (gl_Position)));
	gl_Position.xy += d * scale;
	gl_Position = ((uProjMatrix) * (gl_Position));
	vTexCoord.xy = (gl_Position.xy / gl_Position.w + vec2(+1.0, -1.0)) * vec2(+0.5, -0.5);
	vTexCoord.zw = (d * off) * 2.0;
	vTexCoordSprite = aTexCoord.xy;
	vTexCoord.y = 1.0 - vTexCoord.y;
	vTexCoord.w *= -1.0;
	vTexCoordSprite.y = 1.0 - vTexCoordSprite.y;
	vColor = aColor;
}
