#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (triangles) in;
layout (invocations = 6) in;

layout (triangle_strip, max_vertices = 15) out;

layout(push_constant) uniform params_t
{
	mat4 mProjView;
	mat4 mModel;
} params;

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} surfInp[3];

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} surfOut;

layout (binding = 0, set=0) uniform AppData
{
    UniformParams Params;
};

vec3 rotateZaxis(vec3 v, float deg) {
    return vec3(v.x*cos(deg)-v.y*sin(deg),v.x*sin(deg)+v.y*cos(deg),v.z);
}

void main() {
    vec3 center = vec3(0);
    vec3 normal = vec3(0);
    for (uint i = 0; i < 3; ++i)
    {
        center += surfInp[i].wPos;
        normal += surfInp[i].wNorm;
    }
    center /= 3.0;
    normal /= 3.0;

    vec3 h1 = normal * 0.04f;
    vec3 h2 = normal * 0.02f;
    vec3 w1 = normalize(surfInp[0].wPos - center) * 0.002f;
    vec3 w2 = normalize(surfInp[0].wPos - center) * 0.004f;
    

    vec3[5][3] out_triangles;
    out_triangles[0] = vec3[3](center + w1,      center - w1,      center + w1 + h1);
    out_triangles[1] = vec3[3](center - w1 + h1, center + w1 + h1, center - w1     );
    out_triangles[2] = vec3[3](center + w1 + h1, center - w1 + h1, center + h1 + h2);
    out_triangles[3] = vec3[3](center - w1 + h1, center - w2 + h1 , center + h1 + h2);
    out_triangles[4] = vec3[3](center + w2 + h1, center + w1 + h1, center + h1 + h2);


    for (uint i = 0; i < 3; ++i) {
        surfOut.wPos = surfInp[i].wPos;
        surfOut.wNorm = surfInp[i].wNorm;
        surfOut.wTangent = surfInp[i].wTangent;
        surfOut.texCoord = surfInp[i].texCoord;

        if (gl_InvocationID>0)
            surfOut.wPos = out_triangles[gl_InvocationID-1][i];

        gl_Position = params.mProjView * vec4(surfOut.wPos, 1.0);
        EmitVertex();
    }

    /*
    vec3[7] out_vertices;
    out_vertices[0] = vec3(center + w1);
    out_vertices[1] = vec3(center - w1);
    out_vertices[2] = vec3(center + w1 + h1);
    out_vertices[3] = vec3(center - w1 + h1);
    out_vertices[4] = vec3(center + h1 + h2);
    out_vertices[5] = vec3(center - w2 + h1);
    out_vertices[6] = vec3(center + w2 + h1);


    for (uint i = 0; i < 7; ++i) {
        surfOut.wPos = surfInp[i].wPos;
        surfOut.wNorm = surfInp[i].wNorm;
        surfOut.wTangent = surfInp[i].wTangent;
        surfOut.texCoord = surfInp[i].texCoord;

        if (gl_InvocationID>0)
            surfOut.wPos = out_vertices[i];

        gl_Position = params.mProjView * vec4(surfOut.wPos, 1.0);
        EmitVertex();
    }
    */
    EndPrimitive();
}
