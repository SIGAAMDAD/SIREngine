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



static bool testAsyncCopyQueue( SIRD3D12Device &device )
{
	SIRD3D12Transfers &transfers = device.getTransfers();
	const uint32_t nSlots = transfers.getSlotCount();
	if ( !transfers.isReady() || nSlots != 3 ) {
		return false;
	}

	uint32_t szInput[3][64] = {};
	sirD3D12TransferTicket_t szTickets[3] = {};
	for ( uint32_t iSlot = 0; iSlot < nSlots; ++iSlot ) {
		for ( uint32_t iWord = 0; iWord < 64; ++iWord ) {
			szInput[iSlot][iWord] = 0x5a5a0000U ^ ( iSlot * 8191U ) ^ ( iWord * 73U );
		}
		if ( !transfers.submit( szInput[iSlot], sizeof( szInput[iSlot] ), &szTickets[iSlot] ) ) {
			return false;
		}
	}
	sirD3D12TransferTicket_t rejected = {};
	if ( transfers.submit( szInput[0], sizeof( szInput[0] ), &rejected ) || rejected.nSerial != 0 ) {
		return false;
	}

	for ( uint32_t iSlot = 0; iSlot < nSlots; ++iSlot ) {
		bool bCompleted = false;
		if ( !transfers.poll( szTickets[iSlot], &bCompleted ) || !transfers.wait( szTickets[iSlot], 15000 ) ) {
			return false;
		}
		uint32_t szOutput[64] = {};
		if ( transfers.receive( szTickets[iSlot], szOutput, sizeof( szOutput ) - 1 ) ||
		     !transfers.receive( szTickets[iSlot], szOutput, sizeof( szOutput ) ) ||
		     memcmp( szInput[iSlot], szOutput, sizeof( szOutput ) ) != 0 ) {
			return false;
		}
		if ( transfers.receive( szTickets[iSlot], szOutput, sizeof( szOutput ) ) ) {
			return false;
		}
	}

	for ( uint32_t iPass = 0; iPass < 96; ++iPass ) {
		uint32_t szPayload[128] = {};
		for ( uint32_t iWord = 0; iWord < 128; ++iWord ) {
			szPayload[iWord] = ( iPass + 1 ) * 0x9e3779b9U + iWord;
		}
		sirD3D12TransferTicket_t ticket = {};
		const uint64_t nBytes = iPass % 2 == 0 ? 64 : sizeof( szPayload );
		if ( !transfers.submit( szPayload, nBytes, &ticket ) || !transfers.wait( ticket, 15000 ) ) {
			return false;
		}
		if ( iPass % 7 == 0 ) {
			if ( !transfers.discard( ticket ) || transfers.discard( ticket ) ) {
				return false;
			}
		} else {
			uint32_t szOutput[128] = {};
			if ( !transfers.receive( ticket, szOutput, sizeof( szOutput ) ) ||
			     memcmp( szPayload, szOutput, ( size_t )nBytes ) != 0 ) {
				return false;
			}
		}
	}
	if ( transfers.getCompletedCopyCount() != 3 + 96 - 14 ) {
		return false;
	}

	sirD3D12TransferTicket_t invalid = { 0, 999 };
	bool bCompleted = false;
	if ( transfers.poll( invalid, &bCompleted ) || transfers.poll( szTickets[0], &bCompleted ) ||
	     transfers.submit( nullptr, 8, &invalid ) || transfers.submit( szInput[0], 0, &invalid ) ) {
		return false;
	}
	return true;
}

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

	for ( uint32_t iPass = 0; iPass < 64; ++iPass ) {
		for ( uint32_t iWord = 0; iWord < 32; ++iWord ) {
			szInput[iWord] = ( iPass * 0x9e3779b9U ) ^ ( iWord * 1337U );
		}
		memset( szOutput, 0, sizeof( szOutput ) );
		if ( !device.copyRoundTripForTest( szInput, szOutput, sizeof( szInput ) ) || memcmp( szInput, szOutput, sizeof( szInput ) ) != 0 ) {
			fprintf( stderr, "D3D12 pooled command / D3D12MA transfer failed at pass %u: HRESULT 0x%08lx\n", iPass,
			         ( unsigned long )device.getLastError() );
			return 1;
		}
	}
	if ( device.getCompletedCopyCount() != 64 ) {
		fputs( "D3D12 completion counter drifted\n", stderr );
		return 1;
	}

	// DXGI memory-budget reporting can be unavailable for software adapters.
	if ( device.queryLocalMemory( &memory ) && memory.nCurrentUsageBytes > memory.nBudgetBytes && memory.nBudgetBytes != 0 ) {
		fputs( "D3D12 memory report is inconsistent\n", stderr );
		return 1;
	}

	if ( !testAsyncCopyQueue( device ) ) {
		fputs( "D3D12 asynchronous copy queue failed\n", stderr );
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
