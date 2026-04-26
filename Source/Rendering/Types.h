#ifndef __SIRENGINE_RENDERING_TYPES_H__
#define __SIRENGINE_RENDERING_TYPES_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

namespace SIREngine::Rendering
{
	enum class EVertexAttributeType
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

		Half,
		Half2,
		Half3,
		Half4,

		Count
	};

	typedef struct SVertexAttribute
	{
		/// <summary>
		/// The offset within the buffer.
		/// </summary>
		uint32_t nOffset;

		/// <summary>
		/// Shader location.
		/// </summary>
		uint32_t nLocation;

		/// <summary>
		/// The attribute's type.
		/// </summary>
		EVertexAttributeType eType;

		/// <summary>
		/// Integer-to-float conversion.
		/// </summary>
		bool bNormalized;
	} vertexAttribute_t;

	typedef struct SVertexBinding
	{
		uint32_t nBinding;
		uint32_t nStride;
		uint32_t nDiviser;
	} vertexBinding_t;
};

#endif