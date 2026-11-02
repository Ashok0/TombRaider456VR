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
	out vec2 vRatio;
	out float vProgress;
	out float vSmoothness;
void main() {
	vec4 p = vec4(aCoord.xy, 0.0, 1.0);
	gl_Position = ((uProjMatrix) * (p));
	vTexCoord   = aTexCoord.xy * uParams.xx;
	vRatio      = vec2(aTexCoord.w / 4096.0, 1.0);
	vProgress   = aTexCoord.z / 4096.0;
	vSmoothness = aNormal.x / 4096.0;
}
