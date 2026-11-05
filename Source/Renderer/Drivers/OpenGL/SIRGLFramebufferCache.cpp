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

#include <Renderer/Drivers/OpenGL/SIRGLFramebufferCache.h>
#include <limits.h>
#include <string.h>

static bool sirGLFramebufferSameHandle( sirGraphicsHandle_t hLeft, sirGraphicsHandle_t hRight ) noexcept
{
	return hLeft.nSlot == hRight.nSlot && hLeft.nGeneration == hRight.nGeneration && hLeft.nDeviceGeneration == hRight.nDeviceGeneration &&
	       hLeft.eType == hRight.eType;
}

static bool sirGLFramebufferSameKey( const sirGLFramebufferCacheKey_t *pLeft, const sirGLFramebufferCacheKey_t *pRight ) noexcept
{
	return sirGLFramebufferSameHandle( pLeft->hColor, pRight->hColor ) && sirGLFramebufferSameHandle( pLeft->hDepth, pRight->hDepth ) &&
	       pLeft->nColorMip == pRight->nColorMip && pLeft->nDepthMip == pRight->nDepthMip;
}

static sirGraphicsResult_t sirGLFramebufferResolveAttachment( const sirGLHandlePool_t *pPool, const sirGLFramebufferCacheKey_t *pKey,
                                                              bool bDepth, sirGLNativeResource_t *pNative ) noexcept
{
	const sirGraphicsHandle_t hResource = bDepth ? pKey->hDepth : pKey->hColor;
	const uint32_t nMip = bDepth ? pKey->nDepthMip : pKey->nColorMip;
	const uint32_t nRequiredUsage = bDepth ? SIR_GRAPHICS_USAGE_DEPTH_STENCIL : SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	if ( hResource.nGeneration == 0 ) {
		if ( nMip != 0 || hResource.nSlot != 0 || hResource.nDeviceGeneration != 0 || hResource.eType != SIR_GRAPHICS_HANDLE_NONE ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pNative = {};
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t eResult = sirGLHandlePoolFind( pPool, hResource, pNative );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( pNative->eType != SIR_GRAPHICS_HANDLE_TEXTURE && pNative->eType != SIR_GRAPHICS_HANDLE_VIEW ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( pNative->nUsage & nRequiredUsage ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pNative->nMipLevels == 0 || nMip >= pNative->nMipLevels ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLFramebufferCacheInit( sirGLFramebufferCache_t *pCache, const sirGLFramebufferCacheConfig_t *pConfig ) noexcept
{
	if ( pCache == nullptr || pConfig == nullptr || pConfig->pHost == nullptr || pConfig->pResources == nullptr ||
	     pConfig->pDispatch == nullptr || pConfig->pHost->pAllocate == nullptr || pConfig->pHost->pFree == nullptr ||
	     pConfig->nCapacity == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCache->pSlots != nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pConfig->nCapacity > UINT32_MAX / sizeof( sirGLFramebufferCacheSlot_t ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	void *pMemory =
	    pConfig->pHost->pAllocate( pConfig->pHost->pContext, ( uint64_t )pConfig->nCapacity * sizeof( sirGLFramebufferCacheSlot_t ),
		                           alignof( sirGLFramebufferCacheSlot_t ) );
	if ( pMemory == nullptr ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	memset( pMemory, 0, ( size_t )pConfig->nCapacity * sizeof( sirGLFramebufferCacheSlot_t ) );
	pCache->host = *pConfig->pHost;
	pCache->pResources = pConfig->pResources;
	pCache->pDispatch = pConfig->pDispatch;
	pCache->pSlots = ( sirGLFramebufferCacheSlot_t * )pMemory;
	pCache->nCapacity = pConfig->nCapacity;
	pCache->nCount = 0;
	pCache->nNextVictim = 0;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLFramebufferCacheAcquire( sirGLFramebufferCache_t *pCache, const sirGLFramebufferCacheKey_t *pKey,
                                                  uint32_t *pName ) noexcept
{
	if ( pCache == nullptr || pCache->pSlots == nullptr || pKey == nullptr || pName == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLNativeResource_t color = {};
	sirGLNativeResource_t depth = {};
	sirGraphicsResult_t eResult = sirGLFramebufferResolveAttachment( pCache->pResources, pKey, false, &color );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = sirGLFramebufferResolveAttachment( pCache->pResources, pKey, true, &depth );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( color.nName == 0 && depth.nName == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( color.nName != 0 && color.nName == depth.nName ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pCache->nCapacity; ++i ) {
		const sirGLFramebufferCacheSlot_t *pSlot = &pCache->pSlots[i];
		if ( pSlot->nName != 0 && sirGLFramebufferSameKey( &pSlot->key, pKey ) ) {
			*pName = pSlot->nName;
			return SIR_GRAPHICS_SUCCESS;
		}
	}
	sirGLFramebufferDesc_t desc = {};
	desc.nColorName = color.nName;
	desc.nColorMip = pKey->nColorMip;
	desc.eColorFormat = color.eFormat;
	desc.nDepthName = depth.nName;
	desc.nDepthMip = pKey->nDepthMip;
	desc.eDepthFormat = depth.eFormat;
	uint32_t nNewName = 0;
	eResult = sirGLCreateNativeFramebuffer( pCache->pDispatch, &desc, &nNewName );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	uint32_t nSlotIndex = pCache->nCapacity;
	for ( uint32_t i = 0; i < pCache->nCapacity; ++i ) {
		if ( pCache->pSlots[i].nName == 0 ) {
			nSlotIndex = i;
			break;
		}
	}
	if ( nSlotIndex == pCache->nCapacity ) {
		nSlotIndex = pCache->nNextVictim;
		eResult = sirGLDeleteNativeFramebuffer( pCache->pDispatch, pCache->pSlots[nSlotIndex].nName );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			( void )sirGLDeleteNativeFramebuffer( pCache->pDispatch, nNewName );
			return eResult;
		}
		pCache->nNextVictim = nSlotIndex + 1 == pCache->nCapacity ? 0 : nSlotIndex + 1;
	} else {
		++pCache->nCount;
	}
	pCache->pSlots[nSlotIndex].key = *pKey;
	pCache->pSlots[nSlotIndex].nName = nNewName;
	*pName = nNewName;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLFramebufferCacheInvalidate( sirGLFramebufferCache_t *pCache, sirGraphicsHandle_t hResource ) noexcept
{
	if ( pCache == nullptr || pCache->pSlots == nullptr || hResource.nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pCache->nCapacity; ++i ) {
		sirGLFramebufferCacheSlot_t *pSlot = &pCache->pSlots[i];
		if ( pSlot->nName == 0 ) {
			continue;
		}
		if ( !sirGLFramebufferSameHandle( pSlot->key.hColor, hResource ) && !sirGLFramebufferSameHandle( pSlot->key.hDepth, hResource ) ) {
			continue;
		}
		const sirGraphicsResult_t eResult = sirGLDeleteNativeFramebuffer( pCache->pDispatch, pSlot->nName );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		*pSlot = {};
		--pCache->nCount;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLFramebufferCacheShutdown( sirGLFramebufferCache_t *pCache ) noexcept
{
	if ( pCache == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCache->pSlots == nullptr ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	for ( uint32_t i = 0; i < pCache->nCapacity; ++i ) {
		sirGLFramebufferCacheSlot_t *pSlot = &pCache->pSlots[i];
		if ( pSlot->nName == 0 ) {
			continue;
		}
		const sirGraphicsResult_t eResult = sirGLDeleteNativeFramebuffer( pCache->pDispatch, pSlot->nName );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		*pSlot = {};
		--pCache->nCount;
	}
	pCache->host.pFree( pCache->host.pContext, pCache->pSlots );
	*pCache = {};
	return SIR_GRAPHICS_SUCCESS;
}
