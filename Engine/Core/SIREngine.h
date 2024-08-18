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

#include "Application/GenericPlatform/GenericApplication.h"
#include "MathLib/Types.h"

#define SIRENGINE_MAKE_VERSION( major, minor, patch ) \
	((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

#define SIRENGINE_VERSION_MAJOR 2
#define SIRENGINE_VERSION_MINOR 0
#define SIRENGINE_VERSION_PATCH 0
#define SIRENGINE_VERSION SIRENGINE_MAKE_VERSION( SIRENGINE_VERSION_MAJOR, SIRENGINE_VERSION_MINOR, SIRENGINE_VERSION_PATCH )
#define SIRENGINE_VERSION_STRING "SIR Engine v" SIRENGINE_XSTRING( SIRENGINE_VERSION_MAJOR ) "." SIRENGINE_XSTRING( SIRENGINE_VERSION_MINOR ) "." SIRENGINE_XSTRING( SIRENGINE_VERSION_PATCH ) ""

//#define SIRENGINE_DEPRECATED( version, str )

#endif