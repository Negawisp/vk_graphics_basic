#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "common.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;


layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (binding = 1) uniform sampler2D heightmap;

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const float HeightMult = 5.0f;
    const vec2 MapSize = textureSize(heightmap, 0);

    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.texCoord = vTexCoordAndTang.xy;

    // Height
    float height = textureLod(heightmap, vOut.texCoord, 0).x * HeightMult;
    vOut.wPos.y = height;
    
    // Normal
    vec2 d  = vec2(1 / MapSize.x, 1 / MapSize.y);
    vec2 dx = vec2(d.x , 0.0);
    vec2 dy = vec2(0.0 , d.y);
    float r = textureLod(heightmap, vOut.texCoord + dx, 0).x;
    float l = textureLod(heightmap, vOut.texCoord - dx, 0).x;
    float t = textureLod(heightmap, vOut.texCoord + dy, 0).x;
    float b = textureLod(heightmap, vOut.texCoord - dy, 0).x;
    vec3 norm = vec3((r - l) / (2.0f * d.x), -1.0f, (t - b) / (2.0f * d.y));
    norm = normalize(norm);
    vOut.wNorm = norm;

    //vOut.wNorm = vec3

    // Lighting
    float shadow = 1.0f;
    vec3 lightDir = normalize(Params.lightPos.xyz);
    vec3 rayPos = vec3(vOut.texCoord.x, height, vOut.texCoord.y);
    vec3 step = +lightDir / MapSize.x;
    step.y = -lightDir.y / 150.0f;

    if (length(lightDir.xz) > 0.1)
    {
      while (all(lessThan   (rayPos.xz, vec2(1))) &&
             all(greaterThan(rayPos.xz, vec2(0))))
      {
        rayPos += step;

        float obstacleHeight = textureLod(heightmap, rayPos.xz, 0).x * HeightMult;
        if (obstacleHeight > rayPos.y) {
          shadow = 0.0;
          break;
        }
      }
    }
    vOut.wTangent.x = shadow;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
