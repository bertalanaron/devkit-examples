#version 330 core

struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

uniform sampler2D u_texture;

centroid in vec3 Normal;
in vec3 Position;
in vec2 UV;

out vec4 FragColor;

void main()
{
    vec3 sunDir = normalize(vec3(1, 1, 0));
    float d = dot(sunDir, Normal);

    vec4 color = texture(u_texture, vec2(UV)) * d;

    FragColor = color;
}
