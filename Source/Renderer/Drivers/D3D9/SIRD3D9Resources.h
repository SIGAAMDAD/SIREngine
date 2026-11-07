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
#ifndef __SIRENGINE_SIRD3D9RESOURCES_H__
#define __SIRENGINE_SIRD3D9RESOURCES_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsTypes.h>

typedef struct sirD3D9ResourceSlot_s
{
	uint32_t nGeneration;
	sirGraphicsHandleType_t eType;
	uint64_t nBytes;
	uint64_t nRetireValue;
	void *pNative;
	bool bLive;
	bool bRetiring;
} sirD3D9ResourceSlot_t;

typedef struct sirD3D9ResourceTable_s
{
	sirD3D9ResourceSlot_t *pSlots;
	uint32_t nCapacity;
	uint32_t nDeviceGeneration;
	uint64_t nBudgetBytes;
	uint64_t nUsedBytes;
	uint64_t nPeakBytes;
} sirD3D9ResourceTable_t;

sirGraphicsResult_t sirD3D9ResourcesInitialize( sirD3D9ResourceTable_t *pTable, sirD3D9ResourceSlot_t *pSlots, uint32_t nCapacity,
                                                uint64_t nBudgetBytes ) noexcept;
sirGraphicsResult_t sirD3D9ResourcesInsert( sirD3D9ResourceTable_t *pTable, sirGraphicsHandleType_t eType, uint64_t nBytes, void *pNative,
                                            sirGraphicsHandle_t *pHandle ) noexcept;
sirGraphicsResult_t sirD3D9ResourcesLookup( const sirD3D9ResourceTable_t *pTable, sirGraphicsHandle_t hHandle, void **ppNative ) noexcept;
sirGraphicsResult_t sirD3D9ResourcesRetire( sirD3D9ResourceTable_t *pTable, sirGraphicsHandle_t hHandle,
                                            uint64_t nCompletionValue ) noexcept;
// Returns one native pointer at a time for the owner to release, after GPU completion.
void *sirD3D9ResourcesCollect( sirD3D9ResourceTable_t *pTable, uint64_t nCompletedValue ) noexcept;
void sirD3D9ResourcesInvalidate( sirD3D9ResourceTable_t *pTable ) noexcept;
#endif // __SIRENGINE_SIRD3D9RESOURCES_H__
