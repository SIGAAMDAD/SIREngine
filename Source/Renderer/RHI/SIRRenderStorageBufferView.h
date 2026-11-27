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

#ifndef __SIR_RENDER_STORAGE_BUFFER_VIEW_H__
#define __SIR_RENDER_STORAGE_BUFFER_VIEW_H__

#include "Renderer/RHI/SIRRenderTypes.h"
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderResourceView.h>
#include <Renderer/RHI/SIRRenderStorageBuffer.h>
#include <Resources/SIRResourcePtr.h>

// Manages a suballocation of a SIRRenderStorageBuffer
class SIRRenderStorageBufferView : public SIRRenderResourceView
{
public:
	explicit SIRRenderStorageBufferView( const SIRRenderStorageBufferView& ) = delete;
	explicit SIRRenderStorageBufferView( SIRRenderStorageBufferView&& ) = delete;
	virtual ~SIRRenderStorageBufferView() noexcept = default;

	SIRRenderStorageBufferView& operator=( const SIRRenderStorageBufferView& ) = delete;
	SIRRenderStorageBufferView& operator=( SIRRenderStorageBufferView&& ) = delete;

	const SIRRenderStorageBuffer& getBuffer( void ) const noexcept
	{
		return *m_pBuffer;
	}

	uint64_t getOffset( void ) const noexcept
	{
		return m_nOffset;
	}

	uint64_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint32_t getStride( void ) const noexcept
	{
		return m_nStride;
	}

	uint64_t getUsage( void ) const noexcept
	{
		return m_nUsage;
	}

	const SIRRenderResource& getResource( void ) const noexcept override
	{
		return *m_pBuffer;
	}

protected:
	explicit SIRRenderStorageBufferView( const SIRSharedPtr<SIRRenderDevice>& device,
	                                     const SIRResourcePtr<SIRRenderStorageBuffer>& buffer,
	                                     const SIRRenderBufferRange& description ) noexcept
	    : SIRRenderResourceView( device, description.eFormat ),
	      m_pBuffer( buffer ),
	      m_nOffset( description.bufferRange.nOffsetBytes ),
	      m_nSize( description.bufferRange.nSizeBytes ),
	      m_nStride( description.nStrideBytes ),
	      m_nUsage( description.nUsageMask )
	{
	}

	SIRSharedPtr<SIRRenderStorageBuffer> m_pBuffer;
	uint64_t m_nOffset{};
	uint64_t m_nSize{};
	uint32_t m_nStride{};
	uint64_t m_nUsage{};
};
#endif // __SIR_RENDER_STORAGE_BUFFER_VIEW_H__
