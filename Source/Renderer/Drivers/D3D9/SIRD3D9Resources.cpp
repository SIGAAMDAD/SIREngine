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
#include <Renderer/Drivers/D3D9/SIRD3D9Resources.h>
#include <limits.h>

sirGraphicsResult_t sirD3D9ResourcesInitialize( sirD3D9ResourceTable_t *pTable, sirD3D9ResourceSlot_t *pSlots, uint32_t nCapacity,
                                                uint64_t nBudgetBytes ) noexcept
{
	if ( pTable == nullptr || pSlots == nullptr || nCapacity == 0 || nBudgetBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pTable = {};
	pTable->pSlots = pSlots;
	pTable->nCapacity = nCapacity;
	pTable->nBudgetBytes = nBudgetBytes;
	pTable->nDeviceGeneration = 1;
	for ( uint32_t i = 0; i < nCapacity; ++i ) {
		pSlots[i] = {};
		pSlots[i].nGeneration = 1;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ResourcesCanAllocate( const sirD3D9ResourceTable_t *pTable, uint64_t nBytes ) noexcept
{
	if ( pTable == nullptr || pTable->pSlots == nullptr || pTable->nCapacity == 0 || pTable->nDeviceGeneration == 0 || nBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pTable->nUsedBytes > pTable->nBudgetBytes || nBytes > pTable->nBudgetBytes - pTable->nUsedBytes ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	for ( uint32_t i = 0; i < pTable->nCapacity; ++i ) {
		const sirD3D9ResourceSlot_t *pSlot = &pTable->pSlots[i];
		if ( !pSlot->bLive && !pSlot->bRetiring && pSlot->nGeneration != UINT32_MAX ) {
			return SIR_GRAPHICS_SUCCESS;
		}
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t sirD3D9ResourcesInsert( sirD3D9ResourceTable_t *pTable, sirGraphicsHandleType_t eType, uint64_t nBytes, void *pNative,
                                            sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pTable == nullptr || pHandle == nullptr || pNative == nullptr || pTable->pSlots == nullptr || nBytes == 0 ||
	     eType == SIR_GRAPHICS_HANDLE_NONE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eBudget = sirD3D9ResourcesCanAllocate( pTable, nBytes );
	if ( eBudget != SIR_GRAPHICS_SUCCESS ) {
		return eBudget;
	}
	for ( uint32_t i = 0; i < pTable->nCapacity; ++i ) {
		sirD3D9ResourceSlot_t *pSlot = &pTable->pSlots[i];
		if ( !pSlot->bLive && !pSlot->bRetiring && pSlot->nGeneration != UINT32_MAX ) {
			pSlot->bLive = true;
			pSlot->pNative = pNative;
			pSlot->nBytes = nBytes;
			pSlot->eType = eType;
			*pHandle = { i, pSlot->nGeneration, pTable->nDeviceGeneration, eType };
			pTable->nUsedBytes += nBytes;
			if ( pTable->nUsedBytes > pTable->nPeakBytes ) {
				pTable->nPeakBytes = pTable->nUsedBytes;
			}
			return SIR_GRAPHICS_SUCCESS;
		}
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t sirD3D9ResourcesLookup( const sirD3D9ResourceTable_t *pTable, sirGraphicsHandle_t hHandle, void **ppNative ) noexcept
{
	if ( ppNative == nullptr || pTable == nullptr || pTable->pSlots == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppNative = nullptr;
	if ( hHandle.nDeviceGeneration != pTable->nDeviceGeneration || hHandle.nSlot >= pTable->nCapacity ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirD3D9ResourceSlot_t *pSlot = &pTable->pSlots[hHandle.nSlot];
	if ( !pSlot->bLive || pSlot->bRetiring || pSlot->nGeneration != hHandle.nGeneration || pSlot->eType != hHandle.eType ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*ppNative = pSlot->pNative;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ResourcesRetire( sirD3D9ResourceTable_t *pTable, sirGraphicsHandle_t hHandle,
                                            uint64_t nCompletionValue ) noexcept
{
	void *pNative = nullptr;
	const sirGraphicsResult_t eResult = sirD3D9ResourcesLookup( pTable, hHandle, &pNative );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	sirD3D9ResourceSlot_t *pSlot = &pTable->pSlots[hHandle.nSlot];
	pSlot->bLive = false;
	pSlot->bRetiring = true;
	pSlot->nRetireValue = nCompletionValue;
	return SIR_GRAPHICS_SUCCESS;
}

void *sirD3D9ResourcesCollect( sirD3D9ResourceTable_t *pTable, uint64_t nCompletedValue ) noexcept
{
	if ( pTable == nullptr || pTable->pSlots == nullptr ) {
		return nullptr;
	}
	for ( uint32_t i = 0; i < pTable->nCapacity; ++i ) {
		sirD3D9ResourceSlot_t *pSlot = &pTable->pSlots[i];
		if ( pSlot->bRetiring && pSlot->nRetireValue <= nCompletedValue ) {
			void *pNative = pSlot->pNative;
			pTable->nUsedBytes -= pSlot->nBytes;
			const uint32_t nGeneration = pSlot->nGeneration + 1;
			*pSlot = {};
			pSlot->nGeneration = nGeneration;
			return pNative;
		}
	}
	return nullptr;
}

void sirD3D9ResourcesInvalidate( sirD3D9ResourceTable_t *pTable ) noexcept
{
	if ( pTable == nullptr || pTable->pSlots == nullptr ) {
		return;
	}
	// Caller must release all native resources before invalidation/reset.
	for ( uint32_t i = 0; i < pTable->nCapacity; ++i ) {
		const uint32_t nGeneration = pTable->pSlots[i].nGeneration;
		pTable->pSlots[i] = {};
		pTable->pSlots[i].nGeneration = nGeneration == UINT32_MAX ? UINT32_MAX : nGeneration + 1;
	}
	pTable->nUsedBytes = 0;
	pTable->nDeviceGeneration = pTable->nDeviceGeneration == UINT32_MAX ? 0 : pTable->nDeviceGeneration + 1;
}
