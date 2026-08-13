#version 330 core

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

out vec3 Position;
out vec3 Normal;
out vec2 UV;
out vec3 Tangent;
out vec3 Bitangent;

void main()
{
    vec4 vM = vec4(vertex.xyz, 1.0);

    gl_Position = vM * u_camera.VP;
    Position = vec3(vM.x / vM.w, vM.y / vM.w, vM.z / vM.w);

    Normal    = normal;
    UV        = uv;
	Tangent   = tangent;
	Bitangent = bitangent;
}
