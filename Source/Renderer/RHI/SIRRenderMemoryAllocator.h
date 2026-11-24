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

#ifndef __SIR_RENDERMEMORYALLOCATOR_H__
#define __SIR_RENDERMEMORYALLOCATOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Device-owned policy/requirements interface; ONLY the native device creates
// and destroys allocations. No standalone allocator can create GPU backings.
// Implementations report API-specific requirements without exposing SDK types.
class SIRRenderMemoryAllocator
{
public:
	virtual SIRGraphicsResult queryBufferRequirements( const sirGraphicsBufferDesc_t& desc,
	                                                     sirRenderMemoryRequirements_t *pInfo ) const noexcept = 0;
	virtual SIRGraphicsResult queryTextureRequirements( const sirGraphicsTextureDesc_t& desc,
	                                                      sirRenderMemoryRequirements_t *pInfo ) const noexcept = 0;
	virtual SIRGraphicsResult queryResourceResidency( sirGraphicsHandle_t hResource, sirRenderMemoryStatus_t *pInfo ) const noexcept = 0;
	virtual SIRGraphicsResult queryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept = 0;

protected:
	virtual ~SIRRenderMemoryAllocator( void ) noexcept = default;
};

#endif // __SIR_RENDERMEMORYALLOCATOR_H__
