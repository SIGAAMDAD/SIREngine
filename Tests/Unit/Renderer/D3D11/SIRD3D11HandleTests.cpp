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
#include <stdio.h>

static uint32_t s_nAssertions = 0;
static uint32_t s_nFailures = 0;
static void sirD3D11Check( bool bValid ) noexcept
{
	++s_nAssertions;
	if ( !bValid ) {
		++s_nFailures;
	}
}

int main( void )
{
	int nNativeA = 13;
	int nNativeB = 41;
	SIRD3D11Handles handles( 7 );
	sirD3D11ObjectInfo_t object = { &nNativeA, 256, SIR_GRAPHICS_HANDLE_BUFFER, SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	sirGraphicsHandle_t handle = {};
	sirD3D11Check( handles.getDeviceGeneration() == 7 );
	sirD3D11Check( handles.getActiveCount() == 0 );
	sirD3D11Check( handles.create( object, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( handles.create( {}, &handle ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( handles.create( object, &handle ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( handle.nSlot == 0 && handle.nGeneration == 1 && handle.nDeviceGeneration == 7 );
	sirD3D11Check( handles.getActiveCount() == 1 && handles.getAllocatedBytes() == 256 );
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	sirD3D11Check( handles.lookup( handle, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( handles.lookup( handle, &pSlot ) == SIR_GRAPHICS_SUCCESS && pSlot != nullptr );
	sirGraphicsHandle_t invalid = handle;
	++invalid.nDeviceGeneration;
	sirD3D11Check( handles.lookup( invalid, &pSlot ) == SIR_GRAPHICS_INVALID_HANDLE && pSlot == nullptr );
	invalid = handle;
	invalid.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	sirD3D11Check( handles.lookup( invalid, &pSlot ) == SIR_GRAPHICS_INVALID_HANDLE );
	invalid = handle;
	invalid.nSlot = SIRD3D11Handles::CAPACITY;
	sirD3D11Check( handles.lookup( invalid, &pSlot ) == SIR_GRAPHICS_INVALID_HANDLE );
	invalid = handle;
	++invalid.nGeneration;
	sirD3D11Check( handles.lookup( invalid, &pSlot ) == SIR_GRAPHICS_INVALID_HANDLE );
	sirD3D11Check( handles.getSlot( SIRD3D11Handles::CAPACITY ) == nullptr );
	void *pNative = nullptr;
	sirD3D11Check( handles.destroy( handle, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( handles.lookup( handle, &pSlot ) == SIR_GRAPHICS_SUCCESS );
	pSlot->nMapped = 1;
	sirD3D11Check( handles.destroy( handle, &pNative ) == SIR_GRAPHICS_BUSY && pNative == nullptr );
	pSlot->nMapped = 0;
	sirD3D11Check( handles.destroy( handle, &pNative ) == SIR_GRAPHICS_SUCCESS && pNative == &nNativeA );
	sirD3D11Check( handles.destroy( handle, &pNative ) == SIR_GRAPHICS_INVALID_HANDLE && pNative == nullptr );
	sirD3D11Check( handles.getActiveCount() == 0 && handles.getAllocatedBytes() == 0 );
	sirGraphicsHandle_t second = {};
	object.pNativeObject = &nNativeB;
	sirD3D11Check( handles.create( object, &second ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( second.nSlot == handle.nSlot && second.nGeneration == handle.nGeneration + 1 );
	sirD3D11Check( handles.lookup( handle, &pSlot ) == SIR_GRAPHICS_INVALID_HANDLE );
	sirD3D11Check( handles.destroy( second, &pNative ) == SIR_GRAPHICS_SUCCESS && pNative == &nNativeB );
	object.nSizeBytes = 1;
	for ( uint32_t i = 0; i < SIRD3D11Handles::CAPACITY; ++i ) {
		sirD3D11Check( handles.create( object, &handle ) == SIR_GRAPHICS_SUCCESS );
	}
	sirD3D11Check( handles.getActiveCount() == SIRD3D11Handles::CAPACITY );
	sirD3D11Check( handles.create( object, &handle ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	handles.clear();
	sirD3D11Check( handles.getActiveCount() == 0 && handles.getAllocatedBytes() == 0 );
	sirD3D11Check( handles.create( object, &handle ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	SIRD3D11Handles zero( 0 );
	sirD3D11Check( zero.create( object, &handle ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	SIRD3D11Handles overflow( 3 );
	object.nSizeBytes = UINT64_MAX;
	sirD3D11Check( overflow.create( object, &handle ) == SIR_GRAPHICS_SUCCESS );
	object.nSizeBytes = 1;
	sirD3D11Check( overflow.create( object, &handle ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	printf( "D3D11 handle tests: %u assertions, %u failures\n", s_nAssertions, s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
