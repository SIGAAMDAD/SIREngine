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

#ifndef __SIRENGINE_SIRGLFRAMEBUFFERCACHE_H__
#define __SIRENGINE_SIRGLFRAMEBUFFERCACHE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/OpenGL/SIRGLFramebuffers.h>
#include <Renderer/Drivers/OpenGL/SIRGLHandlePool.h>

typedef struct sirGLFramebufferCacheKey_s
{
	sirGraphicsHandle_t hColor;
	sirGraphicsHandle_t hDepth;
	uint32_t nColorMip;
	uint32_t nDepthMip;
} sirGLFramebufferCacheKey_t;

typedef struct sirGLFramebufferCacheSlot_s
{
	sirGLFramebufferCacheKey_t key;
	uint32_t nName;
} sirGLFramebufferCacheSlot_t;

typedef struct sirGLFramebufferCacheConfig_s
{
	const sirGraphicsABIHost_t *pHost;
	const sirGLHandlePool_t *pResources;
	const sirGLFramebufferDispatch_t *pDispatch;
	uint32_t nCapacity;
} sirGLFramebufferCacheConfig_t;

typedef struct sirGLFramebufferCache_s
{
	sirGraphicsABIHost_t host;
	const sirGLHandlePool_t *pResources;
	const sirGLFramebufferDispatch_t *pDispatch;
	sirGLFramebufferCacheSlot_t *pSlots;
	uint32_t nCapacity;
	uint32_t nCount;
	uint32_t nNextVictim;
} sirGLFramebufferCache_t;

sirGraphicsResult_t sirGLFramebufferCacheInit( sirGLFramebufferCache_t *pCache, const sirGLFramebufferCacheConfig_t *pConfig ) noexcept;
sirGraphicsResult_t sirGLFramebufferCacheAcquire( sirGLFramebufferCache_t *pCache, const sirGLFramebufferCacheKey_t *pKey,
                                                  uint32_t *pName ) noexcept;
sirGraphicsResult_t sirGLFramebufferCacheInvalidate( sirGLFramebufferCache_t *pCache, sirGraphicsHandle_t hResource ) noexcept;
sirGraphicsResult_t sirGLFramebufferCacheShutdown( sirGLFramebufferCache_t *pCache ) noexcept;

#endif // __SIRENGINE_SIRGLFRAMEBUFFERCACHE_H__
