/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRRENDERSHADER_H__
#define __SIRENGINE_SIRRENDERSHADER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// CPU-side immutable shader-stage reference. The canonical cooked artifact
// remains owned by ShaderCooker/ShaderLibrary; this facade does not copy bytes.
// Driver pipeline creation may compile/cache native shader representations.
class SIRRenderShader final
{
public:
	SIRRenderShader( void ) noexcept = default;
	explicit SIRRenderShader( const sirRenderShaderDesc_t& desc ) noexcept : m_Desc( desc )
	{
	}

	const sirRenderShaderDesc_t& getDesc( void ) const noexcept
	{
		return m_Desc;
	}
	bool isValid( void ) const noexcept
	{
		return m_Desc.nStageMask != 0 && m_Desc.code.pBytes != nullptr && m_Desc.code.nByteCount != 0;
	}

private:
	sirRenderShaderDesc_t m_Desc = {};
};

#endif // __SIRENGINE_SIRRENDERSHADER_H__
