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

#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>

#include <stdio.h>
#include <string.h>

int main( void )
{
	SIRD3D12Device device;
	sirD3D12AdapterInfo_t adapter = {};
	sirD3D12MemoryInfo_t memory = {};
	uint32_t szInput[32] = {};
	uint32_t szOutput[32] = {};

	if ( device.isReady() || device.queryAdapterInfo( &adapter ) || device.queryLocalMemory( &memory ) ||
	     device.copyRoundTripForTest( szInput, szOutput, sizeof( szInput ) ) ) {
		fputs( "D3D12 accepted an operation before initialization\n", stderr );
		return 1;
	}

	if ( !device.initialize( true ) ) {
		fprintf( stderr, "D3D12 WARP device initialization failed: HRESULT 0x%08lx\n", ( unsigned long )device.getLastError() );
		return 1;
	}
	if ( !device.isReady() || device.initialize( true ) || !device.queryAdapterInfo( &adapter ) || !adapter.bSoftware ) {
		fputs( "D3D12 WARP adapter or lifecycle contract failed\n", stderr );
		return 1;
	}

	for ( uint32_t iWord = 0; iWord < 32; ++iWord ) {
		szInput[iWord] = 0x12345678U ^ ( iWord * 1337U );
	}
	if ( device.copyRoundTripForTest( nullptr, szOutput, sizeof( szInput ) ) ||
	     device.copyRoundTripForTest( szInput, nullptr, sizeof( szInput ) ) || device.copyRoundTripForTest( szInput, szOutput, 0 ) ) {
		fputs( "D3D12 invalid transfer accepted\n", stderr );
		return 1;
	}

	if ( !device.copyRoundTripForTest( szInput, szOutput, sizeof( szInput ) ) || memcmp( szInput, szOutput, sizeof( szInput ) ) != 0 ) {
		fprintf( stderr, "D3D12 upload/device/readback round trip failed: HRESULT 0x%08lx\n", ( unsigned long )device.getLastError() );
		return 1;
	}

	// DXGI memory-budget reporting can be unavailable for software adapters.
	if ( device.queryLocalMemory( &memory ) && memory.nCurrentUsageBytes > memory.nBudgetBytes && memory.nBudgetBytes != 0 ) {
		fputs( "D3D12 memory report is inconsistent\n", stderr );
		return 1;
	}

	device.shutdown();
	device.shutdown();
	if ( device.isReady() || device.copyRoundTripForTest( szInput, szOutput, sizeof( szInput ) ) ) {
		fputs( "D3D12 accepted an operation after shutdown\n", stderr );
		return 1;
	}
	return 0;
}
