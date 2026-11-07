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

#include <Renderer/Drivers/D3D11/SIRD3D11Handles.h>
#include <stdint.h>

SIRD3D11Handles::SIRD3D11Handles( uint32_t nDeviceGeneration ) noexcept : m_nDeviceGeneration( nDeviceGeneration )
{
	for ( uint32_t iSlot = 0; iSlot < CAPACITY; ++iSlot ) {
		m_szSlots[iSlot].nGeneration = 1;
	}
}

uint32_t SIRD3D11Handles::getDeviceGeneration( void ) const noexcept
{
	return m_nDeviceGeneration;
}

uint32_t SIRD3D11Handles::getActiveCount( void ) const noexcept
{
	return m_nActiveCount;
}

uint64_t SIRD3D11Handles::getAllocatedBytes( void ) const noexcept
{
	return m_nAllocatedBytes;
}

sirGraphicsResult_t SIRD3D11Handles::create( const sirD3D11ObjectInfo_t& object, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_nDeviceGeneration == 0 || object.pNativeObject == nullptr || object.eType <= SIR_GRAPHICS_HANDLE_NONE ||
	     object.eType > SIR_GRAPHICS_HANDLE_SURFACE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( UINT64_MAX - m_nAllocatedBytes < object.nSizeBytes ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	for ( uint32_t iSlot = 0; iSlot < CAPACITY; ++iSlot ) {
		sirD3D11ObjectSlot_t *pSlot = &m_szSlots[iSlot];
		if ( pSlot->object.pNativeObject != nullptr || pSlot->nGeneration == 0 ) {
			continue;
		}
		pSlot->object = object;
		pSlot->nMapped = 0;
		pSlot->nMappedOffsetBytes = 0;
		pSlot->nMappedBytes = 0;
		m_nAllocatedBytes += object.nSizeBytes;
		++m_nActiveCount;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, object.eType };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRD3D11Handles::lookup( sirGraphicsHandle_t handle, sirD3D11ObjectSlot_t **ppSlot ) noexcept
{
	if ( ppSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppSlot = nullptr;
	if ( m_nDeviceGeneration == 0 || handle.nDeviceGeneration != m_nDeviceGeneration || handle.nGeneration == 0 ||
	     handle.nSlot >= CAPACITY || handle.eType <= SIR_GRAPHICS_HANDLE_NONE || handle.eType > SIR_GRAPHICS_HANDLE_SURFACE ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D11ObjectSlot_t *pSlot = &m_szSlots[handle.nSlot];
	if ( pSlot->object.pNativeObject == nullptr || pSlot->nGeneration != handle.nGeneration || pSlot->object.eType != handle.eType ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*ppSlot = pSlot;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Handles::destroy( sirGraphicsHandle_t handle, void **ppNativeObject ) noexcept
{
	if ( ppNativeObject == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppNativeObject = nullptr;
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const sirGraphicsResult_t eLookup = lookup( handle, &pSlot );
	if ( eLookup != SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( pSlot->nMapped != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	*ppNativeObject = pSlot->object.pNativeObject;
	m_nAllocatedBytes -= pSlot->object.nSizeBytes;
	--m_nActiveCount;
	pSlot->object = {};
	pSlot->nMappedBytes = 0;
	pSlot->nMappedOffsetBytes = 0;
	// A wrapped generation is permanently retired; no stale handle revival.
	++pSlot->nGeneration;
	return SIR_GRAPHICS_SUCCESS;
}

sirD3D11ObjectSlot_t *SIRD3D11Handles::getSlot( uint32_t nIndex ) noexcept
{
	return nIndex < CAPACITY ? &m_szSlots[nIndex] : nullptr;
}

void SIRD3D11Handles::clear( void ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < CAPACITY; ++iSlot ) {
		m_szSlots[iSlot].object = {};
		m_szSlots[iSlot].nMapped = 0;
		m_szSlots[iSlot].nMappedBytes = 0;
		m_szSlots[iSlot].nMappedOffsetBytes = 0;
		++m_szSlots[iSlot].nGeneration;
	}
	m_nActiveCount = 0;
	m_nAllocatedBytes = 0;
	m_nDeviceGeneration = 0;
}
