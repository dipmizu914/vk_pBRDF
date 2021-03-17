#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require          // This is about ray tracing
#extension GL_EXT_nonuniform_qualifier : enable  // To access unsized descriptor arrays
// Align structure layout to scalar
#extension GL_EXT_scalar_block_layout : enable        // This is about ray tracing

#include "headers/struct.glsl"
// Payload and other structures
#include "headers/globals.glsl"
#include "headers/layouts.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;


layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

layout(push_constant) uniform Constants{
  uint instanceId;
  uint materialId;
} pushC;

void main() {
	mat4 objMatrixIT = transpose(inverse(matrices[pushC.instanceId].object2World));

    vec4 worldPos = matrices[pushC.instanceId].object2World * vec4(inPosition, 1.0f);
	vec4 screenPos=uniforms.projectionViewMatrix* worldPos;

	outPosition = worldPos.xyz;
	outNormal   = vec3(objMatrixIT * vec4(inNormal, 0.0));
	outTexCoord = inTexcoord;
	gl_Position = screenPos;
}
