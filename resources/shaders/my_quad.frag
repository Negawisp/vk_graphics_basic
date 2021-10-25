#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;


const int   n  = 10;
  
const float g_sigma = 100;
const float k_g = 1 / ( 2.0 * g_sigma * g_sigma);

const float r_sigma = 0.1;
const float k_r = 1 / ( 2.0 * r_sigma * r_sigma);

const vec4 k_lum = vec4 (0.2126, 0.7152, 0.0722, 0.0);

void main()
{
  ivec2	sz  = textureSize ( colorTex, 0 );
  float dx  = 1.0 / float ( sz.x );
  float dy  = 1.0 / float ( sz.y );
  vec2  tex = surf.texCoord;
  float lum = dot ( k_lum, texture ( colorTex, tex ) );

  vec4	clr = vec4 ( 0.0 );
  float sum = 0.0;

  for ( int i = -n; i <= n; i++ )
  {
	for ( int j = -n; j <= n; j++ )
	{
	  float i_f    = float ( i );
	  float j_f    = float ( j );

	  vec4  c      = texture ( colorTex, tex + vec2 ( i*dx, j*dy ) );
	  
	  float l      = dot (k_lum, c);
	  float l_dist = l - lum;
	  
	  float w_g    = exp ( -k_g * ( i_f * i_f + j_f * j_f ) );
	  float w_r    = exp ( -k_r * l_dist * l_dist );
	  float w      = w_g * w_r;

	  clr += w * c;
	  sum += w;
	}	
  }

  color = clr / sum;

  //color = textureLod(colorTex, surf.texCoord, 0);
}
