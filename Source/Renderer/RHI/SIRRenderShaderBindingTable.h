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

#ifndef __SIRENGINE_SIRRENDERSHADERBINDINGTABLE_H__
#define __SIRENGINE_SIRRENDERSHADERBINDINGTABLE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Optional ray-tracing shader record layout, backed by one existing GPU
// buffer. No separate CPU/GPU copy is created by this descriptor wrapper.
class SIRRenderShaderBindingTable final
{
public:
	SIRRenderShaderBindingTable( void ) noexcept = default;
	explicit SIRRenderShaderBindingTable( const sirRenderShaderBindingTableDesc_t& desc ) noexcept : m_Desc( desc )
	{
	}

	const sirRenderShaderBindingTableDesc_t& getDesc( void ) const noexcept
	{
		return m_Desc;
	}
	bool isValid( void ) const noexcept
	{
		return m_Desc.hBackingBuffer.nGeneration != 0 && m_Desc.hBackingBuffer.nDeviceGeneration != 0 &&
		       m_Desc.hBackingBuffer.eType == SIR_GRAPHICS_HANDLE_BUFFER && m_Desc.rayGeneration.nSizeBytes != 0;
	}

private:
	sirRenderShaderBindingTableDesc_t m_Desc = {};
};

#endif // __SIRENGINE_SIRRENDERSHADERBINDINGTABLE_H__
