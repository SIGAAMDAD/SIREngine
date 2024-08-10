#ifndef __SIRENGINE_H__
#define __SIRENGINE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include "stb_sprintf.h"

#include <Engine/Core/Compiler.h>
#include <Engine/Core/Config.h>
#include <Engine/Core/Platform.h>
#include <Engine/Core/Types.h>

#define SIRENGINE_REPLACE_NEW_AND_DELETE
#include <Engine/Memory/Memory.h>

#include <type_traits>
#include <forward_list>
#include <EASTL/type_traits.h>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/unordered_map.h>
#include <EASTL/map.h>
#include <EASTL/algorithm.h>
#include <EASTL/sort.h>

#include "Logging/Logger.h"

#include <Engine/Util/CReverseIterator.h>
#include <Engine/Util/CString.h>
#include <Engine/Util/CVector.h>
#include <Engine/Util/CStaticArray.h>
#include <Engine/Util/CConstList.h>

#include <Engine/Memory/Backend/TagArenaAllocator.h>

#include <Engine/Core/Util.h>

#include <Engine/Core/ResourceDef.h>
#include <Engine/Core/ResourceManager.h>

#include "Application/GenericPlatform/GenericApplication.h"

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

#define SIRENGINE_XSTRING_HELPER( x ) #x
#define SIRENGINE_XSTRING( x ) SIRENGINE_XSTRING_HELPER( x )

#define SIRENGINE_MAKE_VERSION( major, minor, patch ) \
    ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

#define SIRENGINE_VERSION_MAJOR 2
#define SIRENGINE_VERSION_MINOR 0
#define SIRENGINE_VERSION_PATCH 0
#define SIRENGINE_VERSION SIRENGINE_MAKE_VERSION( SIRENGINE_VERSION_MAJOR, SIRENGINE_VERSION_MINOR, SIRENGINE_VERSION_PATCH )
#define SIRENGINE_VERSION_STRING "SIR Engine v" SIRENGINE_XSTRING( SIRENGINE_VERSION_MAJOR ) "." SIRENGINE_XSTRING( SIRENGINE_VERSION_MINOR ) "." SIRENGINE_XSTRING( SIRENGINE_VERSION_PATCH ) ""

#endif