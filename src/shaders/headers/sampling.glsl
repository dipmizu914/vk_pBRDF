/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

#include "random.glsl"
 //-------------------------------------------------------------------------------------------------
 // Environment Sampling
 // See:  https://arxiv.org/pdf/1901.05423.pdf
 //-------------------------------------------------------------------------------------------------

struct Environment_sample_data
{
    uint  alias;
    float q;
    float pdf;
};

Environment_sample_data GetSampleData(sampler2D sample_buffer, ivec2 idx)
{
    vec3 data = texelFetch(sample_buffer, idx, 0).xyz;

    Environment_sample_data sample_data;
    sample_data.alias = floatBitsToInt(data.x);
    sample_data.q = data.y;
    sample_data.pdf = data.z;
    return sample_data;
}

Environment_sample_data GetSampleData(sampler2D sample_buffer, uint idx)
{
    uvec2 size = textureSize(sample_buffer, 0);
    uint  px = idx % size.x;
    uint  py = idx / size.x;
    return GetSampleData(sample_buffer, ivec2(px, size.y - py - 1));  // Image is upside down
}


vec3 Environment_sample(sampler2D lat_long_tex, sampler2D sample_buffer, in vec3 randVal, out vec3 to_light, out float pdf)
{
    vec3 xi = randVal;

    uvec2 tsize = textureSize(lat_long_tex, 0);
    uint  width = tsize.x;
    uint  height = tsize.y;

    const uint size = width * height;
    const uint idx = min(uint(xi.x * float(size)), size - 1);

    Environment_sample_data sample_data = GetSampleData(sample_buffer, idx);

    uint env_idx;
    if (xi.y < sample_data.q)
    {
        env_idx = idx;
        xi.y /= sample_data.q;
    }
    else
    {
        env_idx = sample_data.alias;
        xi.y = (xi.y - sample_data.q) / (1.0f - sample_data.q);
    }

    uint       py = env_idx / width;
    const uint px = env_idx % width;
    pdf = GetSampleData(sample_buffer, env_idx).pdf;
    py = height - py - 1;  // Image is upside down


    // uniformly sample spherical area of pixel
    const float u = float(px + xi.y) / float(width);
    const float phi = u * (2.0f * M_PI) - M_PI;
    float       sin_phi = sin(phi);
    float       cos_phi = cos(phi);

    const float step_theta = M_PI / float(height);
    const float theta0 = float(py) * step_theta;
    const float cos_theta = cos(theta0) * (1.0f - xi.z) + cos(theta0 + step_theta) * xi.z;
    const float theta = acos(cos_theta);
    const float sin_theta = sin(theta);
    to_light = vec3(cos_phi * sin_theta, -cos_theta, sin_phi * sin_theta);

    // lookup filtered value
    const float v = theta * M_1_PI;
    return texture(lat_long_tex, vec2(u, v)).xyz;
}


//-------------------------------------------------------------------------------------------------
// Sampling
//-------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------
// Randomly sampling around +Z
vec3 CosineSampleHemisphere(float u1, float u2)
{
    vec3  dir;
    float r = sqrt(u1);
    float phi = 2.0 * M_PI * u2;
    dir.x = r * cos(phi);
    dir.y = r * sin(phi);
    dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));

    return dir;
}

// Sampling hemiphere around z, with basis tangent (x) and bitangent (y)
vec3 SamplingHemisphere(vec2 randVal, in vec3 x, in vec3 y, in vec3 z)
{
    float r1 = randVal.x;
    float r2 = randVal.y;
    vec3  direction = CosineSampleHemisphere(r1, r2);
    return direction.x * x + direction.y * y + direction.z * z;
}


// Return the tangent and binormal from the incoming normal
void CreateCoordinateSystem(in vec3 N, out vec3 Nt, out vec3 Nb)
{
    if (abs(N.x) > abs(N.y))
        Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
    else
        Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
    Nb = cross(N, Nt);
}


// Return the UV in a lat-long HDR map
vec2 GetSphericalUv(vec3 v)
{
    float gamma = asin(-v.y);
    float theta = atan(v.z, v.x);

    vec2 uv = vec2(theta * M_1_PI * 0.5, gamma * M_1_PI) + 0.5;
    return uv;
}


//-------------------------------------------------------------------------------------------------
// Avoiding self intersections (see Ray Tracing Gems, Ch. 6)
//
vec3 OffsetRay(in vec3 p, in vec3 n)
{
    const float intScale = 256.0f;
    const float floatScale = 1.0f / 65536.0f;
    const float origin = 1.0f / 32.0f;

    ivec3 of_i = ivec3(intScale * n.x, intScale * n.y, intScale * n.z);

    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return vec3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,  //
        abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,  //
        abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

vec3 projectToPlane(vec3 a, vec3 n) {
    return normalize(a - dot(a, n) * n);
}

void aliasTableSample(float r1, float r2, out uint index, out float probability) {
    uint selected_column = min(uint(uniforms.aliasTableCount * r1), uniforms.aliasTableCount - 1);
    aliasTableCell col = aliasCol[selected_column];
    if (col.prob > r2) {
        index = selected_column;
        probability = col.pdf;
    }
    else {
        index = col.alias;
        probability = col.aliasPdf;
    }

}

vec3 getTrianglePoint(float r1, float r2, vec3 p1, vec3 p2, vec3 p3) {
    float sqrt_r1 = sqrt(r1);
    return (1.0 - sqrt_r1) * p1 + (sqrt_r1 * (1.0 - r2)) * p2 + (r2 * sqrt_r1) * p3;
}

vec3 getTriangleNormal(float r1, float r2, vec3 n1, vec3 n2, vec3 n3, mat4 worldToObject) {
    float sqrt_r1 = sqrt(r1);
    vec3 normal = normalize(n1 * (1.0 - sqrt_r1) + n2 * (sqrt_r1 * (1.0 - r2)) + n3 * (r2 * sqrt_r1));
    vec3 world_normal = normalize(vec3(vec4(normal,0.0) * worldToObject));
    return world_normal;
}
vec3 getTriangleTangent(float r1, float r2, vec3 world_normal,vec3 t1, vec3 t2, vec3 t3, mat4 objectToWorld) {
    float sqrt_r1 = sqrt(r1);
    vec3 tangent = t1 * (1.0 - sqrt_r1) + t2 * (sqrt_r1 * (1.0 - r2)) + t3 * (r2 * sqrt_r1);
    vec3 world_tangent = normalize(vec3(mat4(objectToWorld) * vec4(tangent, 0)));
    world_tangent = normalize(world_tangent - dot(world_tangent, world_normal) * world_normal);
    return world_tangent;
}
void SceneSample(inout uint seed, out vec3 lightSamplePos, out vec4 lightNormal,out vec3 lightTangent ,out vec4 lightEmission, out float lightSamplePdf) {
    uint selected_idx;
    aliasTableSample(rnd(seed), rnd(seed), selected_idx, lightSamplePdf);

    triangleLight light = triangleLights[selected_idx];

    float r1 = rnd(seed);
    float r2 = rnd(seed);
    lightSamplePos = getTrianglePoint(r1,r2, light.p1.xyz, light.p2.xyz, light.p3.xyz);
    lightEmission = light.emission_luminance;

    vec3 normal = light.normalArea.xyz;
   // lightSamplePdf /= abs(dot(wi, normal)) * light.normalArea.w;
    lightNormal = vec4(normal, 1.0f);

    vec3 world_normal= getTriangleNormal(r1, r2, light.n1.xyz, light.n2.xyz, light.n3.xyz, light.worldToObject);

    lightTangent = getTriangleTangent(r1, r2, world_normal, light.t1.xyz, light.t2.xyz, light.t3.xyz, light.objectToWorld);
}

//-------------------------------------------------------------------------------------------------
// Special sampling for compute shader
// This is the way sampling is done, which can improve texture access
//
#ifdef USE_COMPUTE__
layout(local_size_x = 32, local_size_y = 2) in;
#extension GL_EXT_shader_8bit_storage : enable  // Using uint_8 ...
ivec2 SampleSizzled()
{
    // Sampling Swizzling
    // Convert 32x2 to 8x8, where the sampling will follow how invocation are done in a subgroup.
    // layout(local_size_x = 32, local_size_y = 2) in;
    ivec2 base = ivec2(gl_WorkGroupID.xy) * 8;
    ivec2 subset = ivec2(int(gl_LocalInvocationID.x) & 1, int(gl_LocalInvocationID.x) / 2);
    subset += gl_LocalInvocationID.x >= 16 ? ivec2(2, -8) : ivec2(0, 0);
    subset += ivec2(gl_LocalInvocationID.y * 4, 0);
    return base + subset;
}
#elif defined(USE_COMPUTE)
#extension GL_EXT_shader_8bit_storage : enable

layout(local_size_x = 8, local_size_y = 8) in;
ivec2 SampleSizzled()
{
    return ivec2(gl_GlobalInvocationID.xy);
}
#endif


#endif  // SAMPLING_GLSL
