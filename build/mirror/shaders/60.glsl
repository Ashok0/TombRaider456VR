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
    in vec2 vTexCoord;
    in vec3 vColor;
    in vec3 vLight;
    in float vLayer;
    in float vFog;
    in vec3 vNormal;
    in vec3 vPos;
out vec4 fragColor; void main() {
    vec4 d;
    vec2 uv = vTexCoord;
    vec3 normal = normalize(vNormal);
    vec3 light = vLight;
    vec3 color = vColor;
    vec3 pos = vPos;
    vec3 viewVec = normalize(-pos);
        d = texture(sTex0_wrap, vec3(uv.xy, vLayer));
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }
    light += color; 
    float NdotV = max(0.0, dot(normal.xyz, viewVec));
	light.xyz *= NdotV;
    light.xyz = (pow(light.xyz, uFogPlane.xxx) + light.xyz * (1.0 - uFogPlane.x) * 2.0);
    d.xyz *= light * 2.0;
    {
        d.xyz = mix(uFogColor.xyz * d.w, d.xyz, vFog);
    }
    fragColor = d;
}
