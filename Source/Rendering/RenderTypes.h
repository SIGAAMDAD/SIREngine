#ifndef __SIRENGINE_RENDER_TYPES_H__
#define __SIRENGINE_RENDER_TYPES_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>
#include <glm/glm.hpp>

namespace SIREngine::Rendering
{
	enum class EBufferMapAccess : uint32_t
	{
		WriteOnly,
		ReadOnly,

		Count
	};

	enum class EVertexAttributeClass : uint32_t
	{
		Position,
		Color
	};
	
	enum class EVertexAttributeType : uint32_t
	{
		Float,
		Float2,
		Float3,
		Float4,

		Int,
		Int2,
		Int3,
		Int4,

		UInt,
		UInt2,
		UInt3,
		UInt4,

		Byte,
		ByteNorm,
		Byte2,
		Byte2Norm,

		Byte4,
		Byte4Norm,
		Short,
		ShortNorm,

		Half,
		Half2,
		Half3,
		Half4,

		VertexAttribute_Count
	};

	typedef struct vertexAttribute_t
	{
		uint32_t nLocation;
		EVertexAttributeType eType;
		uint32_t nOffset;
		bool bNormalized;
	} SVertexAttribute;

	typedef struct vertexBinding_t
	{
		uint32_t nBinding;
		uint32_t nStride;
		uint32_t nDivisor;
	} SVertexBinding;

	typedef struct vertex_t
	{
		glm::vec2 Position;
		glm::vec3 Color;
	} SVertex;
};

#endif