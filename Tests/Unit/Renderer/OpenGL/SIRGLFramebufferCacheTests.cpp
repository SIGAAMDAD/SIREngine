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
#include <stdio.h>
#include <stdlib.h>

static uint32_t s_nFailures = 0;
static uint32_t s_nCreated = 0;
static uint32_t s_nDeleted = 0;
static uint32_t s_nAllocations = 0;
static uint32_t s_nCachedBytes = 0;
static uint32_t s_nDeleteFailureCount = 0;
static bool s_bAllocationFailure = false;
static bool s_bCreateFailure = false;
static uint32_t s_nLastDeleted = 0;
static sirGLFramebufferDesc_t s_lastFramebuffer = {};

static void sirGLCheck( bool bValue, const char *pszMessage ) noexcept
{
	if ( !bValue ) {
		fprintf( stderr, "FAIL %s\n", pszMessage );
		++s_nFailures;
	}
}

static void *sirGLTestAllocate( void *, uint64_t nBytes, uint64_t ) noexcept
{
	if ( s_bAllocationFailure ) {
		return nullptr;
	}
	void *pMemory = malloc( ( size_t )nBytes );
	if ( pMemory != nullptr ) {
		++s_nAllocations;
		s_nCachedBytes = ( uint32_t )nBytes;
	}
	return pMemory;
}

static void sirGLTestFree( void *, void *pMemory ) noexcept
{
	if ( pMemory != nullptr ) {
		--s_nAllocations;
		free( pMemory );
	}
}

sirGraphicsResult_t sirGLHandlePoolFind( const sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle,
                                         sirGLNativeResource_t *pNative ) noexcept
{
	if ( pPool == nullptr || pPool->pSlots == nullptr || pNative == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( hHandle.nGeneration == 0 || hHandle.nDeviceGeneration != pPool->nDeviceGeneration || hHandle.nSlot >= pPool->nCapacity ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGLHandleSlot_t *pSlot = &pPool->pSlots[hHandle.nSlot];
	if ( !pSlot->bLive || pSlot->nGeneration != hHandle.nGeneration || pSlot->resource.eType != hHandle.eType ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*pNative = pSlot->resource;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLCreateNativeFramebuffer( const sirGLFramebufferDispatch_t *, const sirGLFramebufferDesc_t *pDesc,
                                                  uint32_t *pName ) noexcept
{
	if ( s_bCreateFailure ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	s_lastFramebuffer = *pDesc;
	*pName = ++s_nCreated + 100;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLDeleteNativeFramebuffer( const sirGLFramebufferDispatch_t *, uint32_t nName ) noexcept
{
	if ( s_nDeleteFailureCount != 0 ) {
		--s_nDeleteFailureCount;
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	++s_nDeleted;
	s_nLastDeleted = nName;
	return SIR_GRAPHICS_SUCCESS;
}

int main( void )
{
	sirGraphicsABIHost_t host = {};
	host.pAllocate = sirGLTestAllocate;
	host.pFree = sirGLTestFree;
	sirGLHandleSlot_t szSlots[4] = {};
	sirGLHandlePool_t pool = {};
	pool.pSlots = szSlots;
	pool.nCapacity = 4;
	pool.nDeviceGeneration = 19;
	for ( uint32_t i = 0; i < 4; ++i ) {
		szSlots[i].bLive = true;
		szSlots[i].nGeneration = 1;
		szSlots[i].resource.nName = 11 + i;
		szSlots[i].resource.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
		szSlots[i].resource.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		szSlots[i].resource.nMipLevels = 3;
		szSlots[i].resource.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	}
	szSlots[3].resource.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	szSlots[3].resource.eFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	sirGLFramebufferDispatch_t dispatch = {};
	sirGLFramebufferCacheConfig_t config = {};
	config.pHost = &host;
	config.pResources = &pool;
	config.pDispatch = &dispatch;
	config.nCapacity = 2;
	sirGLFramebufferCache_t cache = {};
	sirGLCheck( sirGLFramebufferCacheInit( nullptr, &config ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null cache" );
	config.nCapacity = 0;
	sirGLCheck( sirGLFramebufferCacheInit( &cache, &config ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero capacity" );
	config.nCapacity = 2;
	s_bAllocationFailure = true;
	sirGLCheck( sirGLFramebufferCacheInit( &cache, &config ) == SIR_GRAPHICS_OUT_OF_MEMORY, "allocator failure" );
	s_bAllocationFailure = false;
	sirGLCheck( sirGLFramebufferCacheInit( &cache, &config ) == SIR_GRAPHICS_SUCCESS, "cache initialize" );
	sirGLCheck( sirGLFramebufferCacheInit( &cache, &config ) == SIR_GRAPHICS_BUSY, "double initialize" );
	sirGLCheck( s_nCachedBytes == sizeof( sirGLFramebufferCacheSlot_t ) * 2, "bounded host slab" );

	sirGLFramebufferCacheKey_t key = {};
	key.hColor = { 0, 1, 19, SIR_GRAPHICS_HANDLE_TEXTURE };
	uint32_t nFirst = 0;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &key, &nFirst ) == SIR_GRAPHICS_SUCCESS && nFirst == 101, "first framebuffer" );
	sirGLCheck( s_lastFramebuffer.nColorName == 11 && s_lastFramebuffer.eColorFormat == SIR_GRAPHICS_FORMAT_RGBA8_UNORM,
	            "source name and format" );
	uint32_t nHit = 0;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &key, &nHit ) == SIR_GRAPHICS_SUCCESS && nHit == nFirst && s_nCreated == 1,
	            "cache hit avoids new GL object" );
	sirGLFramebufferCacheKey_t keyTwo = key;
	keyTwo.hColor.nSlot = 1;
	uint32_t nSecond = 0;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &keyTwo, &nSecond ) == SIR_GRAPHICS_SUCCESS && nSecond == 102, "second framebuffer" );
	sirGLCheck( cache.nCount == 2, "capacity reached" );
	sirGLFramebufferCacheKey_t keyThree = key;
	keyThree.nColorMip = 1;
	s_bCreateFailure = true;
	uint32_t nThird = 0;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &keyThree, &nThird ) == SIR_GRAPHICS_OUT_OF_MEMORY && cache.nCount == 2,
	            "creation failure leaves cache intact" );
	s_bCreateFailure = false;
	s_nDeleteFailureCount = 1;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &keyThree, &nThird ) == SIR_GRAPHICS_DRIVER_ERROR && cache.pSlots[0].nName == nFirst,
	            "victim deletion failure leaves cache intact" );
	sirGLCheck( s_nLastDeleted == 103, "new object rolled back" );
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &keyThree, &nThird ) == SIR_GRAPHICS_SUCCESS && nThird == 104 &&
	                s_nLastDeleted == nFirst,
	            "FIFO replacement" );
	key.nColorMip = 3;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &key, &nHit ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid mip rejected" );
	key.nColorMip = 0;
	key.hColor.nGeneration = 9;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &key, &nHit ) == SIR_GRAPHICS_INVALID_HANDLE, "stale generation rejected" );
	key.hColor.nGeneration = 1;
	key.hColor.nSlot = 3;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &key, &nHit ) == SIR_GRAPHICS_UNSUPPORTED,
	            "depth texture cannot bind to color slot" );
	key.hColor = {};
	key.hDepth = { 3, 1, 19, SIR_GRAPHICS_HANDLE_TEXTURE };
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &key, &nHit ) == SIR_GRAPHICS_SUCCESS && s_lastFramebuffer.nDepthName == 14 &&
	                s_lastFramebuffer.nColorName == 0,
	            "depth attachment populated" );
	sirGLFramebufferCacheKey_t empty = {};
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &empty, &nHit ) == SIR_GRAPHICS_INVALID_ARGUMENT, "empty framebuffer rejected" );
	empty.hColor.nSlot = 2;
	sirGLCheck( sirGLFramebufferCacheAcquire( &cache, &empty, &nHit ) == SIR_GRAPHICS_INVALID_ARGUMENT, "malformed empty handle rejected" );
	const uint32_t nDeletesBefore = s_nDeleted;
	sirGLCheck( sirGLFramebufferCacheInvalidate( &cache, keyTwo.hColor ) == SIR_GRAPHICS_SUCCESS && s_nDeleted == nDeletesBefore,
	            "evicted key not invalidated" );
	sirGLCheck( sirGLFramebufferCacheInvalidate( &cache, key.hDepth ) == SIR_GRAPHICS_SUCCESS && cache.nCount == 1,
	            "depth retirement invalidates attachment" );
	sirGLCheck( sirGLFramebufferCacheInvalidate( &cache, keyThree.hColor ) == SIR_GRAPHICS_SUCCESS && cache.nCount == 0,
	            "color retirement invalidates attachment" );
	sirGLCheck( sirGLFramebufferCacheShutdown( &cache ) == SIR_GRAPHICS_SUCCESS, "shutdown success" );
	sirGLCheck( sirGLFramebufferCacheShutdown( &cache ) == SIR_GRAPHICS_SUCCESS, "idempotent shutdown" );
	sirGLCheck( s_nAllocations == 0, "slab returned" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 framebuffer cache tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
