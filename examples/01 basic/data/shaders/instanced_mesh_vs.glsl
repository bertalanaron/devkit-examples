#version 330 core

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in mat4 a_M;

struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

uniform float u_t;

out vec3 Position;
out vec3 Normal;
out vec2 UV;

mat4 rotation(float t) {
    float c = cos(t);
    float s = sin(t);

    return mat4(
        vec4( c, 0.0,  s, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(-s, 0.0,  c, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
}

void main()
{
    mat4 M = rotation(u_t) * a_M;
    vec4 vM = M * vec4(vertex.xyz, 1.0);

    gl_Position =  vM * u_camera.VP;

    Position = vec3(vM.x / vM.w, vM.y / vM.w, vM.z / vM.w);
    Normal = normalize(mat3(transpose(inverse(M))) * normal);
    UV = uv;
}
