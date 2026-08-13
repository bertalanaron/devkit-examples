config: 
  patch_vertices: 4

vertex: |
  #version 410 core

  layout (location = 0) in vec3 aPos;
  layout (location = 1) in vec2 aTex;

  out vec2 v_TexCoord;

  void main()
  {
      gl_Position = vec4(aPos, 1.0);
      v_TexCoord  = aTex;
  }

tessellation_control: |
  #version 410 core

  layout (vertices=4) out;

  in vec2 v_TexCoord[];
  out vec2 tc_TexCoord[];

  uniform mat4   u_userView;
  uniform mat4   u_model;

  uniform int    u_minTessLevel;
  uniform int    u_maxTessLevel;
  uniform float  u_tessMaxDistance;
  uniform float  u_tessMinDistance;

  void main()
  {
    // Forward position and tex coords
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tc_TexCoord[gl_InvocationID]        = v_TexCoord[gl_InvocationID];

    if(gl_InvocationID == 0)
    {
      // get control point position relative to the user camera
      vec4 eyeSpacePos00 = gl_in[0].gl_Position * u_model * u_userView;
      vec4 eyeSpacePos01 = gl_in[1].gl_Position * u_model * u_userView;
      vec4 eyeSpacePos10 = gl_in[2].gl_Position * u_model * u_userView;
      vec4 eyeSpacePos11 = gl_in[3].gl_Position * u_model * u_userView;

      // get control point distances
      float distance00 = clamp((abs(eyeSpacePos00.z)-u_tessMinDistance) / (u_tessMaxDistance-u_tessMinDistance), 0.0, 1.0);
      float distance01 = clamp((abs(eyeSpacePos01.z)-u_tessMinDistance) / (u_tessMaxDistance-u_tessMinDistance), 0.0, 1.0);
      float distance10 = clamp((abs(eyeSpacePos10.z)-u_tessMinDistance) / (u_tessMaxDistance-u_tessMinDistance), 0.0, 1.0);
      float distance11 = clamp((abs(eyeSpacePos11.z)-u_tessMinDistance) / (u_tessMaxDistance-u_tessMinDistance), 0.0, 1.0);

      // interpolate min and max tess levels
      float tessLevel0 = mix(u_maxTessLevel, u_minTessLevel, min(distance10, distance00));
      float tessLevel1 = mix(u_maxTessLevel, u_minTessLevel, min(distance00, distance01));
      float tessLevel2 = mix(u_maxTessLevel, u_minTessLevel, min(distance01, distance11));
      float tessLevel3 = mix(u_maxTessLevel, u_minTessLevel, min(distance11, distance10));

      // set outer level
      gl_TessLevelOuter[0] = tessLevel0;
      gl_TessLevelOuter[1] = tessLevel1;
      gl_TessLevelOuter[2] = tessLevel2;
      gl_TessLevelOuter[3] = tessLevel3;

      // set inner level
      gl_TessLevelInner[0] = max(tessLevel1, tessLevel3);
      gl_TessLevelInner[1] = max(tessLevel0, tessLevel2);
    }
  }

tessellation_evaluation: |
  #version 410 core

  layout (quads, fractional_odd_spacing, ccw) in;
  
  struct Camera {
    mat4 view;
    mat4 projection;
  }; 

  uniform Camera    u_sun;
  uniform sampler2D u_heightMap;
  uniform mat4      u_model;

  uniform float     u_maxHeight;
  uniform float     u_heightOffset;
  uniform vec2      u_terrainSize;

  in vec2  tc_TexCoord[];
  out vec3 te_Position;

  void main()
  {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // ----------------------------------------------------------------------
    // retrieve control point texture coordinates
    vec2 t00 = tc_TexCoord[0];
    vec2 t01 = tc_TexCoord[1];
    vec2 t10 = tc_TexCoord[2];
    vec2 t11 = tc_TexCoord[3];

    // bilinearly interpolate texture coordinate across patch
    vec2 t0 = (t01 - t00) * u + t00;
    vec2 t1 = (t11 - t10) * u + t10;
    vec2 texCoord = (t1 - t0) * v + t0;

    // lookup texel at patch coordinate for height and scale + shift as desired
    float height = texture(u_heightMap, texCoord).y * u_maxHeight - u_heightOffset;

    // ----------------------------------------------------------------------
    // retrieve control point position coordinates
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;

    // compute patch surface normal
    vec4 uVec = p01 - p00;
    vec4 vVec = p10 - p00;
    vec4 normal = normalize( vec4(cross(vVec.xyz, uVec.xyz), 0) );

    // bilinearly interpolate position coordinate across patch
    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;
    vec4 p = (p1 - p0) * v + p0;

    // displace point along normal
    p += normal * height;

    // ----------------------------------------------------------------------
    // output patch point position in clip space
    gl_Position = p * u_model * u_sun.view * u_sun.projection;
    te_Position = p.xyz;
  }

fragment: |
  #version 410 core

  in vec3 te_Position;

  out vec4 FragColor;

  void main()
  {
    FragColor = vec4(te_Position, 1.0);
  }
