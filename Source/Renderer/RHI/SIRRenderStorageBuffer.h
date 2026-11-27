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

#ifndef __SIR_RENDER_STORAGE_BUFFER_H__
#define __SIR_RENDER_STORAGE_BUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>
#include <Core/Containers/SIRSpan.h>

class SIRRenderStorageBuffer : public SIRRenderResource
{
public:
	explicit SIRRenderStorageBuffer( const SIRRenderStorageBuffer& ) = delete;
	explicit SIRRenderStorageBuffer( SIRRenderStorageBuffer&& ) = delete;
	virtual ~SIRRenderStorageBuffer() noexcept = default;
	SIRRenderStorageBuffer& operator=( const SIRRenderStorageBuffer& ) = delete;
	SIRRenderStorageBuffer& operator=( SIRRenderStorageBuffer&& ) = delete;

	uint64_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint64_t getCommittedBytes( void ) const noexcept
	{
		return m_nCommittedBytes;
	}

	uint64_t getUsage( void ) const noexcept
	{
		return m_nUsage;
	}

	SIRRenderMemoryClass getMemoryType( void ) const noexcept
	{
		return m_eMemoryType;
	}

	bool isHostVisible( void ) const noexcept
	{
		return m_bHostVisible;
	}

	bool isHostCoherent( void ) const noexcept
	{
		return m_bHostCoherent;
	}

	bool isPersistentlyMapped( void ) const noexcept
	{
		return m_bPersistentlyMapped;
	}

	// advanced pipeline features
	virtual SIRRenderResult map( uint64_t nOffsetBytes, uint64_t nSizeBytes, SIRRenderMappedBuffer *pMapping ) noexcept = 0;
	virtual SIRRenderResult unmap( void ) noexcept = 0;
	virtual SIRRenderResult flush( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept = 0;
	virtual SIRRenderResult invalidate( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept = 0;
	virtual uint64_t getDeviceAddress( void ) const noexcept = 0;

	virtual SIRRenderResult updateBufferRange( uint64_t nOffsetBytes, uint64_t nSizeBytes, const SIRSpan<uint8_t>& memory ) noexcept = 0;
	virtual SIRRenderResult copyBufferRange( SIRRenderStorageBuffer& copyTarget, uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept = 0;
	virtual SIRRenderResult clearBufferRange( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept = 0;
	virtual SIRRenderResult clear( void ) noexcept = 0;

	virtual SIRRenderResult bind( void ) const noexcept = 0;
	virtual SIRRenderResult unbind( void ) const noexcept = 0;

protected:
	explicit SIRRenderStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderBufferDesc& desc ) noexcept
	    : SIRRenderResource( device ),
	      m_nSize( desc.nSizeBytes ),
	      m_nCommittedBytes( 0 ),
	      m_eUsage( desc.eUsageMask ),
	      m_eMemoryType( desc.eMemory ),
	      m_bHostVisible( ( desc.nRequiredMemoryProperties & static_cast<uint32_t>( SIRRenderMemoryProperty::HostVisible ) ) != 0 ),
	      m_bHostCoherent( ( desc.nRequiredMemoryProperties & static_cast<uint32_t>( SIRRenderMemoryProperty::HostCoherent ) ) != 0 ),
	      m_bPersistentlyMapped( ( desc.nRequiredMemoryProperties & static_cast<uint32_t>( SIRRenderMemoryProperty::PersistentMapped ) ) != 0 )
	{
	}

	uint64_t m_nSize{};
	uint64_t m_nCommittedBytes{};
	SIRRenderBufferUsage m_eUsage{};
	SIRRenderMemoryClass m_eMemoryType{};

	bool m_bHostVisible{};
	bool m_bHostCoherent{};
	bool m_bPersistentlyMapped{};
};

#endif // __SIR_RENDER_STORAGE_BUFFER_H__
