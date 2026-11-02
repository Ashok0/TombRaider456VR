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
    in vec3 vWorldPos;
out vec4 fragColor; void main() {
    vec4 d;
    vec2 uv = vTexCoord;
    vec3 normal = normalize(vNormal);
    vec3 light = vLight;
    vec3 color = vColor;
    vec3 tc = vWorldPos.xyz / 1024.0 * uParams.x; 
    float t = uModelMatrix[3].x * uParams.z; 
    tc += normal * t;
    float n0 = texture(sNoise, tc).x;
    float n1 = texture(sNoise, tc + vec3(1.0 / 128.0, 0.0, 0.0)).x;
    float n2 = texture(sNoise, tc + vec3(0.0, 1.0 / 128.0, 0.0)).x;
    vec3 v1 = vec3(1.0, n1 - n0, 0.0);
    vec3 v2 = vec3(0.0, n2 - n0, 1.0);
    vec2 v = normalize(cross(v1, v2)).xz * uParams.y; 
    d = texture(sTex0_wrap, vec3(uv + v.xy, vLayer));
    d.xyz *= (light + color) * 2.0;
    {
        d.xyz = mix(uFogColor.xyz * d.w, d.xyz, vFog);
    }
    fragColor = d;
}
