#version 330 core

struct Camera {
    mat4 VP;
    vec3 position;
    vec3 direction;
}; uniform Camera u_camera;

uniform sampler2D u_texture;
uniform int       u_useSpecular;
uniform sampler2D u_specular;
uniform sampler2D u_normal;

in vec3 Normal;
in vec3 Position;
in vec2 UV;
in vec3 Tangent;
in vec3 Bitangent;

out vec4 FragColor;

mat3 getTBN(vec3 normal, vec3 tangent, vec3 bitangent) {
    return mat3(
        normalize(tangent),
        normalize(bitangent),
        normalize(normal)
    );
}

vec3 sampleNormal(vec2 uv, mat3 TBN) {
    vec3 tangentNormal = texture(u_normal, uv).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0; // from [0,1] to [-1,1]
    return normalize(TBN * tangentNormal);
}

void main()
{
    vec3 sunDir = normalize(vec3(1, 1, 0));
    float d = dot(sunDir, Normal);
	
	if (u_useSpecular > 0) {
	    // Compute TBN matrix
        mat3 TBN = getTBN(normalize(Normal), normalize(Tangent), normalize(Bitangent));
		
		vec3 norm = sampleNormal(UV, TBN);
		//norm = normalize(Normal);
		vec3 reflectDir = reflect(sunDir, norm);

		// Sample the diffuse and specular maps
		vec3 diffuseColor  = texture(u_texture, UV).rgb;
		float specStrength = texture(u_specular, UV).r;

		// Diffuse shading
		float diff = max(dot(norm, sunDir), 0.0);
		
		// Specular shading
		float spec = pow(max(dot(normalize(u_camera.direction), reflectDir), 0.0), 12.0); // 32 = shininess
		vec3 specular = specStrength * spec * vec3(0.3, 0.3, 0.4); // white specular light

		vec3 result = (diffuseColor * diff + specular);
		FragColor = vec4(result, 1.0);
		// FragColor = vec4(norm, 1.0);
	} else {
		FragColor = texture(u_texture, UV) * d;
	}
}
