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

#ifndef __SIR_RENDER_ACCELERATION_STRUCTURE_H__
#define __SIR_RENDER_ACCELERATION_STRUCTURE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderStorageBuffer.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderStorageBuffer;

class SIRRenderAccelerationStructure : public SIRRenderResource
{
public:
	explicit SIRRenderAccelerationStructure( const SIRRenderAccelerationStructure& ) = delete;
	explicit SIRRenderAccelerationStructure( SIRRenderAccelerationStructure&& ) = delete;
	virtual ~SIRRenderAccelerationStructure() noexcept = default;
	SIRRenderAccelerationStructure& operator=( const SIRRenderAccelerationStructure& ) = delete;
	SIRRenderAccelerationStructure& operator=( SIRRenderAccelerationStructure&& ) = delete;

	SIRRenderAccelerationStructureType getType( void ) const noexcept
	{
		return m_eType;
	}

	const SIRRenderStorageBuffer& getBackingBuffer( void ) const noexcept
	{
		return *m_pBackingBuffer;
	}

	uint64_t getBackingOffset( void ) const noexcept
	{
		return m_nBackingOffset;
	}

	uint64_t getBackingSize( void ) const noexcept
	{
		return m_nBackingSize;
	}

	uint32_t getBuildFlags( void ) const noexcept
	{
		return m_nBuildFlags;
	}

	virtual uint64_t getDeviceAddress( void ) const noexcept = 0;

protected:
	explicit SIRRenderAccelerationStructure( const SIRSharedPtr<SIRRenderDevice>& device,
	                                         const SIRSharedPtr<SIRRenderStorageBuffer>& backingBuffer,
	                                         const SIRRenderAccelerationStructureDesc& description ) noexcept
	    : SIRRenderResource( device ),
	      m_eType( description.eType ),
	      m_pBackingBuffer( backingBuffer ),
	      m_nBackingOffset( description.nBackingOffsetBytes ),
	      m_nBackingSize( description.nBackingSizeBytes ),
	      m_nBuildFlags( description.nBuildFlags )
	{
	}

private:
	SIRRenderAccelerationStructureType m_eType{};
	SIRSharedPtr<SIRRenderStorageBuffer> m_pBackingBuffer;
	uint64_t m_nBackingOffset{};
	uint64_t m_nBackingSize{};
	uint32_t m_nBuildFlags{};
};
#endif // __SIR_RENDER_ACCELERATION_STRUCTURE_H__
