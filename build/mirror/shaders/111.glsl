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
	in vec4 vTexCoord;
	in vec4 vColor;
	in vec3 vLight;
	in float vLayer;
	in float vFog;
	in vec3 vNormal;
	in vec3 vPos;
	in vec4 vSProj;
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
out vec4 fragColor; void main() {
	float y = texture(sTex0, vec3(vTexCoord.xy.xy, 0.0)).r;
	vec2 uv = texture(sTex1, vec3(vTexCoord.xy.xy, 0.0)).rg - 0.5;
	float r, g, b;
	{
		y -= 1.0 / 15.0;
		y *= 1.1643;
		r = y + uv.y * 1.5958;
		g = y - uv.x * 0.39173 - uv.y * 0.8129;
		b = y + uv.x * 2.017;
	}
    vec4 d = vec4(r, g, b, 1.0);
	if (uParams.w > 0.0)
	{
		d.xyz = pow(d.xyz, vec3(2.2, 2.2, 2.2));
	}
    fragColor = d;
}
