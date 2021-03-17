#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "headers/globals.glsl"
#include "headers/layouts.glsl"

#include "headers/binding.glsl"
#include "headers/struct.glsl"


layout(location = 0) in vec2 inUv;

layout(location = 0) out vec3 outColor;

const float eps=1e-9;

vec3 tonemapping(vec3 col) {
	return pow(uniforms.exposure*col, vec3(1.0f / uniforms.gamma));
}

float atan2(in float y, in float x){
    return x == 0.0 ? sign(y)*M_PI/2 : atan(y, x);
}

void getRenderInfo(out int kind, out vec2 uv){
	vec2 iuv=vec2(inUv.x*2.0f,inUv.y*2.0f);
	uv.x=fract(iuv.x);
	uv.y=fract(iuv.y);

	kind=int(floor(iuv.x)+floor(iuv.y)*2);
}

void main() {
	ivec2 coordImage = ivec2(gl_FragCoord.xy);

	int kind;
	vec2 uv;
	getRenderInfo(kind,uv);

	vec3 raytraceColor = texture(rayTraceResult, uv).rgb;
	vec3 pathtraceColor = texture(pathTraceResult, uv).rgb;
	vec4 storks = texture(storksResult, uv);
    
	bool exist = texture(rayTraceResult, uv).a > 0.5f;


	float DoP=storks.x;
	float ALoP=storks.y;
    vec3 ALoPc=texture(colorLamp, vec2(ALoP,0.5)).rgb;

	float t=storks.z;
	vec3 blue=vec3(0.0,0.0,1.0);
	vec3 yellow=vec3(1.0,1.0,0.0);
	vec3 ToP=blue*t+yellow*(1.0f-t);
	

	raytraceColor=tonemapping(raytraceColor);
	pathtraceColor=tonemapping(pathtraceColor);

	if(!exist){
		DoP=0.0;
		ALoPc=vec3(0.0);
		ToP=vec3(0.0);
	}

	switch(kind){
		case 0:
		outColor=raytraceColor;
		break;
		case 1:
		outColor=vec3(DoP,0.0,0.0);
		break;
		case 2:
		outColor= ALoPc;
		break;
		case 3:
		outColor=ToP;
		break;
	}

	if(uv.x<0.01 || uv.y<0.01){
		outColor=vec3(1.0);
	}

}
