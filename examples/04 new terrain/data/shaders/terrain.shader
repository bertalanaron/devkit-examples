config: 
  patch_vertices: 4

vertex: |
  #version 410 core

  // vertex position
  layout (location = 0) in vec3 aPos;
  // texture coordinate
  layout (location = 1) in vec2 aTex;

  out vec2 TexCoord;

  void main()
  {
      // convert XYZ vertex to XYZW homogeneous coordinate
      gl_Position = vec4(aPos, 1.0);
      // pass texture coordinate though
      TexCoord = aTex;
  }

tessellation_control: |
  #version 410 core

  // specify number of control points per patch output
  // this value controls the size of the input and output arrays
  layout (vertices=4) out;

  // varying input from vertex shader
  in vec2 TexCoord[];
  // varying output to evaluation shader
  out vec2 TextureCoord[];

  struct Camera {
    mat4 view;
    mat4 projection;
    vec3 position;
    vec3 direction;
  }; uniform Camera u_camera;

  uniform mat4 u_model;         // the model matrix

  uniform int u_minTessLevel;
  uniform int u_maxTessLevel;
  uniform float u_tessMaxDistance;
  uniform float u_tessMinDistance;

  void main()
  {
    // ----------------------------------------------------------------------
    // pass attributes through
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    TextureCoord[gl_InvocationID] = TexCoord[gl_InvocationID];

    // ----------------------------------------------------------------------
    if(gl_InvocationID == 0)
    {
      // ----------------------------------------------------------------------
      // Step 1: define constants to control tessellation parameters
      // set these as desired for your world scale
      float MIN_TESS_LEVEL = u_minTessLevel;
      float MAX_TESS_LEVEL = u_maxTessLevel;
      float MIN_DISTANCE = u_tessMinDistance;
      float MAX_DISTANCE = u_tessMaxDistance;

      // ----------------------------------------------------------------------
      // Step 2: transform each vertex into eye space
      vec4 eyeSpacePos00 = gl_in[0].gl_Position * u_model * u_camera.view;
      vec4 eyeSpacePos01 = gl_in[1].gl_Position * u_model * u_camera.view;
      vec4 eyeSpacePos10 = gl_in[2].gl_Position * u_model * u_camera.view;
      vec4 eyeSpacePos11 = gl_in[3].gl_Position * u_model * u_camera.view;

      // ----------------------------------------------------------------------
      // Step 3: "distance" from camera scaled between 0 and 1
      float distance00 = clamp((abs(eyeSpacePos00.z)-MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0);
      float distance01 = clamp((abs(eyeSpacePos01.z)-MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0);
      float distance10 = clamp((abs(eyeSpacePos10.z)-MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0);
      float distance11 = clamp((abs(eyeSpacePos11.z)-MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0);

      // ----------------------------------------------------------------------
      // Step 4: interpolate edge tessellation level based on closer vertex
      float tessLevel0 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance10, distance00) );
      float tessLevel1 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance00, distance01) );
      float tessLevel2 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance01, distance11) );
      float tessLevel3 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance11, distance10) );

      // ----------------------------------------------------------------------
      // Step 5: set the corresponding outer edge tessellation levels
      gl_TessLevelOuter[0] = tessLevel0;
      gl_TessLevelOuter[1] = tessLevel1;
      gl_TessLevelOuter[2] = tessLevel2;
      gl_TessLevelOuter[3] = tessLevel3;

      // ----------------------------------------------------------------------
      // Step 6: set the inner tessellation levels to the max of the two parallel edges
      gl_TessLevelInner[0] = max(tessLevel1, tessLevel3);
      gl_TessLevelInner[1] = max(tessLevel0, tessLevel2);
    }
  }

tessellation_evaluation: |
  #version 410 core

  layout (quads, fractional_odd_spacing, ccw) in;

  uniform sampler2D u_heightMap;  // the texture corresponding to our height map
  
  struct Camera {
    mat4 view;
    mat4 projection;
    vec3 position;
    vec3 direction;
  }; uniform Camera u_camera;

  uniform mat4 u_model;         // the model matrix

  uniform float u_maxHeight;
  uniform float u_heightOffset;
  uniform vec2 u_terrainSize;

  // received from Tessellation Control Shader - all texture coordinates for the patch vertices
  in vec2 TextureCoord[];
  out vec3 TessPosition;
  out vec2 TessTexCoord;
  out vec3 TessNormal;

  // send to Fragment Shader for coloring
  out float Height;

  void main()
  {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // ----------------------------------------------------------------------
    // retrieve control point texture coordinates
    vec2 t00 = TextureCoord[0];
    vec2 t01 = TextureCoord[1];
    vec2 t10 = TextureCoord[2];
    vec2 t11 = TextureCoord[3];

    // bilinearly interpolate texture coordinate across patch
    vec2 t0 = (t01 - t00) * u + t00;
    vec2 t1 = (t11 - t10) * u + t10;
    vec2 texCoord = (t1 - t0) * v + t0;
    TessTexCoord = texCoord;

    // lookup texel at patch coordinate for height and scale + shift as desired
    Height = texture(u_heightMap, texCoord).y * u_maxHeight - u_heightOffset;

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
    p += normal * Height;

    // ----------------------------------------------------------------------
    // Calculate normal
    vec2 texel = 1.0 / textureSize(u_heightMap, 0);

    float hL = texture(u_heightMap, texCoord - vec2(texel.x, 0)).r * u_maxHeight;
    float hR = texture(u_heightMap, texCoord + vec2(texel.x, 0)).r * u_maxHeight;
    float hD = texture(u_heightMap, texCoord - vec2(0, texel.y)).r * u_maxHeight;
    float hU = texture(u_heightMap, texCoord + vec2(0, texel.y)).r * u_maxHeight;

    float dx = 500 * texel.x;
    float dz = 200 * texel.y;

    vec3 tangent = normalize(vec3(2.0 * dx, hR - hL, 0.0));
    vec3 bitangent = normalize(vec3(0.0, hU - hD, 2.0 * dz));

    TessNormal = normalize(cross(bitangent, tangent));

    // ----------------------------------------------------------------------
    // output patch point position in clip space
    gl_Position = p * u_model * u_camera.view * u_camera.projection;
    TessPosition = p.xyz;
  }

# fragment: |
#   #version 410 core

#   in float Height;

#   out vec4 FragColor;

#   void main()
#   {
#     float h = (Height)/8.0f;
#     FragColor = vec4(h, h, h, 1.0);
#   }

# ------------------------- Wireframe with lines -------------------------

# geometry: |
#   #version 410 core

#   layout (triangles) in;
#   layout (line_strip, max_vertices = 6) out;  // 3 edges × 2 vertices each

#   in float Height[];
#   out float Height_f;

#   void EmitEdge(int i0, int i1)
#   {
#       Height_f = Height[i0];
#       gl_Position = gl_in[i0].gl_Position;
#       EmitVertex();

#       Height_f = Height[i1];
#       gl_Position = gl_in[i1].gl_Position;
#       EmitVertex();

#       EndPrimitive();
#   }

#   void main()
#   {
#       // Emit the 3 edges of the triangle
#       EmitEdge(0, 1);
#       EmitEdge(1, 2);
#       EmitEdge(2, 0);
#   }


# fragment: |
#   #version 410 core

#   in float Height_f;
#   out vec4 FragColor;

#   void main()
#   {
#       float h = (Height_f + 8.0) / 16.0;
#       FragColor = vec4(1., 1., 0.0, 1.0); // pure black lines
#   }

# ----------------- wireframe without lines

geometry: |
  #version 410 core

  // Input primitive from tessellation evaluation
  layout (triangles) in;

  // Output primitive to rasterizer
  layout (triangle_strip, max_vertices = 3) out;

  // receive per-vertex varyings from tess eval
  in float Height[]; // name matches tess eval's 'out float Height;'
  in vec3 TessPosition[];
  in vec2 TessTexCoord[];
  in vec3 TessTangent[];
  in vec3 TessBitangent[];
  in vec3 TessNormal[];

  // pass to fragment shader
  out float Height_f;
  out vec3 Barycentric;
  out vec3 Position;
  out vec2 TexCoord;
  out vec3 Tangent;
  out vec3 Bitangent;
  out vec3 Normal;

  uniform bool u_shadeFlat;

  void main()
  {
    // Emit the triangle vertices, tagging barycentric coordinates
    // so fragment shader can detect edges.
    for(int i = 0; i < 3; ++i)
    {
      TexCoord  = TessTexCoord[i];
      Position  = TessPosition[i];
      Tangent   = TessTangent[i];
      Bitangent = TessBitangent[i];

      // barycentric coordinates: (1,0,0), (0,1,0), (0,0,1)
      if(i == 0) Barycentric = vec3(1.0, 0.0, 0.0);
      else if(i == 1) Barycentric = vec3(0.0, 1.0, 0.0);
      else       Barycentric = vec3(0.0, 0.0, 1.0);

      Height_f = Height[i];

      Normal = normalize(cross(TessPosition[(i + 2) % 3] - TessPosition[i], TessPosition[(i + 1) % 3] - TessPosition[i]));

      gl_Position = gl_in[i].gl_Position;
      EmitVertex();
    }
    EndPrimitive();
  }

fragment: |
  #version 410 core

  in float Height_f;
  in vec3 Barycentric;
  in vec2 TexCoord;
  in vec3 Position;
  in vec3 Normal;

  out vec4 FragColor;

  struct Camera {
    mat4 view;
    mat4 projection;
    vec3 position;
    vec3 direction;
  }; uniform Camera u_camera;

  uniform mat4 u_model;

  uniform sampler2D u_simplexNoise;
  uniform sampler2D u_heightMap;
  uniform sampler2D u_terrainTexture;
  uniform sampler2D u_terrainTexture2;
  uniform sampler2D u_terrainTexture3;

  uniform float u_maxHeight;
  uniform float u_heightOffset;
  uniform bool  u_visualizeNormals;
  uniform bool  u_shadeFlat;
  uniform bool  u_drawWireframe;
  uniform bool  u_textureBombing;
  uniform bool  u_triplanarSampling;

  vec3 getNormal()
  {
    if (u_shadeFlat)
    {
      return Normal;
    }
    vec2 texel = 1.0 / textureSize(u_heightMap, 0);

    float hL = texture(u_heightMap, TexCoord - vec2(texel.x, 0)).r * u_maxHeight;
    float hR = texture(u_heightMap, TexCoord + vec2(texel.x, 0)).r * u_maxHeight;
    float hD = texture(u_heightMap, TexCoord - vec2(0, texel.y)).r * u_maxHeight;
    float hU = texture(u_heightMap, TexCoord + vec2(0, texel.y)).r * u_maxHeight;

    float dx = 500 * texel.x;
    float dz = 200 * texel.y;

    vec3 tangent = normalize(vec3(2.0 * dx, hR - hL, 0.0));
    vec3 bitangent = normalize(vec3(0.0, hU - hD, 2.0 * dz));

    return normalize(cross(bitangent, tangent));
  }

  float getDifuse() {
    //mat3 TBN = mat3(normalize(Tangent), normalize(Bitangent), normalize(vec3(0, 1, 0)));
    // vec3 Normal = normalize(TBN * (texture(u_normalMap, TexCoord).rgb * 2.0 - 1.0));
    // vec3 dx = dFdx(Position);
    // vec3 dy = dFdy(Position);
    // vec3 Normal = normalize(cross(dy, dx));

    vec3 normal = getNormal();

    float prod = 0.0;
    // PERSPECTIVE
    if (u_camera.projection[3][3] == 1.0) {
      prod = dot(normalize(-u_camera.direction), normalize(normal));
    }
    // ORTHOGRAPHIC
    else {
      prod = dot(normalize(u_camera.position - Position), normalize(normal));
    }
    //if (prod < 0)
    //  prod *= -1;
    prod = clamp(prod, 0, 1);
    prod = prod * .2 + .5;
    return prod;
  }

  // vec4 hash4(ivec2 p)
  // {
  //     const vec4 prime = vec4(127.1, 311.7, 74.7, 18.3);
  //     vec4 res = vec4(dot(vec2(p.x, p.y), prime.xy), dot(vec2(p.x, p.y), prime.zw), 0.0, 0.0);
  //     res = fract(res);
  //     return res;
  // }

  vec4 hash4( ivec2 p ) 
  { 
    return fract(sin(vec4( 1.0+dot(p,vec2(37.0,17.0)),
                                           2.0+dot(p,vec2(11.0,47.0)),
                                           3.0+dot(p,vec2(41.0,29.0)),
                                           4.0+dot(p,vec2(23.0,31.0))))*103.0); 
  }

  float sum( vec3 v ) { return v.x+v.y+v.z; }

  // https://www.shadertoy.com/view/Xtl3zf
  vec4 bombedTerrainTex(in sampler2D tex, in vec2 uv) {
    if (!u_textureBombing)
      return texture(tex, uv);
    float k = texture(u_simplexNoise, 0.0025*uv.xy).x; // cheap (cache friendly) lookup
    float l = k*8.0;
    float f = fract(l);
    
    float ia = floor(l+0.5); // suslik's method (see comments)
    float ib = floor(l);
    f = min(f, 1.0-f)*2.0;

    vec2 offa = sin(vec2(3.0,7.0)*ia); // can replace with any other hash
    vec2 offb = sin(vec2(3.0,7.0)*ib); // can replace with any other hash

    vec4 cola = texture(tex, vec2(uv.xy + offa));
    vec4 colb = texture(tex, vec2(uv.xy + offb));

    return mix(cola, colb, smoothstep(0.2, 0.8, f - 0.1 * sum(cola.xyz - colb.xyz)));
  }

  const float _TRI_SCALE = 1.0;

  vec4 triplanar(vec3 pos, vec3 normal, sampler2D tex) {
    vec4 dx = bombedTerrainTex(tex, vec2(pos.zy / _TRI_SCALE));
    vec4 dy = bombedTerrainTex(tex, vec2(pos.xz / _TRI_SCALE));
    vec4 dz = bombedTerrainTex(tex, vec2(pos.xy / _TRI_SCALE));

    vec3 weights = abs(normal.xyz);
    weights = weights / (weights.x + weights.y + weights.z);

    return dx * weights.x + dy * weights.y + dz * weights.z;
  }

  vec4 sampleTerrainTex(vec3 position, vec3 normal, sampler2D tex) {
    if (u_triplanarSampling)
      return triplanar(position, normal, tex);
    else
      return vec4(bombedTerrainTex(tex, position.xz).rgb, 1.0);
  }

  void main()
  {
      // base grayscale shading from Height
      float h = (Height_f + u_maxHeight) / (u_maxHeight * 2.0);
      vec4 base;
      if (dot(getNormal(), vec3(0,1,0)) > .8)
        if (h > .8)
        {
          base = sampleTerrainTex(Position, getNormal(), u_terrainTexture);
          vec4 terrainColor = sampleTerrainTex(Position, getNormal(), u_terrainTexture);
          vec4 snowColor = sampleTerrainTex(Position, getNormal(), u_terrainTexture3);
          base.xyz = mix(terrainColor.xyz, snowColor.xyz, snowColor.w);
        }
        else
          base = sampleTerrainTex(Position, getNormal(), u_terrainTexture);
      else
        base = sampleTerrainTex(Position, getNormal(), u_terrainTexture2);

      // detect edge using minimum barycentric coordinate
      float edgeMetric = min(min(Barycentric.x, Barycentric.y), Barycentric.z);

      // tweak this to control line thickness (in barycentric space)
      float lineWidth = 0.02; // ~2% of triangle -> change to taste

      // smoothstep for antialiased lines:
      // edgeMetric ~ 0 on edges, ~0.33 near center (for equilateral); smoothstep maps to [0..1]
      float edgeFactor = smoothstep(0.0, lineWidth, edgeMetric);

      // line color (black). You can change this.
      vec4 lineColor = vec4(0.0, 0.0, 0.0, 1.0);

      // mix line color (when edgeFactor close to 0) with base otherwise
      vec4 color = base * getDifuse();
      if (u_visualizeNormals)
        color = vec4(getNormal(), 1.0);

      FragColor = color;
      if (u_drawWireframe)
        FragColor = mix(lineColor, color, edgeFactor);
  }

