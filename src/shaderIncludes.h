#pragma once
#include <nvmath/nvmath.h>
#include <nvmath/nvmath_glsltypes.h>

namespace shader
{
#include "shaders/headers/struct.glsl"

//#define uint ::std::uint32_t
//#define vec2 ::nvmath::vec2f
//#define vec4 ::nvmath::vec4f
//#define ivec2 ::nvmath::ivec2
//#define ivec4 ::nvmath::ivec4
//#define uvec2 ::nvmath::uvec2
//#define uvec4 ::nvmath::uvec4
//#define mat4 ::nvmath::mat4f

#define CPP_FUNCTION inline

//#include "shaders/headers/struct.glsl"
//#include "shaders/headers/common.glsl"
#include "shaders/headers/binding.glsl"
//#include "shaders/structs/restirStructs.glsl"
//#include "shaders/structs/sceneStructs.glsl"
//#include "shaders/structs/light.glsl"

//#undef uint
//#undef vec2
//#undef vec4
//#undef ivec2
//#undef ivec4
//#undef uvec2
//#undef uvec4
//#undef mat4

#undef CPP_FUNCTION

	struct PushConstant
	{
		int frame{0};
		int initialize{1};
		float gamma{ 2.2 };
	};
} // namespace shader
