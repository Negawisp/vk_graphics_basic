#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec2 out_fragColor;
layout (binding = 0) uniform sampler2D shadowMap;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    bool vsmEnabled;
} pushConst;

void main()
{
    vec2 resolution = textureSize(shadowMap, 0);

    if (!pushConst.vsmEnabled) {
      out_fragColor = vec2(textureLod(shadowMap, gl_FragCoord.xy / resolution, 0).x, 0);
      return;
    }


    int r = 4;
    int n = (2*r + 1) * (2*r + 1);

    float M1 = 0;
    float M2 = 0;
    float t;

    for (float i = -r; i <= r; i++) {
        for (float j = -r; j <= r; j++) {
            t = textureLod(shadowMap, (gl_FragCoord.xy + vec2(i, j)) / resolution, 0).x;
            M1 += t;
            M2 += t*t;
        }
    }
    M1 /= n;
    M2 /= n;

    out_fragColor = vec2(M1, M2);
}
