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

#include <Renderer/Drivers/OpenGL/SIRGLHandlePool.h>
#include <limits.h>
#include <string.h>

SIRGraphicsResult sirGLHandlePoolInit( sirGLHandlePool_t *pPool, const sirGraphicsABIHost_t *pHost, uint32_t nDeviceGeneration,
                                         uint32_t nCapacity ) noexcept
{
	if ( pPool == nullptr || pHost == nullptr || pHost->pAllocate == nullptr || pHost->pFree == nullptr || nDeviceGeneration == 0 ||
	     nCapacity == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPool->pSlots != nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( nCapacity > UINT32_MAX / sizeof( sirGLHandleSlot_t ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirGLHandleSlot_t *pSlots = static_cast<sirGLHandleSlot_t *>( pHost->pAllocate(
	    pHost->pContext, sizeof( sirGLHandleSlot_t ) * static_cast<uint64_t>(nCapacity), alignof( sirGLHandleSlot_t ) );
	if ( pSlots == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	memset( pSlots, 0, sizeof( sirGLHandleSlot_t ) * static_cast<uint64_t>(nCapacity) );
	for ( uint32_t i = 0; i < nCapacity; ++i ) {
		pSlots[i].nGeneration = 1;
		pSlots[i].nNextFree = i + 1 < nCapacity ? i + 1 : UINT32_MAX;
	}
	pPool->host = *pHost;
	pPool->pSlots = pSlots;
	pPool->nCapacity = nCapacity;
	pPool->nActive = 0;
	pPool->nFreeHead = 0;
	pPool->nDeviceGeneration = nDeviceGeneration;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLHandlePoolInsert( sirGLHandlePool_t *pPool, const sirGLNativeResource_t *pResource,
                                           sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pPool == nullptr || pPool->pSlots == nullptr || pResource == nullptr || pHandle == nullptr || pResource->nName == 0 ||
	     pResource->eType <= SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_NONE || pResource->eType > SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SURFACE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPool->nFreeHead == UINT32_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pResource->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW ) {
		sirGLNativeResource_t parent {};
		const SIRGraphicsResult eParent = sirGLHandlePoolFind( pPool, pResource->hParent, &parent );
		if ( eParent != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eParent;
		}
		if ( parent.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pPool->pSlots[pResource->hParent.nSlot].nChildCount == UINT32_MAX ) {
			return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
	} else if ( pResource->hParent.nGeneration != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nSlot = pPool->nFreeHead;
	sirGLHandleSlot_t *pSlot = &pPool->pSlots[nSlot];
	pPool->nFreeHead = pSlot->nNextFree;
	pSlot->resource = *pResource;
	pSlot->nChildCount = 0;
	pSlot->bLive = true;
	if ( pResource->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW ) {
		++pPool->pSlots[pResource->hParent.nSlot].nChildCount;
	}
	++pPool->nActive;
	*pHandle = { nSlot, pSlot->nGeneration, pPool->nDeviceGeneration, pResource->eType };
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLHandlePoolFind( const sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle,
                                         sirGLNativeResource_t *pResource ) noexcept
{
	if ( pPool == nullptr || pPool->pSlots == nullptr || pResource == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( hHandle.nDeviceGeneration != pPool->nDeviceGeneration || hHandle.nSlot >= pPool->nCapacity || hHandle.nGeneration == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGLHandleSlot_t *pSlot = &pPool->pSlots[hHandle.nSlot];
	if ( !pSlot->bLive || pSlot->nGeneration != hHandle.nGeneration || pSlot->resource.eType != hHandle.eType ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	*pResource = pSlot->resource;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLHandlePoolTake( sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle, sirGLNativeResource_t *pResource ) noexcept
{
	const SIRGraphicsResult eStatus = sirGLHandlePoolFind( pPool, hHandle, pResource );
	if ( eStatus != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eStatus;
	}
	sirGLHandleSlot_t *pSlot = &pPool->pSlots[hHandle.nSlot];
	if ( pSlot->nChildCount != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( pSlot->resource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW ) {
		sirGLNativeResource_t parent {};
		const SIRGraphicsResult eParent = sirGLHandlePoolFind( pPool, pSlot->resource.hParent, &parent );
		if ( eParent != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS || pPool->pSlots[pSlot->resource.hParent.nSlot].nChildCount == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		--pPool->pSlots[pSlot->resource.hParent.nSlot].nChildCount;
	}
	pSlot->resource = {};
	pSlot->bLive = false;
	--pPool->nActive;
	// A wrapped generation makes a slot permanently unavailable rather than reviving old handles.
	if ( pSlot->nGeneration == UINT32_MAX ) {
		pSlot->nGeneration = 0;
		pSlot->nNextFree = UINT32_MAX;
	} else {
		++pSlot->nGeneration;
		pSlot->nNextFree = pPool->nFreeHead;
		pPool->nFreeHead = hHandle.nSlot;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLHandlePoolShutdown( sirGLHandlePool_t *pPool ) noexcept
{
	if ( pPool == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPool->pSlots == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	if ( pPool->nActive != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	pPool->host.pFree( pPool->host.pContext, pPool->pSlots );
	*pPool = {};
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
