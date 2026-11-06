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

int main( void )
{
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	if ( FAILED( CreateDXGIFactory2( 0, IID_PPV_ARGS( pFactory.GetAddressOf() ) ) ) ) {
		return 1;
	}
	uint32_t nAdapters = 0;
	if ( FAILED( sirD3D12EnumerateAdapters( pFactory.Get(), UINT32_MAX, nullptr, &nAdapters ) ) || nAdapters == 0 ) {
		return 1;
	}

	SIRD3D12Device device;
	sirD3D12TransferPoolConfig_t config = {};
	config.nSlotBytes = 4096;
	config.bPreferDirectMapped = true;
	if ( !device.initialize( nAdapters - 1, config ) ) {
		fprintf( stderr, "D3D12 WARP initialization failed: 0x%08lx\n", ( unsigned long )device.getLastError() );
		return 1;
	}
	SIRD3D12Transfers& transfers = device.getTransfers();
	const uint32_t nPayload = 0x1234abcdU;
	sirD3D12TransferTicket_t ticket = { 123, 2 };
	if ( transfers.submit( nullptr, sizeof( nPayload ), &ticket ) || ticket.nSerial != 0 || ticket.iSlot != 0 ||
	     transfers.getLastError() != E_INVALIDARG ) {
		return 1;
	}
	ticket = { 456, 2 };
	if ( transfers.submit( &nPayload, 0, &ticket ) || ticket.nSerial != 0 || ticket.iSlot != 0 ||
	     transfers.getLastError() != E_INVALIDARG ) {
		return 1;
	}
	if ( transfers.submit( &nPayload, sizeof( nPayload ), nullptr ) || transfers.getLastError() != E_POINTER ) {
		return 1;
	}
	if ( transfers.waitForFence( 0, 0 ) || transfers.waitForFence( transfers.getLastSubmittedFence() + 1, 0 ) ||
	     transfers.getLastError() != E_INVALIDARG ) {
		return 1;
	}

	ID3D12CommandList *const szInvalidLists[] = { nullptr };
	uint64_t nFence = 999;
	if ( transfers.submitRecorded( szInvalidLists, 1, &nFence ) || nFence != 0 || transfers.getLastError() != E_INVALIDARG ||
	     transfers.submitRecorded( nullptr, 1, &nFence ) || transfers.submitRecorded( szInvalidLists, 0, &nFence ) ||
	     transfers.getLastSubmittedFence() != 0 ) {
		return 1;
	}

	const uint32_t nExpected = 0x4321dcbaU;
	uint32_t nActual = 0;
	if ( !transfers.submit( &nExpected, sizeof( nExpected ), &ticket ) || ticket.nSerial == 0 || !transfers.wait( ticket, 15000 ) ||
	     !transfers.receive( ticket, &nActual, sizeof( nActual ) ) || nActual != nExpected ) {
		return 1;
	}
	if ( transfers.waitForFence( transfers.getLastSubmittedFence() + 1, 0 ) || transfers.getLastError() != E_INVALIDARG ) {
		return 1;
	}
	device.shutdown();
	return 0;
}
