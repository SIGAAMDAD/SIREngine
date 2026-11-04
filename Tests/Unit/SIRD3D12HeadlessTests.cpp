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
#include <stdlib.h>
#include <string.h>

static bool testAsyncCopyQueue( SIRD3D12Device &device )
{
	SIRD3D12Transfers &transfers = device.getTransfers();
	const uint32_t nSlots = transfers.getSlotCount();
	sirD3D12ResidentTransferView_t arena = {};
	if ( !transfers.isReady() || nSlots != 3 || !transfers.queryResidentArena( &arena ) ||
	     arena.nSlotBytes == 0 || arena.nReservedBufferBytes == 0 ||
	     arena.nPersistentMappedBytes != arena.nSlotBytes * nSlots * 2 ||
	     arena.pWriteBytes == nullptr || arena.pReadbackBytes == nullptr ||
	     arena.pWriteResource == nullptr || arena.pReadbackResource == nullptr ) {
		return false;
	}
	if ( ( arena.ePath == SIR_D3D12_TRANSFER_STAGED ) == ( arena.pWriteResource == arena.pGpuResource ) ) {
		return false;
	}
	sirD3D12TransferTicket_t overCapacity = {};
	const uint32_t nSample = 0x55aa11eeU;
	if ( transfers.submit( &nSample, arena.nSlotBytes + 1, &overCapacity ) || overCapacity.nSerial != 0 ) {
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
			if ( !transfers.receive( ticket, szOutput, sizeof( szOutput ) ) || memcmp( szPayload, szOutput, ( size_t )nBytes ) != 0 ) {
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
	sirD3D12ResidentTransferView_t after = {};
	if ( !transfers.queryResidentArena( &after ) || arena.pWriteResource != after.pWriteResource ||
	     arena.pGpuResource != after.pGpuResource || arena.pReadbackResource != after.pReadbackResource ||
	     arena.pWriteBytes != after.pWriteBytes || arena.pReadbackBytes != after.pReadbackBytes ||
	     arena.nReservedBufferBytes != after.nReservedBufferBytes ) {
		return false;
	}
	return true;
}

static bool testComputeAndCompletions( SIRD3D12Device &device, sirD3D12CompletionSet_t *pOldSet )
{
	SIRD3D12ComputeQueue &compute = device.getComputeQueue();
	SIRD3D12Transfers &transfers = device.getTransfers();
	sirD3D12CompletionSet_t initial = {};
	if ( !compute.isReady() || !device.snapshotCompletionSet( &initial ) || initial.nDeviceGeneration == 0 ) {
		return false;
	}

	uint32_t szInput[64] = {};
	uint32_t szOutput[64] = {};
	for ( uint32_t iWord = 0; iWord < 64; ++iWord ) {
		szInput[iWord] = 0x5e0f0000U ^ ( iWord * 143U );
	}
	sirD3D12TransferTicket_t transfer = {};
	if ( !transfers.submit( szInput, sizeof( szInput ), &transfer ) ) {
		return false;
	}

	sirD3D12CompletionSet_t submitted = {};
	if ( !device.submitComputeMarker( &submitted ) || submitted.nComputeFence == 0 || submitted.nCopyFence <= initial.nCopyFence ||
	     submitted.nDeviceGeneration != initial.nDeviceGeneration ) {
		return false;
	}

	bool bCompleted = false;
	sirD3D12CompletionSet_t invalid = submitted;
	invalid.nDeviceGeneration = 0;
	if ( device.pollCompletionSet( invalid, &bCompleted ) || device.pollCompletionSet( submitted, nullptr ) ) {
		return false;
	}
	invalid = submitted;
	++invalid.nComputeFence;
	if ( device.pollCompletionSet( invalid, &bCompleted ) ) {
		return false;
	}
	if ( !compute.waitForFence( submitted.nComputeFence, 15000 ) || !transfers.wait( transfer, 15000 ) ||
	     !device.pollCompletionSet( submitted, &bCompleted ) || !bCompleted ) {
		return false;
	}
	if ( !transfers.receive( transfer, szOutput, sizeof( szOutput ) ) || memcmp( szInput, szOutput, sizeof( szInput ) ) != 0 ) {
		return false;
	}

	for ( uint32_t iCycle = 0; iCycle < 64; ++iCycle ) {
		uint64_t nFence = 0;
		if ( !compute.submitMarker( &nFence ) || !compute.waitForFence( nFence, 15000 ) ) {
			return false;
		}
		bool bDone = false;
		if ( !compute.pollFence( nFence, &bDone ) || !bDone || compute.pollFence( nFence + 1, &bDone ) ) {
			return false;
		}
	}
	if ( !device.snapshotCompletionSet( pOldSet ) || !device.pollCompletionSet( *pOldSet, &bCompleted ) || !bCompleted ) {
		return false;
	}
	return true;
}

static bool testCopyToComputeGpuWait( SIRD3D12Device &device )
{
	SIRD3D12Transfers &transfers = device.getTransfers();
	SIRD3D12ComputeQueue &compute = device.getComputeQueue();

	for ( uint32_t iCycle = 0; iCycle < 48; ++iCycle ) {
		uint32_t szInput[256] = {};
		uint32_t szOutput[256] = {};
		for ( uint32_t iWord = 0; iWord < 256; ++iWord ) {
			szInput[iWord] = ( iCycle + 1 ) * 0x9e3779b9U ^ ( iWord * 313U );
		}

		const uint64_t nBytes = ( iCycle % 2 == 0 ) ? 128 : sizeof( szInput );
		sirD3D12TransferTicket_t ticket = {};
		if ( !transfers.submit( szInput, nBytes, &ticket ) ) {
			return false;
		}

		sirD3D12TransferTicket_t forged = ticket;
		++forged.nSerial;
		sirD3D12CompletionSet_t completed = {};
		if ( device.submitComputeAfterCopy( forged, &completed ) || device.submitComputeAfterCopy( ticket, nullptr ) ) {
			return false;
		}
		if ( !device.submitComputeAfterCopy( ticket, &completed ) || completed.nComputeFence == 0 || completed.nCopyFence == 0 ||
		     completed.nDeviceGeneration == 0 ) {
			return false;
		}

		// A COMPUTE fence completion after the GPU-side wait proves that
		// the COPY queue reached the dependency's recorded signal.
		if ( !compute.waitForFence( completed.nComputeFence, 15000 ) ) {
			return false;
		}
		bool bCopyDone = false;
		bool bAllDone = false;
		if ( !transfers.poll( ticket, &bCopyDone ) || !bCopyDone || !device.pollCompletionSet( completed, &bAllDone ) || !bAllDone ) {
			return false;
		}
		if ( !transfers.receive( ticket, szOutput, sizeof( szOutput ) ) || memcmp( szInput, szOutput, ( size_t )nBytes ) != 0 ) {
			return false;
		}
		if ( device.submitComputeAfterCopy( ticket, &completed ) ) {
			return false;
		}
	}
	return true;
}

static bool testComputeToDirectGpuWait( SIRD3D12Device &device )
{
	SIRD3D12ComputeQueue &compute = device.getComputeQueue();
	SIRD3D12Transfers &transfers = device.getTransfers();

	sirD3D12CompletionSet_t invalid = {};
	sirD3D12CompletionSet_t result = {};
	if ( device.submitGraphicsAfterCompute( invalid, &result ) ) {
		return false;
	}
	for ( uint32_t iCycle = 0; iCycle < 48; ++iCycle ) {
		uint32_t szInput[128] = {};
		uint32_t szOutput[128] = {};
		for ( uint32_t iWord = 0; iWord < 128; ++iWord ) {
			szInput[iWord] = ( iCycle + 17U ) * 0x9e3779b9U ^ ( iWord * 371U );
		}

		const uint64_t nBytes = iCycle % 2 == 0 ? 128 : sizeof( szInput );
		sirD3D12TransferTicket_t ticket = {};
		if ( !transfers.submit( szInput, nBytes, &ticket ) ) {
			return false;
		}

		sirD3D12CompletionSet_t computed = {};
		if ( !device.submitComputeAfterCopy( ticket, &computed ) ) {
			return false;
		}
		sirD3D12CompletionSet_t forged = computed;
		++forged.nComputeFence;
		if ( device.submitGraphicsAfterCompute( forged, &result ) || device.submitGraphicsAfterCompute( computed, nullptr ) ) {
			return false;
		}
		forged = computed;
		forged.nDeviceGeneration = 0;
		if ( device.submitGraphicsAfterCompute( forged, &result ) ) {
			return false;
		}

		sirD3D12CompletionSet_t submitted = {};
		if ( !device.submitGraphicsAfterCompute( computed, &submitted ) ) {
			return false;
		}
		if ( submitted.nDeviceGeneration != computed.nDeviceGeneration || submitted.nGraphicsFence <= computed.nGraphicsFence ||
		     submitted.nComputeFence < computed.nComputeFence || submitted.nCopyFence < computed.nCopyFence ) {
			return false;
		}

		// A completed DIRECT fence proves COMPUTE finished, which in turn
		// proves the preceding COPY transfer completed on the GPU.
		if ( !device.waitForGraphicsFence( submitted.nGraphicsFence, 15000 ) ) {
			return false;
		}
		bool bComputeDone = false;
		bool bCopyDone = false;
		bool bAllDone = false;
		if ( !compute.pollFence( computed.nComputeFence, &bComputeDone ) || !bComputeDone || !transfers.poll( ticket, &bCopyDone ) ||
		     !bCopyDone || !device.pollCompletionSet( submitted, &bAllDone ) || !bAllDone ) {
			return false;
		}

		if ( !transfers.receive( ticket, szOutput, sizeof( szOutput ) ) || memcmp( szInput, szOutput, ( size_t )nBytes ) != 0 ) {
			return false;
		}
	}
	return true;
}

static bool testRetirementInvalidCompletions( SIRD3D12Device &device, const sirD3D12CompletionSet_t &set )
{
	D3D12MA::Allocation *pInvalid = nullptr;
	if ( !device.createNativeBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 4096, &pInvalid ) ) {
		return false;
	}

	sirD3D12CompletionSet_t invalid = set;
	invalid.nDeviceGeneration = 0;
	const bool bStaleAccepted = device.retireNativeAllocation( &pInvalid, invalid );
	if ( bStaleAccepted || pInvalid == nullptr ) {
		if ( pInvalid != nullptr ) {
			pInvalid->Release();
		}
		return false;
	}

	invalid = set;
	++invalid.nGraphicsFence;
	const bool bFutureAccepted = device.retireNativeAllocation( &pInvalid, invalid );
	const bool bNullAccepted = device.retireNativeAllocation( nullptr, set );
	const bool bValidRejection = !bFutureAccepted && !bNullAccepted && pInvalid != nullptr;
	if ( pInvalid != nullptr ) {
		pInvalid->Release();
	}
	return bValidRejection;
}

static bool testRetirementQueueCapacity( SIRD3D12Device &device, const sirD3D12CompletionSet_t &set )
{
	const uint32_t nCapacity = device.getRetirementCapacity();
	for ( uint32_t iEntry = 0; iEntry < nCapacity; ++iEntry ) {
		D3D12MA::Allocation *pAllocation = nullptr;
		if ( !device.createNativeBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 256, &pAllocation ) ) {
			return false;
		}
		if ( !device.retireNativeAllocation( &pAllocation, set ) || pAllocation != nullptr ) {
			if ( pAllocation != nullptr ) {
				pAllocation->Release();
			}
			return false;
		}
	}
	if ( device.getRetiredAllocationCount() != nCapacity ) {
		return false;
	}

	D3D12MA::Allocation *pOverflow = nullptr;
	if ( !device.createNativeBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 256, &pOverflow ) ) {
		return false;
	}
	const bool bAcceptedOverflow = device.retireNativeAllocation( &pOverflow, set );
	const bool bRejectedSafely = !bAcceptedOverflow && pOverflow != nullptr;
	if ( pOverflow != nullptr ) {
		pOverflow->Release();
	}
	return bRejectedSafely;
}

static bool testRetirementCollect( SIRD3D12Device &device, const sirD3D12CompletionSet_t &set )
{
	const uint32_t nCapacity = device.getRetirementCapacity();
	const uint32_t nEarlyCollected = device.collectRetiredAllocations();
	if ( nEarlyCollected > nCapacity || device.getRetiredAllocationCount() + nEarlyCollected != nCapacity ) {
		return false;
	}
	if ( !device.waitForGraphicsFence( set.nGraphicsFence, 15000 ) ) {
		return false;
	}

	const uint32_t nLateCollected = device.collectRetiredAllocations();
	if ( nEarlyCollected + nLateCollected != nCapacity || device.getRetiredAllocationCount() != 0 ||
	     device.collectRetiredAllocations() != 0 ) {
		return false;
	}
	return true;
}

static bool testDeferredMultiQueueRetirement( SIRD3D12Device &device )
{
	if ( device.getRetirementCapacity() != 32 || device.getRetiredAllocationCount() != 0 ) {
		return false;
	}

	uint32_t szInput[64] = {};
	uint32_t szOutput[64] = {};
	for ( uint32_t iWord = 0; iWord < 64; ++iWord ) {
		szInput[iWord] = ( iWord * 0x9e3779b9U ) ^ 0x55aabb11U;
	}

	sirD3D12TransferTicket_t ticket = {};
	if ( !device.getTransfers().submit( szInput, sizeof( szInput ), &ticket ) ) {
		return false;
	}
	sirD3D12CompletionSet_t computed = {};
	sirD3D12CompletionSet_t submitted = {};
	if ( !device.submitComputeAfterCopy( ticket, &computed ) || !device.submitGraphicsAfterCompute( computed, &submitted ) ) {
		return false;
	}
	if ( !testRetirementInvalidCompletions( device, submitted ) || !testRetirementQueueCapacity( device, submitted ) ||
	     !testRetirementCollect( device, submitted ) ) {
		return false;
	}

	bool bComplete = false;
	if ( !device.pollCompletionSet( submitted, &bComplete ) || !bComplete ||
	     !device.getTransfers().receive( ticket, szOutput, sizeof( szOutput ) ) || memcmp( szInput, szOutput, sizeof( szInput ) ) != 0 ) {
		return false;
	}

	// The registered allocation must be retired by shutdown even when
	// the caller never explicitly invokes collectRetiredAllocations().
	D3D12MA::Allocation *pLast = nullptr;
	if ( !device.createNativeBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 512, &pLast ) ) {
		return false;
	}
	const bool bTransferred = device.retireNativeAllocation( &pLast, submitted );
	if ( !bTransferred && pLast != nullptr ) {
		pLast->Release();
	}
	return bTransferred && pLast == nullptr && device.getRetiredAllocationCount() == 1;
}

static bool testCompiledDxilCompute( SIRD3D12Device &device )
{
	FILE *pFile = nullptr;
	if ( fopen_s( &pFile, SIR_D3D12_SMOKE_DXIL_PATH, "rb" ) != 0 || pFile == nullptr ) {
		fputs( "DXC test shader output missing\n", stderr );
		return false;
	}
	if ( fseek( pFile, 0, SEEK_END ) != 0 ) {
		fclose( pFile );
		return false;
	}
	const long nFileBytes = ftell( pFile );
	if ( nFileBytes < 16 || nFileBytes > 4 * 1024 * 1024 || fseek( pFile, 0, SEEK_SET ) != 0 ) {
		fclose( pFile );
		return false;
	}
	void *pShader = malloc( ( size_t )nFileBytes );
	if ( pShader == nullptr ) {
		fclose( pFile );
		return false;
	}
	const size_t nRead = fread( pShader, 1, ( size_t )nFileBytes, pFile );
	fclose( pFile );
	if ( nRead != ( size_t )nFileBytes ) {
		free( pShader );
		return false;
	}

	const bool bLoaded = device.initializeComputeKernel( pShader, nRead );
	free( pShader );
	if ( !bLoaded ) {
		fprintf( stderr, "DXIL compute PSO creation failed: HRESULT 0x%08lx\n", ( unsigned long )device.getLastError() );
		return false;
	}
	if ( device.initializeComputeKernel( nullptr, 0 ) ) {
		return false;
	}

	sirD3D12CompletionSet_t rejected = {};
	if ( device.submitComputeKernel( 0, 123, &rejected ) || device.submitComputeKernel( 4097, 123, &rejected ) ||
	     device.submitComputeKernel( 64, 123, nullptr ) ) {
		return false;
	}

	const uint32_t szCounts[] = { 1, 63, 64, 65, 130, 257, 4096 };
	for ( uint32_t iPass = 0; iPass < sizeof( szCounts ) / sizeof( szCounts[0] ); ++iPass ) {
		const uint32_t nCount = szCounts[iPass];
		const uint32_t nSeed = 0x1234567u ^ ( iPass * 0x9e3779b9u );
		sirD3D12CompletionSet_t submitted = {};
		if ( !device.submitComputeKernel( nCount, nSeed, &submitted ) || submitted.nComputeFence == 0 ) {
			fprintf( stderr, "DXIL compute dispatch failed for %u values: HRESULT 0x%08lx\n", nCount,
			         ( unsigned long )device.getLastError() );
			return false;
		}
		if ( device.submitComputeKernel( nCount, nSeed, &rejected ) ) {
			return false;
		}

		uint32_t szOutput[4096] = {};
		if ( device.getComputeQueue().readKernel( submitted.nComputeFence, szOutput, nCount - 1 ) ||
		     device.getComputeQueue().readKernel( submitted.nComputeFence + 1, szOutput, nCount ) ) {
			return false;
		}
		if ( !device.getComputeQueue().waitForFence( submitted.nComputeFence, 15000 ) ||
		     !device.getComputeQueue().readKernel( submitted.nComputeFence, szOutput, nCount ) ) {
			fprintf( stderr, "DXIL compute readback failed for %u values: HRESULT 0x%08lx\n", nCount,
			         ( unsigned long )device.getComputeQueue().getLastError() );
			return false;
		}

		for ( uint32_t iWord = 0; iWord < nCount; ++iWord ) {
			if ( szOutput[iWord] != ( ( iWord * 13u ) ^ nSeed ) ) {
				fprintf( stderr, "DXIL compute mismatch at pass %u element %u\n", iPass, iWord );
				return false;
			}
		}
		if ( device.getComputeQueue().readKernel( submitted.nComputeFence, szOutput, nCount ) ) {
			return false;
		}
	}
	return true;
}

static bool loadGraphicsDxil( const char *pszPath, void **ppBytes, size_t *pSize )
{
	if ( pszPath == nullptr || ppBytes == nullptr || pSize == nullptr ) {
		return false;
	}
	*ppBytes = nullptr;
	*pSize = 0;
	FILE *pFile = nullptr;
	if ( fopen_s( &pFile, pszPath, "rb" ) != 0 || pFile == nullptr ) {
		return false;
	}
	if ( fseek( pFile, 0, SEEK_END ) != 0 ) {
		fclose( pFile );
		return false;
	}
	const long nBytes = ftell( pFile );
	if ( nBytes < 16 || nBytes > 4 * 1024 * 1024 || fseek( pFile, 0, SEEK_SET ) != 0 ) {
		fclose( pFile );
		return false;
	}

	void *pBytes = malloc( ( size_t )nBytes );
	if ( pBytes == nullptr ) {
		fclose( pFile );
		return false;
	}
	const size_t nRead = fread( pBytes, 1, ( size_t )nBytes, pFile );
	fclose( pFile );
	if ( nRead != ( size_t )nBytes ) {
		free( pBytes );
		return false;
	}
	*ppBytes = pBytes;
	*pSize = nRead;
	return true;
}

static bool testOffscreenIndexedGraphics( SIRD3D12Device &device )
{
	void *pVertexDxil = nullptr;
	void *pPixelDxil = nullptr;
	size_t nVertexBytes = 0;
	size_t nPixelBytes = 0;

	const bool bVs = loadGraphicsDxil( SIR_D3D12_VS_DXIL_PATH, &pVertexDxil, &nVertexBytes );
	const bool bPs = bVs && loadGraphicsDxil( SIR_D3D12_PS_DXIL_PATH, &pPixelDxil, &nPixelBytes );
	if ( !bVs || !bPs ) {
		fputs( "D3D12 offline graphics VS/PS DXIL missing\n", stderr );
		free( pVertexDxil );
		free( pPixelDxil );
		return false;
	}

	const bool bInitialized = device.initializeGraphicsKernel( pVertexDxil, nVertexBytes, pPixelDxil, nPixelBytes );
	free( pVertexDxil );
	free( pPixelDxil );
	if ( !bInitialized ) {
		fprintf( stderr, "D3D12 graphics PSO initialization failed: HRESULT 0x%08lx\n", ( unsigned long )device.getLastError() );
		return false;
	}
	if ( device.initializeGraphicsKernel( nullptr, 0, nullptr, 0 ) ) {
		return false;
	}

	sirD3D12CompletionSet_t invalid = {};
	if ( device.submitGraphicsTriangle( 0, 32, &invalid ) || device.submitGraphicsTriangle( 32, 0, &invalid ) ||
	     device.submitGraphicsTriangle( 129, 32, &invalid ) || device.submitGraphicsTriangle( 32, 129, &invalid ) ||
	     device.submitGraphicsTriangle( 32, 32, nullptr ) ) {
		return false;
	}

	const uint32_t szWidth[] = { 32, 17, 64 };
	const uint32_t szHeight[] = { 32, 29, 48 };
	for ( uint32_t iPass = 0; iPass < 3; ++iPass ) {
		const uint32_t nWidth = szWidth[iPass];
		const uint32_t nHeight = szHeight[iPass];
		const uint32_t nStride = nWidth * 4 + 8;
		uint8_t szPixels[128 * 128 * 4] = {};
		sirD3D12CompletionSet_t submitted = {};

		if ( !device.submitGraphicsTriangle( nWidth, nHeight, &submitted ) || submitted.nGraphicsFence == 0 ) {
			fprintf( stderr, "D3D12 graphics draw submission failed at %u: HRESULT 0x%08lx\n", iPass,
			         ( unsigned long )device.getLastError() );
			return false;
		}
		if ( device.submitGraphicsTriangle( nWidth, nHeight, &invalid ) ) {
			return false;
		}

		sirD3D12CompletionSet_t forged = submitted;
		forged.nDeviceGeneration = 0;
		if ( device.readGraphicsTriangle( forged, szPixels, nStride, sizeof( szPixels ) ) ) {
			return false;
		}
		forged = submitted;
		++forged.nGraphicsFence;
		if ( device.readGraphicsTriangle( forged, szPixels, nStride, sizeof( szPixels ) ) ||
		     device.readGraphicsTriangle( submitted, szPixels, nWidth * 4 - 1, sizeof( szPixels ) ) ||
		     device.readGraphicsTriangle( submitted, szPixels, nStride, nWidth * 4 ) ) {
			return false;
		}

		if ( !device.waitForGraphicsFence( submitted.nGraphicsFence, 15000 ) ||
		     !device.readGraphicsTriangle( submitted, szPixels, nStride, sizeof( szPixels ) ) ) {
			fprintf( stderr, "D3D12 graphics readback failed at %u: HRESULT 0x%08lx\n", iPass, ( unsigned long )device.getLastError() );
			return false;
		}

		const uint8_t *pCenter = &szPixels[( nHeight / 2 ) * nStride + ( nWidth / 2 ) * 4];
		const uint8_t *pCorner = &szPixels[0];
		const uint8_t *pOutside = &szPixels[( nHeight - 1 ) * nStride + ( nWidth - 1 ) * 4];
		if ( pCenter[0] < 225 || pCenter[1] > 15 || pCenter[2] > 15 || pCenter[3] != 255 || pCorner[0] != 0 || pCorner[1] != 0 ||
		     pCorner[2] != 0 || pCorner[3] != 255 || pOutside[0] != 0 || pOutside[1] != 0 || pOutside[2] != 0 || pOutside[3] != 255 ) {
			fprintf( stderr, "D3D12 graphics pixel mismatch at %u: center %u/%u/%u/%u corner %u/%u/%u/%u\n", iPass, pCenter[0], pCenter[1],
			         pCenter[2], pCenter[3], pCorner[0], pCorner[1], pCorner[2], pCorner[3] );
			return false;
		}
		if ( device.readGraphicsTriangle( submitted, szPixels, nStride, sizeof( szPixels ) ) ) {
			return false;
		}
	}
	return true;
}

int main( void )
{
	SIRD3D12Device device;
	sirD3D12TransferPoolConfig_t transferConfig = {};
	transferConfig.nSlotBytes = 64 * 1024;
	transferConfig.bPreferDirectMapped = true;
	sirD3D12AdapterInfo_t adapter = {};
	sirD3D12MemoryInfo_t memory = {};
	uint32_t szInput[32] = {};
	uint32_t szOutput[32] = {};

	if ( device.isReady() || device.queryAdapterInfo( &adapter ) || device.queryLocalMemory( &memory ) ||
	     device.copyRoundTripForTest( szInput, szOutput, sizeof( szInput ) ) ) {
		fputs( "D3D12 accepted an operation before initialization\n", stderr );
		return 1;
	}

	if ( !device.initialize( true, transferConfig ) ) {
		fprintf( stderr, "D3D12 WARP device initialization failed: HRESULT 0x%08lx\n", ( unsigned long )device.getLastError() );
		return 1;
	}
	if ( !device.isReady() || device.initialize( true, transferConfig ) || !device.queryAdapterInfo( &adapter ) || !adapter.bSoftware ) {
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

	if ( !testOffscreenIndexedGraphics( device ) ) {
		fputs( "D3D12 offscreen graphics draw failed\n", stderr );
		return 1;
	}

	if ( !testCompiledDxilCompute( device ) ) {
		fputs( "D3D12 DXIL compute execution failed\n", stderr );
		return 1;
	}

	if ( !testAsyncCopyQueue( device ) ) {
		fputs( "D3D12 asynchronous copy queue failed\n", stderr );
		return 1;
	}

	if ( !testCopyToComputeGpuWait( device ) ) {
		fputs( "D3D12 GPU-side COPY to COMPUTE wait failed\n", stderr );
		return 1;
	}

	if ( !testComputeToDirectGpuWait( device ) ) {
		fputs( "D3D12 COMPUTE to DIRECT GPU wait failed\n", stderr );
		return 1;
	}

	if ( !testDeferredMultiQueueRetirement( device ) ) {
		fputs( "D3D12 multi-queue allocation retirement failed\n", stderr );
		return 1;
	}

	sirD3D12CompletionSet_t oldSet = {};
	if ( !testComputeAndCompletions( device, &oldSet ) ) {
		fputs( "D3D12 compute / multi-queue completion test failed\n", stderr );
		return 1;
	}

	device.shutdown();
	device.shutdown();
	if ( device.getRetiredAllocationCount() != 0 || device.isReady() ||
	     device.copyRoundTripForTest( szInput, szOutput, sizeof( szInput ) ) ) {
		fputs( "D3D12 accepted an operation after shutdown\n", stderr );
		return 1;
	}
	transferConfig.nSlotBytes = 4096;
	transferConfig.bPreferDirectMapped = false;
	if ( !device.initialize( true, transferConfig ) ) {
		fputs( "D3D12 WARP forced-staging reinitialization failed\n", stderr );
		return 1;
	}
	sirD3D12ResidentTransferView_t staged = {};
	if ( !device.getTransfers().queryResidentArena( &staged ) ||
	     staged.ePath != SIR_D3D12_TRANSFER_STAGED || staged.nSlotBytes != transferConfig.nSlotBytes ||
	     staged.pWriteResource == staged.pGpuResource ) {
		fputs( "D3D12 staging pool validation failed\n", stderr );
		return 1;
	}
	sirD3D12TransferTicket_t stageTicket = {};
	uint32_t szStagedOutput[32] = {};
	if ( !device.getTransfers().submit( szInput, sizeof( szInput ), &stageTicket ) ||
	     !device.getTransfers().wait( stageTicket, 15000 ) ||
	     !device.getTransfers().receive( stageTicket, szStagedOutput, sizeof( szStagedOutput ) ) ||
	     memcmp( szInput, szStagedOutput, sizeof( szInput ) ) != 0 ) {
		fputs( "D3D12 forced staged transfer failed\n", stderr );
		return 1;
	}
	D3D12MA::Allocation *pOldGeneration = nullptr;
	if ( !device.createNativeBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 256, &pOldGeneration ) ) {
		return 1;
	}
	if ( device.retireNativeAllocation( &pOldGeneration, oldSet ) || pOldGeneration == nullptr ) {
		if ( pOldGeneration != nullptr ) {
			pOldGeneration->Release();
		}
		return 1;
	}
	pOldGeneration->Release();

	sirD3D12CompletionSet_t fresh = {};
	bool bCompleted = false;
	if ( !device.snapshotCompletionSet( &fresh ) || fresh.nDeviceGeneration == oldSet.nDeviceGeneration ||
	     device.pollCompletionSet( oldSet, &bCompleted ) || !device.submitComputeMarker( &fresh ) ||
	     !device.getComputeQueue().waitForFence( fresh.nComputeFence, 15000 ) || !device.pollCompletionSet( fresh, &bCompleted ) ||
	     !bCompleted ) {
		fputs( "D3D12 device-generation invalidation failed\n", stderr );
		return 1;
	}
	device.shutdown();
	return 0;
}
