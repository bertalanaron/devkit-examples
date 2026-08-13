#version 330 core

struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

uniform sampler2D u_grassTexture1;
uniform sampler2D u_grassTexture2;
uniform sampler2D u_rockTexture;

in vec3 Normal;
in vec3 Position;

out vec4 FragColor;

// --- Controls ---
vec2  uvScale    = vec2(.15, .15); // scale for sampling the textures
float noiseScale = .4;             // frequency of the noise mask
float time       = 0.0;            // animate noise (optional)
int   octaves    = 5;              // fbm octave count (1–6 is plenty)
float band       = 0.2;              // 0 = hard cut, larger = softer blend band
float speed      = 0.05;           // animation speed

// ---------- Perlin-style 2D gradient noise ----------
float hash11(vec2 p) {
    // cheap hash -> [0,1)
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

vec2 grad(vec2 lattice) {
    // random unit direction from hashed angle
    float a = 6.28318530718 * hash11(lattice);
    return vec2(cos(a), sin(a));
}

float perlin(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0); // quintic fade

    float n00 = dot(grad(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0));
    float n10 = dot(grad(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
    float n01 = dot(grad(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
    float n11 = dot(grad(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));

    float nx0 = mix(n00, n10, u.x);
    float nx1 = mix(n01, n11, u.x);
    return mix(nx0, nx1, u.y); // roughly in [-1,1]
}

float fbm(vec2 p, int octs) {
    float sum = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 8; ++i) { // unrolled max; use 'octs' to early-out
        if (i >= octs) break;
        sum += perlin(p) * amp;
        p *= 2.0;
        amp *= 0.5;
    }
    // sum of amps approaches 1, remap to [0,1]
    return 0.54 + 0.5 * sum;
}


void main()
{
    float prod = 0.0;
	
	float isUp = dot(Normal, vec3(0, 1, 0));
	vec4 textureColor;
	if (isUp > 0.8)
	{
		vec2 uv = Position.xz * uvScale;
		vec2 nUV = Position.xz * noiseScale + vec2(0.0, time * speed);
		float n = fbm(nUV, octaves);
		float mask = smoothstep(0.5 - band, 0.5 + band, n);
		vec4 A = texture(u_grassTexture1, uv);
		vec4 B = texture(u_grassTexture2, uv);

		textureColor = mix(A, B, mask);
		textureColor = mix(A, B, mask);
	}
	else
		textureColor = texture(u_rockTexture, Position.xy);

    // PERSPECTIVE
    if (u_camera.VP[3][3] == 1.0) {
        prod = dot(normalize(-u_camera.direction), normalize(Normal));
    }
    // ORTHOGRAPHIC
    else {
        prod = dot(normalize(u_camera.position - Position), normalize(Normal));
    }
    
    if (prod < 0)
        prod *= -1;
    //float diffuse = prod;
	
	float diffuse = .9;
	if (mod(Position.x, .5) < .015 || mod(Position.z, .5) < .015)
		diffuse = .4;
    vec4 value = diffuse * (prod / 3 + .66) * textureColor;
    FragColor = vec4(value.xyz, 1.f);
}
