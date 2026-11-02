#version 150 
uniform sampler3D sNoise;
	in vec3 aCoord;
	in vec4 aExtra;
	in vec4 aNormal;
	in vec4 aTexCoord;
	in vec4 aLight;
	in vec4 aColor;
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
    out vec2 vTexCoord;
void main() {
	gl_Position = vec4(aCoord, 1.0);
	vTexCoord = vec2(aTexCoord.xy);
	vTexCoord.y = 1.0f - vTexCoord.y;
}
