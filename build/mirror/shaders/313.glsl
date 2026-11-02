#version 150 
uniform sampler3D sNoise;
	out vec4 vTexCoord;
	out vec4 vColor;
	out vec3 vLight;
	out float vLayer;
	out float vFog;
	out vec3 vNormal;
	out vec3 vPos;
	out vec4 vSProj;
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
    in vec3 aCoord;
void main() {
	vec4 pos = vec4(aCoord, 1.0);
	gl_Position = ((uProjMatrix) * (vec4(dot(uViewMatrix[0], pos), dot(uViewMatrix[1], pos), dot(uViewMatrix[2], pos), (pos).w)));
}
