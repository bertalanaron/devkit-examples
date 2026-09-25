#version 330 core

in vec3 TexCoords;

out vec4 FragColor;

uniform samplerCube u_skybox;

void main()
{    
    FragColor = texture(u_skybox, TexCoords);
	// FragColor = vec4(TexCoords, 0);
}
