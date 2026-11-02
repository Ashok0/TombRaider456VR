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
vec4 GetFog( const vec3 vPosition)
{
    vec4 pos = vec4(vec3(1.0, 1.0, 1.0) * length(vPosition.xyz) / 3.0f, 1.0);
    float FogDistance = dot(pos, uFogPlane);
    vec4 cFog = uFogColor;
	cFog.a *= clamp( FogDistance, 0.0, 1.0);
	return( cFog);
}
vec3 getWorldCameraPos()
{
	vec3 pos = vec3(uViewMatrix[0].w, uViewMatrix[1].w, uViewMatrix[2].w);
	vec3 i_inv = vec3(uViewMatrix[0].x, uViewMatrix[1].x, uViewMatrix[2].x);
	vec3 j_inv = vec3(uViewMatrix[0].y, uViewMatrix[1].y, uViewMatrix[2].y);
	vec3 k_inv = vec3(uViewMatrix[0].z, uViewMatrix[1].z, uViewMatrix[2].z);
	return vec3(-dot(i_inv, pos), -dot(j_inv, pos), -dot(k_inv, pos));
}
    out vec2 vTexCoord;
    out vec4 vColor;
    in vec3 aCoord; 
    in vec4 aColor;   
    in vec4 aTexCoord;   
void main() {
    vec4 pos = vec4(aCoord, 1.0);
	float x_offset = aTexCoord.z;
	float y_offset = aTexCoord.w;
    vec4 point_dir = vec4(pos.xyz - getWorldCameraPos(), 0.0);
    vec4 camera_right = vec4(normalize(cross(point_dir.xyz, vec3(0.0, 0.0, 1.0))), 0.0);
    vec4 camera_up = vec4(normalize(cross(point_dir.xyz, camera_right.xyz)), 0.0);
    pos += uParams.x * (camera_right * x_offset + camera_up * y_offset);
	vec4 Position = vec4(dot(uViewMatrix[0], pos), dot(uViewMatrix[1], pos), dot(uViewMatrix[2], pos), (pos).w);
	gl_Position = ((uProjMatrix) * (Position));
    gl_Position.xy += uParams.y * vec2(x_offset, y_offset) * gl_Position.w;
	vTexCoord = aTexCoord.xy;
	vColor = aColor;
	vColor.rgb *= 1 - GetFog(Position.xyz).a;
}
