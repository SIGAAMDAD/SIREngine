#ifndef __VALDEN_NODE_OBJECT_H__
#define __VALDEN_NODE_OBJECT_H__

#pragma once

#include <Engine/Memory/Memory.h>

namespace Valden {
	using ScriptName_t = eastl::string;
	using ScriptInteger_t = int32_t;
	using ScriptInteger64_t = int64_t;
	using ScriptUInteger_t = uint32_t;
	using ScriptUInteger64_t = uint64_t;
	using ScriptFloat_t = float;
	using ScriptFloat64_t = double;
	using ScriptBoolean_t = bool32;
	using ScriptString_t = eastl::string;

	enum class ENodeObjectType {
		Method,
		Variable,
		Class,
		Comment,
	};

	struct NodeObject {
		ScriptName_t name;
		ENodeObjectType nType;
	};
};

#endif