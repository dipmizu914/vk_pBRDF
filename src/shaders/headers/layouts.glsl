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

#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL 1


// C++ shared structures and binding
#include "binding.glsl"
#include "struct.glsl"


// Including structs used by the layouts
#include "globals.glsl"


// Sun & Sky structure
#include "sun_and_sky.h"

//----------------------------------------------
// Descriptor Set Layout
//----------------------------------------------
layout(set = S_SCENE_UNIFORM, binding = B_SCENE) uniform _SceneUniforms{ SceneUniforms uniforms; };

// clang-format off
layout(set = S_SCENE_BUFFERS, binding = B_ACCELERATION_STRUCTURE) uniform accelerationStructureEXT topLevelAS;
layout(set = S_SCENE_BUFFERS, binding = B_PLIM_LOOK_UP) readonly buffer _InstanceInfo {RtPrimitiveLookup primInfo[];};
layout(set = S_SCENE_BUFFERS, binding = B_VERTICES) readonly buffer _VertexBuf {float vertices[];};
layout(set = S_SCENE_BUFFERS, binding = B_INDICES) readonly buffer _Indices {uint indices[];};
layout(set = S_SCENE_BUFFERS, binding = B_NORMALS) readonly buffer _NormalBuf {float normals[];};
layout(set = S_SCENE_BUFFERS, binding = B_TEXCOORDS) readonly buffer _TexCoordBuf {float texcoord0[];};
layout(set = S_SCENE_BUFFERS, binding = B_TANGENTS) readonly buffer _TangentBuf {float tangents[];};
layout(set = S_SCENE_BUFFERS, binding = B_COLORS) readonly buffer _ColorBuf {float colors[];};
layout(set = S_SCENE_BUFFERS, binding = B_MATERIALS) readonly buffer _MaterialBuffer {GltfShadeMaterial materials[];};
layout(set = S_SCENE_BUFFERS, binding = B_TEXTURES) uniform sampler2D texturesMap[]; // all textures
layout(set = S_SCENE_BUFFERS, binding = B_MATRICES) buffer _Matrices { InstanceMatrices matrices[]; };
layout(set = S_SCENE_BUFFERS, binding = B_TRIANGLE_LIGHT) readonly buffer _TriangleLights { triangleLight triangleLights[]; };
layout(set = S_SCENE_BUFFERS, binding = B_ALIAS_TABLE) readonly buffer _AliasTable {
	aliasTableCell aliasCol[]; };
layout(set = S_SCENE_BUFFERS, binding = B_COLOR_LAMP) uniform sampler2D colorLamp; // all textures

// The folowing extension allow to pass images as function parameters
#extension GL_EXT_shader_image_load_formatted : enable
layout(set = S_RESULT_IMAGES, binding = B_RAYTRACE_RESULT) uniform sampler2D rayTraceResult;
layout(set = S_RESULT_IMAGES, binding = B_RAYTRACE_STORAGE) uniform image2D resultImage;
layout(set = S_RESULT_IMAGES, binding = B_STORKS_STATUS) uniform sampler2D storksResult;
layout(set = S_RESULT_IMAGES, binding = B_STORKS_STATUS_STORAGE) uniform image2D storksImage;
layout(set = S_RESULT_IMAGES, binding = B_PATHTRACE_RESULT) uniform sampler2D pathTraceResult;
layout(set = S_RESULT_IMAGES, binding = B_PATHTRACE_STORAGE) uniform image2D pathTraceImage;

layout(set = S_ENV, binding = B_SUNANDSKY) uniform _SunAndSkyBuffer {SunAndSky _sunAndSky;};
layout(set = S_ENV, binding = B_HDR) uniform sampler2D environmentTexture;
layout(set = S_ENV, binding = B_IMPORT_SMPL) uniform sampler2D environmentSamplingData;


// clang-format on


#endif  // LAYOUTS_GLSL
