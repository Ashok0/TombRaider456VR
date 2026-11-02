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
    out vec3 vPos;
    in vec3 aCoord; 
    in vec4 aTexCoord; 
    in vec4 aExtra; 
    in vec4 aNormal; 
    in vec4 aLight; 
    in vec4 aColor; 
    in vec4 aFlags; 
void main() {
    vec4 p = vec4(dot(uModelMatrix[0], vec4(aCoord.xyz, 1.0)), dot(uModelMatrix[1], vec4(aCoord.xyz, 1.0)), dot(uModelMatrix[2], vec4(aCoord.xyz, 1.0)), 1.0);
    vPos = p.xyz;
    gl_Position = ((uProjMatrix) * (vec4(dot(uViewMatrix[0].xyz, (p).xyz), dot(uViewMatrix[1].xyz, (p).xyz), dot(uViewMatrix[2].xyz, (p).xyz), (p).w)));
}
