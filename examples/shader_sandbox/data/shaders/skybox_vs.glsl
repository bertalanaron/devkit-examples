#version 330 core

layout (location = 0) in vec3 a_pos;
    
struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

out vec3 TexCoords;

void main()
{
    TexCoords = a_pos;
    vec4 pos = vec4(a_pos + u_camera.position, 1.0) * u_camera.VP;
    gl_Position = pos.xyww;
}  
