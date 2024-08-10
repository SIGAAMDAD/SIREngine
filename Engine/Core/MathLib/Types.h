#ifndef __SIRENGINE_MATHLIB_TYPES_H__
#define __SIRENGINE_MATHLIB_TYPES_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../Config.h"

#if defined(SIRENGINE_USE_GLM_MATH)
	#include <glm/glm.hpp>

	using CVec2 = glm::vec2;
	using CVec3 = glm::vec3;
	using CVec4 = glm::vec4;
	using CUVec2 = glm::uvec2;
	using CUVec3 = glm::uvec3;
	using CUVec4 = glm::uvec4;
	using CIVec2 = glm::ivec2;
	using CIVec3 = glm::ivec3;
	using CIVec4 = glm::ivec4;
#else
	#include "MathLib/MathLib.h"
#endif

#endif