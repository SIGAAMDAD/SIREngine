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

#ifndef __SIR_RENDER_MEMORY_ALLOCATOR_H__
#define __SIR_RENDER_MEMORY_ALLOCATOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderTypes.h>
class SIRRenderDevice;

class SIRRenderMemoryAllocator
{
public:
	explicit SIRRenderMemoryAllocator( const SIRRenderMemoryAllocator& ) = delete;
	explicit SIRRenderMemoryAllocator( SIRRenderMemoryAllocator&& ) = delete;
	virtual ~SIRRenderMemoryAllocator() noexcept = default;
	SIRRenderMemoryAllocator& operator=( const SIRRenderMemoryAllocator& ) = delete;
	SIRRenderMemoryAllocator& operator=( SIRRenderMemoryAllocator&& ) = delete;

	SIRRenderDevice& getDevice( void ) const noexcept
	{
		return *m_pDevice;
	}

	virtual SIRRenderResult getBufferRequirements( const sirRenderBufferDescription_t& description, sirRenderMemoryRequirements_t *pRequirements ) const noexcept = 0;
	virtual SIRRenderResult getTextureRequirements( const sirRenderTextureDescription_t& description, sirRenderMemoryRequirements_t *pRequirements ) const noexcept = 0;
	virtual SIRRenderResult getRenderbufferRequirements( const sirRenderTextureDescription_t& description, sirRenderMemoryRequirements_t *pRequirements ) const noexcept = 0;
	virtual SIRRenderResult getAccelerationStructureRequirements( uint64_t nSizeBytes, sirRenderMemoryRequirements_t *pRequirements ) const noexcept = 0;
	virtual SIRRenderResult getMemoryBudget( sirRenderMemoryBudget_t *pBudget ) const noexcept = 0;

protected:
	explicit SIRRenderMemoryAllocator( SIRRenderDevice& device ) noexcept
	    : m_pDevice( &device )
	{
	}

private:
	SIRRenderDevice *m_pDevice;
};
#endif // __SIR_RENDER_MEMORY_ALLOCATOR_H__
