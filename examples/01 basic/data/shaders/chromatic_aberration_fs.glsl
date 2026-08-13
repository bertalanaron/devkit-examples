#version 330

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

in vec2 UV;
out vec4 FragColor;

uniform sampler2D u_texture;
uniform float u_t;

void main()
{
    // Compute the distance from the center of the screen
    vec2 center = vec2(0.5, 0.5);
    float dist = length(UV - center);

    // Set the maximum chromatic aberration at the edges (you can adjust the strength here)
	float factor = max(0.0, rand(vec2(u_t / 9999.0, 0.0)) - .7);
    float maxSeparation = 0.3 * factor;  // Maximum amount of aberration at the edges
    float separation = maxSeparation * pow(dist, 2);  // Linear gradient of aberration strength
    
    // Use sin/cos for each color's offset
    vec2 clockwise     = vec2(sin(separation), cos(separation));
    vec2 anticlockwise = vec2(-sin(separation), cos(separation));
    vec2 offsetRed     = clockwise;
    vec2 offsetGreen   = anticlockwise;
    vec2 offsetBlue    = vec2(cos(separation), sin(separation));
    
    // Sample colors from the texture with the adjusted offsets
    vec4 red   = texture(u_texture, UV - offsetRed);
    vec4 green = texture(u_texture, UV - offsetGreen);
    vec4 blue  = texture(u_texture, UV - offsetBlue);
    
    // Combine the colors with the chromatic aberration effect
    FragColor = vec4(red.r, green.g, blue.b, 1.0);
}
