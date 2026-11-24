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

#ifndef __SIR_GLHANDLEPOOL_H__
#define __SIR_GLHANDLEPOOL_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLNativeResource_s
{
	uint32_t nName;
	SIRGraphicsHandleType eType;
	uint64_t nRequestedBytes;
	uint32_t nInternalFormat;
	SIRGraphicsFormat eFormat;
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

SIRGraphicsResult sirGLHandlePoolInit( sirGLHandlePool_t *pPool, const sirGraphicsABIHost_t *pHost, uint32_t nDeviceGeneration,
                                         uint32_t nCapacity ) noexcept;
SIRGraphicsResult sirGLHandlePoolInsert( sirGLHandlePool_t *pPool, const sirGLNativeResource_t *pResource,
                                           sirGraphicsHandle_t *pHandle ) noexcept;
SIRGraphicsResult sirGLHandlePoolFind( const sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle,
                                         sirGLNativeResource_t *pResource ) noexcept;
SIRGraphicsResult sirGLHandlePoolTake( sirGLHandlePool_t *pPool, sirGraphicsHandle_t hHandle, sirGLNativeResource_t *pResource ) noexcept;
SIRGraphicsResult sirGLHandlePoolShutdown( sirGLHandlePool_t *pPool ) noexcept;

#endif // __SIR_GLHANDLEPOOL_H__
