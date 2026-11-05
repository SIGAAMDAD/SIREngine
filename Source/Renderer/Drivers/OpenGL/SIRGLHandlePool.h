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

#ifndef __SIRENGINE_SIRGLHANDLEPOOL_H__
#define __SIRENGINE_SIRGLHANDLEPOOL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLNativeResource_s
{
	uint32_t nName;
	sirGraphicsHandleType_t eType;
	uint64_t nRequestedBytes;
	uint32_t nInternalFormat;
	sirGraphicsFormat_t eFormat;
	uint32_t nMipLevels;
	uint32_t nUsage;
	sirGraphicsHandle_t hParent;
} sirGLNativeResource_t;

typedef struct sirGLHandleSlot_s
{
	sirGLNativeResource_t resource;
	uint32_t nGeneration;
	uint32_t nNextFree;
	uint32_t nChildCount;
	bool bLive;
} sirGLHandleSlot_t;

typedef struct sirGLHandlePool_s
{
	sirGraphicsABIHost_t host;
	sirGLHandleSlot_t *pSlots;
	uint32_t nCapacity;
	uint32_t nActive;
	uint32_t nFreeHead;
	uint32_t nDeviceGeneration;
} sirGLHandlePool_t;

sirGraphicsResult_t sirGLHandlePoolInit( sirGLHandlePool_t *pPool, const sirGraphicsABIHost_t *pHost, uint32_t nDeviceGeneration,
                                         uint32_t nCapacity ) noexcept;
sirGraphicsResult_t sirGLHandlePoolInsert( sirGLHandlePool_t *pPool, const sirGLNativeResource_t *pResource,
                                           sirGraphicsHandle_t *pHandle ) noexcept;
sirGraphicsResult_t sirGLHandlePoolFind( const sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle,
                                         sirGLNativeResource_t *pResource ) noexcept;
sirGraphicsResult_t sirGLHandlePoolTake( sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle, sirGLNativeResource_t *pResource ) noexcept;
sirGraphicsResult_t sirGLHandlePoolShutdown( sirGLHandlePool_t *pPool ) noexcept;

#endif // __SIRENGINE_SIRGLHANDLEPOOL_H__
