#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

vec4 reinhard(vec4 color)
{
    vec3 c = color.xyz;
    c = c / (1.0f + c);

    return vec4(c, color.w);
}

vec4 reinhard_extended(vec4 color, float max_white)
{
    vec3 c = color.xyz;
    c = c / (1.0f + c) * (1.0f + (c / vec3(max_white * max_white)));

    return vec4(c, color.w);
}

vec4 filmic(vec4 color)
{
    float A = 2.51f;
    float B = 0.03f;
    float C = 2.43f;
    float D = 0.59f;
    float E = 0.14f;

    vec3 c = color.xyz;
    vec3 numerator = c * (A * c + B);
    vec3 denominator = c * (C * c + D) + E;
    c = numerator / denominator;
    c = clamp(c, 0, 1);

    return vec4(c, color.w);
}

// ~~~~~ RGB to HSV conversion ~~~~~ //
// Taken from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
// ~~~~~ RGB to HSV conversion END ~~~~~ //

vec4 cell_shading(vec4 color, int steps)
{
    vec3 c_rgb = color.xyz;
    vec3 c_hsv = rgb2hsv(c_rgb);

    c_hsv.z = floor(c_hsv.z * steps) / steps;

    return vec4(hsv2rgb(c_hsv), color.w);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void main()
{
  vec2 uv = surf.texCoord;
  uv.y = uv.y + 1;
  
  vec4 color = textureLod(colorTex, uv, 0);
  
  //color = reinhard(color);
  //color = reinhard_extended(color, 0.2f);
  color = filmic(color);

  color = cell_shading(color, 3);

  out_color = color;
}
