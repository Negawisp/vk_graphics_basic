#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
{
  vec2 uv = surf.texCoord;
  uv.y = uv.y + 1;
  
  vec4 color = textureLod(colorTex, uv, 0);

  out_color = color;
}
