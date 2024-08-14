#ifndef __VALDEN_NODE_VARIABLE_H__
#define __VALDEN_NODE_VARIABLE_H__

#pragma once

#include <Engine/Memory/Memory.h>
#include <Engine/Core/Types.h>
#include <Engine/Core/MathLib/Types.h>
#include <EASTL/fixed_string.h>
#include "NodeObject.h"
#include <imgui-node-editor/imgui_node_editor.h>

namespace Valden {
	typedef struct NodeClass NodeClass_t;
	typedef struct NodeFunction NodeFunction_t;

	enum class EVariableType {
		Boolean,
		Float,
		Float64,
		Int,
		Int64,
		UInt,
		UInt64,
		Reference,

		Vec2,
		Vec3,
		Vec4,

		CvarInt,
		CvarUInt,
		
		FuncPtr,
		Object,
		Text,
	};

	inline const char *VariableTypeStrings[] = {
		"Boolean",
		"Float",
		"Float64",
		"Int",
		"Int64",
		"UInt",
		"UInt64",
		"Reference",

		"Vec2",
		"Vec3",
		"Vec4",

		"CvarInt",
		"CvarUInt",
		
		"FuncPtr",
		"Object",
		"Text",
	};

	class CNodeVariable
	{
	public:
		CNodeVariable( const char *pName, EVariableType nType )
			: m_nType( nType )
		{
			data.name = pName;
			data.nType = ENodeObjectType::Variable;
		}
		~CNodeVariable()
		{ }

		inline void SetID( ax::NodeEditor::NodeId id )
		{ m_nNodeID = id; }
		inline void SetType( EVariableType nType )
		{ m_nType = nType; }

		inline ax::NodeEditor::NodeId GetID( void ) const
		{ return m_nNodeID; }

		inline int32_t GetValueInt( void ) const
		{ return Value.i32; }
		inline int64_t GetValueI64( void ) const
		{ return Value.i64; }
		inline float GetValueFloat( void ) const
		{ return Value.f; }
		inline uint32_t GetValueUInt( void ) const
		{ return Value.u32; }
		inline uint64_t GetValueU64( void ) const
		{ return Value.u64; }
		inline bool32 GetValueBoolean( void ) const
		{ return Value.b; }

		inline const ScriptName_t& GetName( void ) const
		{ return data.name; }
		inline EVariableType GetType( void ) const
		{ return m_nType; }
	private:
		NodeObject data;
		EVariableType m_nType;
		
		ax::NodeEditor::NodeId m_nNodeID;

		union {
			bool32 b;
			float f;
			int32_t i32;
			int64_t i64;
			uint32_t u32;
			uint64_t u64;
			void *ref;
			CVec2 f2;
			CVec3 f3;
			CVec4 f4;
			NodeFunction_t *pFuncPtr;
			NodeClass_t *pObject;
		} Value;
		eastl::string text;
	};

	typedef CNodeVariable NodeVariable_t;
};

#endif