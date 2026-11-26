/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_RENDER_SHADER_BINDING_TABLE_H__
#define __SIR_RENDER_SHADER_BINDING_TABLE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>
class SIRRenderStorageBuffer;
class SIRRenderPipeline;

class SIRRenderShaderBindingTable : public SIRRenderResource
{
public:
	explicit SIRRenderShaderBindingTable( const SIRRenderShaderBindingTable& ) = delete;
	explicit SIRRenderShaderBindingTable( SIRRenderShaderBindingTable&& ) = delete;
	virtual ~SIRRenderShaderBindingTable() noexcept = default;
	SIRRenderShaderBindingTable& operator=( const SIRRenderShaderBindingTable& ) = delete;
	SIRRenderShaderBindingTable& operator=( SIRRenderShaderBindingTable&& ) = delete;

	const SIRRenderStorageBuffer& getBuffer( void ) const noexcept
	{
		return *m_pBuffer;
	}

	const SIRRenderPipeline& getPipeline( void ) const noexcept
	{
		return *m_pPipeline;
	}

	sirRenderShaderBindingRegion_t getRayGenerationRange( void ) const noexcept
	{
		return m_RayGeneration;
	}

	sirRenderShaderBindingRegion_t getMissRange( void ) const noexcept
	{
		return m_Miss;
	}

	sirRenderShaderBindingRegion_t getHitRange( void ) const noexcept
	{
		return m_Hit;
	}

	sirRenderShaderBindingRegion_t getCallableRange( void ) const noexcept
	{
		return m_Callable;
	}

protected:
	explicit SIRRenderShaderBindingTable( const SIRSharedPtr<SIRRenderDevice>& device,
	                                      const SIRSharedPtr<SIRRenderStorageBuffer>& buffer,
	                                      const SIRSharedPtr<SIRRenderPipeline>& pipeline,
	                                      const sirRenderShaderBindingTableDescription_t& description ) noexcept
	    : SIRRenderResource( device ),
	      m_pBuffer( buffer ),
	      m_pPipeline( pipeline ),
	      m_RayGeneration( description.rayGeneration ),
	      m_Miss( description.miss ),
	      m_Hit( description.hit ),
	      m_Callable( description.callable )
	{
	}

	SIRSharedPtr<SIRRenderStorageBuffer> m_pBuffer;
	SIRSharedPtr<SIRRenderPipeline> m_pPipeline;
	sirRenderShaderBindingRegion_t m_RayGeneration{};
	sirRenderShaderBindingRegion_t m_Miss{};
	sirRenderShaderBindingRegion_t m_Hit{};
	sirRenderShaderBindingRegion_t m_Callable{};
};
#endif // __SIR_RENDER_SHADER_BINDING_TABLE_H__
