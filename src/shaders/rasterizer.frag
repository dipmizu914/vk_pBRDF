#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "headers/globals.glsl"
#include "headers/layouts.glsl"

#include "headers/binding.glsl"
#include "headers/struct.glsl"


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outColor;



vec3 tonemapping(vec3 col, float gamma) {
	return pow(col, vec3(1.0f / gamma));
}

void main() {
	ivec2 coordImage = ivec2(gl_FragCoord.xy);
	outColor = vec3(1.0);

//	switch(uniforms.debugging_mode)
//    {
//      case eMetallic:
//        outColor = vec3(bsdfMat.metallic);
//        return;
//      case eNormal:
//        outColor = (sstate.normal + vec3(1)) * .5;
//        return;
//      case eBaseColor:
//        outColor = bsdfMat.albedo.xyz;
//        return;
//      case eEmissive:
//        outColor = emissive;
//        return;
//      case eAlpha:
//        outColor = vec3(bsdfMat.albedo.a);
//        return;
//      case eRoughness:
//        outColor = vec3(bsdfMat.roughness);
//        return;
//      case eTextcoord:
//        outColor = vec3(sstate.text_coords[0], 1);
//        return;
//      case eTangent:
//        outColor = vec3(sstate.tangent_u[0].xyz + vec3(1)) * .5;
//        return;
//      case eRadiance:
//        outColor = radiance;
//        return;
//      case eWeight:
//        outColor = throughput;
//        return;
//      case eRayDir:
//        outColor = (bsdfDir + vec3(1)) * .5;
//        return;
//    };
}
