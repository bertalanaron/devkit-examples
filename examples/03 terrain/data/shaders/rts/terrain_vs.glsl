#version 330 core

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;

struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

out vec3 Position;
out vec3 Normal;

void main()
{
    vec4 vM = vec4(vertex.xyz, 1.0);

    gl_Position = vM * u_camera.VP;
    Position = vec3(vM.x / vM.w, vM.y / vM.w, vM.z / vM.w);
    Normal    = normal;
}
