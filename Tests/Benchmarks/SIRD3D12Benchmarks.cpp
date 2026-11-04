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

#include <Renderer/Drivers/D3D12/SIRD3D12Driver.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Measurements never assume a hosted WARP runner has hardware-GPU throughput.
// Each case warms its own resource/queue path; GPU timestamps exclude CPU waits.
typedef struct sirD3D12BenchmarkStats_s
{
	double fMeanNs;
	double fP50Ns;
	double fP95Ns;
	double fP99Ns;
	double fMinimumNs;
	double fMaximumNs;
} sirD3D12BenchmarkStats_t;

typedef struct sirD3D12BenchmarkResults_s
{
	sirD3D12BenchmarkStats_t createBuffer;
	sirD3D12BenchmarkStats_t submitEmpty;
	sirD3D12BenchmarkStats_t submitAndWait;
	sirD3D12BenchmarkStats_t copyRoundtrip;
	sirD3D12BenchmarkStats_t gpuCopy;
	sirGraphicsMemoryBudget_t memory;
	uint64_t nTimestampFrequency;
	uint32_t nSamples;
	uint32_t nWarmup;
	uint32_t nCopiesPerGpuSample;
	uint32_t nGpuCopyBytes;
	bool bGpuTimestampAvailable;
} sirD3D12BenchmarkResults_t;

static double s_szSamples0[4096] = {};
static double s_szSamples1[4096] = {};
static double s_szSamples2[4096] = {};
static double s_szSamples3[4096] = {};

static int sirCompareBenchDouble( const void *pA, const void *pB )
{
	const double fA = *( const double * )pA;
	const double fB = *( const double * )pB;
	return fA < fB ? -1 : ( fA > fB ? 1 : 0 );
}

static bool sirBenchStatistics( double *pSortedSamples, uint32_t nSamples, sirD3D12BenchmarkStats_t *pStats ) noexcept
{
	if ( pSortedSamples == nullptr || pStats == nullptr || nSamples == 0 ) {
		return false;
	}
	qsort( pSortedSamples, nSamples, sizeof( double ), sirCompareBenchDouble );
	*pStats = {};
	pStats->fMinimumNs = pSortedSamples[0];
	pStats->fMaximumNs = pSortedSamples[nSamples - 1];
	pStats->fP50Ns = pSortedSamples[( ( uint64_t )nSamples * 50 + 99 ) / 100 - 1];


	pStats->fP95Ns = pSortedSamples[( ( uint64_t )nSamples * 95 + 99 ) / 100 - 1];
	pStats->fP99Ns = pSortedSamples[( ( uint64_t )nSamples * 99 + 99 ) / 100 - 1];
	for ( uint32_t iSample = 0; iSample < nSamples; ++iSample ) {
		if ( !isfinite( pSortedSamples[iSample] ) || pSortedSamples[iSample] < 0.0 ) {
			return false;
		}
		pStats->fMeanNs += pSortedSamples[iSample];
	}
	pStats->fMeanNs /= ( double )nSamples;
	return true;
}

static double sirBenchNanoseconds( LARGE_INTEGER nStart, LARGE_INTEGER nEnd, LARGE_INTEGER nFrequency ) noexcept
{
	return ( double )( nEnd.QuadPart - nStart.QuadPart ) * 1000000000.0 / ( double )nFrequency.QuadPart;
}

static bool sirBenchAdapter( SIRD3D12Driver &driver, bool bHardware, uint32_t *pIndex, sirGraphicsAdapterInfo_t *pInfo ) noexcept
{
	uint32_t nAdapters = 0;
	if ( pIndex == nullptr || pInfo == nullptr || driver.getAdapterCount( &nAdapters ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t i = 0; i < nAdapters; ++i ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( i, &info ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		if ( info.bSoftwareAdapter != bHardware ) {
			continue;
		}
		*pIndex = i;
		*pInfo = info;
		return true;
	}
	return false;
}

static bool sirBenchCpuCases( SIRRenderingDevice *pDevice, uint32_t nWarmup, uint32_t nIterations, LARGE_INTEGER nFrequency,
                              sirD3D12BenchmarkResults_t *pResults ) noexcept
{
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = pDevice->getDeviceGeneration();
	sirGraphicsBufferDesc_t transient = {};
	transient.nSizeBytes = 4096;
	transient.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	transient.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	const uint32_t nTotal = nIterations + nWarmup;
	for ( uint32_t i = 0; i < nTotal; ++i ) {
		LARGE_INTEGER nStart = {};
		LARGE_INTEGER nEnd = {};
		sirGraphicsHandle_t hBuffer = {};
		QueryPerformanceCounter( &nStart );
		if ( pDevice->createBuffer( transient, &hBuffer ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		QueryPerformanceCounter( &nEnd );
		if ( pDevice->retireResource( hBuffer, immediate ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		if ( i >= nWarmup ) {
			s_szSamples0[i - nWarmup] = sirBenchNanoseconds( nStart, nEnd, nFrequency );
		}
	}
	if ( !sirBenchStatistics( s_szSamples0, nIterations, &pResults->createBuffer ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < nTotal; ++i ) {
		SIRGraphicsCommandList *pCommand = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommand ) != SIR_GRAPHICS_SUCCESS || pCommand == nullptr ||
		     pCommand->close() != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		sirGraphicsSubmission_t submission = {};
		submission.ppCommandLists = &pCommand;
		submission.nCommandListCount = 1;
		sirGraphicsCompletion_t completion = {};
		LARGE_INTEGER nStart = {};
		LARGE_INTEGER nSubmitted = {};
		LARGE_INTEGER nEnd = {};
		QueryPerformanceCounter( &nStart );
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &completion ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		QueryPerformanceCounter( &nSubmitted );
		if ( pDevice->wait( completion, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		QueryPerformanceCounter( &nEnd );
		if ( i >= nWarmup ) {
			s_szSamples0[i - nWarmup] = sirBenchNanoseconds( nStart, nSubmitted, nFrequency );
			s_szSamples1[i - nWarmup] = sirBenchNanoseconds( nStart, nEnd, nFrequency );
		}
	}
	if ( !sirBenchStatistics( s_szSamples0, nIterations, &pResults->submitEmpty ) ||
	     !sirBenchStatistics( s_szSamples1, nIterations, &pResults->submitAndWait ) ) {
		return false;
	}
	constexpr uint32_t BUFFER_BYTES = 256U * 1024U;
	sirGraphicsBufferDesc_t transfer = {};
	transfer.nSizeBytes = BUFFER_BYTES;
	transfer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	transfer.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t hUpload = {};
	if ( pDevice->createBuffer( transfer, &hUpload ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	transfer.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	transfer.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( transfer, &hReadback ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsMappedBuffer_t upload = {};
	sirGraphicsMappedBuffer_t readback = {};
	if ( pDevice->mapBuffer( hUpload, 0, BUFFER_BYTES, &upload ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->mapBuffer( hReadback, 0, BUFFER_BYTES, &readback ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	memset( upload.pData, 0x57, BUFFER_BYTES );
	for ( uint32_t i = 0; i < nTotal; ++i ) {
		SIRGraphicsCommandList *pCommand = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pCommand ) != SIR_GRAPHICS_SUCCESS || pCommand == nullptr ||
		     pCommand->copyBuffer( hReadback, 0, hUpload, 0, BUFFER_BYTES ) != SIR_GRAPHICS_SUCCESS ||
		     pCommand->close() != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		sirGraphicsSubmission_t submission = {};
		submission.ppCommandLists = &pCommand;
		submission.nCommandListCount = 1;
		sirGraphicsCompletion_t completion = {};
		LARGE_INTEGER nStart = {};
		LARGE_INTEGER nEnd = {};
		QueryPerformanceCounter( &nStart );
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_COPY, submission, &completion ) != SIR_GRAPHICS_SUCCESS ||
		     pDevice->wait( completion, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		QueryPerformanceCounter( &nEnd );
		if ( i >= nWarmup ) {
			s_szSamples0[i - nWarmup] = sirBenchNanoseconds( nStart, nEnd, nFrequency );
		}
	}
	if ( memcmp( upload.pData, readback.pData, BUFFER_BYTES ) != 0 ||
	     !sirBenchStatistics( s_szSamples0, nIterations, &pResults->copyRoundtrip ) ) {
		return false;
	}
	if ( pDevice->retireResource( hUpload, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReadback, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return pDevice->queryMemoryBudget( &pResults->memory ) == SIR_GRAPHICS_SUCCESS;
}

// Native timestamp path measures GPU copy commands using the D3D12 queue
// frequency, never QueryPerformanceCounter or CPU fence-wait elapsed time.
static bool sirBenchGpuTimestamps( bool bWarp, sirD3D12BenchmarkResults_t *pResults ) noexcept
{
	constexpr uint32_t SAMPLES = 8;
	constexpr uint32_t COPIES_PER_SAMPLE = 32;
	constexpr uint32_t COPY_BYTES = 64 * 1024;
	pResults->nCopiesPerGpuSample = COPIES_PER_SAMPLE;
	pResults->nGpuCopyBytes = COPY_BYTES;
	sirD3D12TransferPoolConfig_t config = {};
	config.nSlotBytes = 64 * 1024;
	SIRD3D12Device device;
	if ( !device.initialize( bWarp, config ) ) {
		return false;
	}
	ID3D12Device *pNative = device.getNativeDevice();
	ID3D12CommandQueue *pQueue = device.getDirectQueue();
	SIRD3D12Memory *pMemory = device.getMemory();
	if ( pNative == nullptr || pQueue == nullptr || pMemory == nullptr ||
	     FAILED( pQueue->GetTimestampFrequency( &pResults->nTimestampFrequency ) ) || pResults->nTimestampFrequency == 0 ) {
		device.shutdown();
		return true;
	}
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> pQueries;
	D3D12_QUERY_HEAP_DESC query = {};
	query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	query.Count = SAMPLES * 2;
	if ( FAILED( pNative->CreateQueryHeap( &query, IID_PPV_ARGS( pQueries.GetAddressOf() ) ) ) ) {
		device.shutdown();
		return true;
	}
	D3D12MA::Allocation *pUpload = nullptr;
	D3D12MA::Allocation *pReadback = nullptr;
	D3D12MA::Allocation *pStamps = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommands;
	bool bSuccess = false;
	bool bSubmissionInFlight = false;
	if ( FAILED( pMemory->createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, COPY_BYTES, &pUpload ) ) ||
	     FAILED( pMemory->createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, COPY_BYTES, &pReadback ) ) ||
	     FAILED( pMemory->createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, SAMPLES * 2 * sizeof( uint64_t ),
	                                    &pStamps ) ) ||
	     FAILED( pNative->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( pAllocator.GetAddressOf() ) ) ) ||
	     FAILED( pNative->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator.Get(), nullptr,
	                                         IID_PPV_ARGS( pCommands.GetAddressOf() ) ) ) ) {
		goto cleanup;
	}
	{
		D3D12_RANGE noRead = { 0, 0 };
		void *pBytes = nullptr;
		if ( FAILED( pUpload->GetResource()->Map( 0, &noRead, &pBytes ) ) || pBytes == nullptr ) {
			goto cleanup;
		}
		memset( pBytes, 0x6D, COPY_BYTES );
		pUpload->GetResource()->Unmap( 0, nullptr );
	}
	for ( uint32_t iSample = 0; iSample < SAMPLES; ++iSample ) {
		pCommands->EndQuery( pQueries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, iSample * 2 );
		for ( uint32_t iCopy = 0; iCopy < COPIES_PER_SAMPLE; ++iCopy ) {
			pCommands->CopyBufferRegion( pReadback->GetResource(), 0, pUpload->GetResource(), 0, COPY_BYTES );
		}
		pCommands->EndQuery( pQueries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, iSample * 2 + 1 );
	}
	pCommands->ResolveQueryData( pQueries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, SAMPLES * 2, pStamps->GetResource(), 0 );
	if ( FAILED( pCommands->Close() ) ) {
		goto cleanup;
	}
	{
		ID3D12CommandList *ppLists[] = { pCommands.Get() };
		sirGraphicsCompletion_t done = {};
		if ( device.submitRecorded( SIR_GRAPHICS_QUEUE_DIRECT, ppLists, 1, nullptr, 0, &done ) != SIR_GRAPHICS_SUCCESS ) {
			goto cleanup;
		}
		bSubmissionInFlight = true;
		if ( !device.waitForGraphicsFence( done.nValue, 10000 ) ) {
			goto cleanup;
		}
		bSubmissionInFlight = false;
	}
	{
		D3D12_RANGE reading = { 0, SAMPLES * 2 * sizeof( uint64_t ) };
		const uint64_t *pTicks = nullptr;
		if ( FAILED( pStamps->GetResource()->Map( 0, &reading, ( void ** )&pTicks ) ) || pTicks == nullptr ) {
			goto cleanup;
		}
		for ( uint32_t iSample = 0; iSample < SAMPLES; ++iSample ) {
			const uint64_t nStart = pTicks[iSample * 2];
			const uint64_t nEnd = pTicks[iSample * 2 + 1];
			s_szSamples0[iSample] =
			    nEnd > nStart ? ( double )( nEnd - nStart ) * 1000000000.0 / ( double )pResults->nTimestampFrequency : 0.0;
		}
		D3D12_RANGE noWrite = { 0, 0 };
		pStamps->GetResource()->Unmap( 0, &noWrite );
	}
	{
		D3D12_RANGE reading = { 0, COPY_BYTES };
		const uint8_t *pBytes = nullptr;
		if ( FAILED( pReadback->GetResource()->Map( 0, &reading, ( void ** )&pBytes ) ) || pBytes == nullptr ) {
			goto cleanup;
		}
		bool bMatches = true;
		for ( uint32_t i = 0; i < COPY_BYTES; ++i ) {
			if ( pBytes[i] != 0x6D ) {
				bMatches = false;
				break;
			}
		}
		D3D12_RANGE noWrite = { 0, 0 };
		pReadback->GetResource()->Unmap( 0, &noWrite );
		if ( !bMatches ) {
			goto cleanup;
		}
	}
	bSuccess = sirBenchStatistics( s_szSamples0, SAMPLES, &pResults->gpuCopy );
	pResults->bGpuTimestampAvailable = bSuccess && pResults->gpuCopy.fMinimumNs > 0.0;

cleanup:
	if ( bSubmissionInFlight ) {
		// An unproved fence cannot authorize command/resource destruction.
		( void )pCommands.Detach();
		( void )pAllocator.Detach();
		( void )pQueries.Detach();
		pStamps = nullptr;
		pReadback = nullptr;
		pUpload = nullptr;
		device.shutdown();
		return false;
	}
	pCommands.Reset();
	pAllocator.Reset();
	pQueries.Reset();
	if ( pStamps != nullptr ) {
		pStamps->Release();
	}
	if ( pReadback != nullptr ) {
		pReadback->Release();
	}
	if ( pUpload != nullptr ) {
		pUpload->Release();
	}
	device.shutdown();
	return bSuccess;
}

static void sirBenchWriteStats( FILE *pFile, const char *pszName, const sirD3D12BenchmarkStats_t &s, bool bTrailingComma ) noexcept
{
	fprintf( pFile,
	         "    \"%s\": {\"mean_ns\": %.3f, \"p50_ns\": %.3f, \"p95_ns\": %.3f, \"p99_ns\": %.3f, "
	         "\"min_ns\": %.3f, \"max_ns\": %.3f}%s\\n",
	         pszName, s.fMeanNs, s.fP50Ns, s.fP95Ns, s.fP99Ns, s.fMinimumNs, s.fMaximumNs, bTrailingComma ? "," : "" );
}

static void sirBenchWriteJsonString( FILE *pFile, const char *pszValue ) noexcept
{
	fputc( '"', pFile );
	for ( const unsigned char *p = ( const unsigned char * )pszValue; *p != 0; ++p ) {
		if ( *p == '"' || *p == '\\' ) {
			fputc( '\\', pFile );
			fputc( *p, pFile );
		} else if ( *p < 0x20 ) {
			fprintf( pFile, "\\u%04x", ( unsigned )*p );
		} else {
			fputc( *p, pFile );
		}
	}
	fputc( '"', pFile );
}

static void sirBenchWriteJson( FILE *pFile, const sirGraphicsAdapterInfo_t &adapter, bool bUma,
                               const sirD3D12BenchmarkResults_t &results ) noexcept
{
	fprintf( pFile, "{\\n  \"schema_version\": 1,\\n  \"backend\": \"d3d12\",\\n" );
	fprintf( pFile, "  \"adapter\": {\"name\": " );
	sirBenchWriteJsonString( pFile, adapter.szName );
	fprintf( pFile,
	         ", \"vendor_id\": %u, \"device_id\": %u, \"software\": %s, "
	         "\"uma\": %s, \"dedicated_video_bytes\": %llu, \"shared_system_bytes\": %llu},\\n",
	         adapter.nVendorId, adapter.nDeviceId, adapter.bSoftwareAdapter ? "true" : "false", bUma ? "true" : "false",
	         ( unsigned long long )adapter.nDedicatedVideoBytes, ( unsigned long long )adapter.nSharedSystemBytes );
	fprintf( pFile, "  \"warmup\": %u, \"iterations\": %u,\\n", results.nWarmup, results.nSamples );
	fprintf( pFile,
	         "  \"memory\": {\"budget_bytes\": %llu, \"committed_bytes\": %llu, "
	         "\"peak_committed_bytes\": %llu, \"resident_bytes\": %llu},\\n",
	         ( unsigned long long )results.memory.nBudgetBytes, ( unsigned long long )results.memory.nCommittedBytes,
	         ( unsigned long long )results.memory.nPeakCommittedBytes, ( unsigned long long )results.memory.nResidentBytes );
	fprintf( pFile, "  \"cpu\": {\\n" );
	sirBenchWriteStats( pFile, "buffer_create", results.createBuffer, true );
	sirBenchWriteStats( pFile, "submit_empty", results.submitEmpty, true );
	sirBenchWriteStats( pFile, "submit_and_wait", results.submitAndWait, true );
	sirBenchWriteStats( pFile, "copy_256k_roundtrip", results.copyRoundtrip, false );
	fprintf( pFile,
	         "  },\\n  \"gpu_timestamp\": {\"available\": %s, \"frequency_hz\": %llu, "
	         "\"copies_per_sample\": %u, \"bytes_per_copy\": %u,\\n",
	         results.bGpuTimestampAvailable ? "true" : "false", ( unsigned long long )results.nTimestampFrequency,
	         results.nCopiesPerGpuSample, results.nGpuCopyBytes );
	sirBenchWriteStats( pFile, "copy_gpu", results.gpuCopy, false );
	fprintf( pFile, "  }\\n}\\n" );
}

int main( int argc, char **argv )
{
	bool bHardware = false;
	uint32_t nIterations = 96;
	uint32_t nWarmup = 16;
	const char *pszOutput = nullptr;
	for ( int iArg = 1; iArg < argc; ++iArg ) {
		if ( strcmp( argv[iArg], "--adapter" ) == 0 && iArg + 1 < argc ) {
			const char *pszAdapter = argv[++iArg];
			if ( strcmp( pszAdapter, "hardware" ) == 0 ) {
				bHardware = true;
			} else if ( strcmp( pszAdapter, "warp" ) != 0 ) {
				return 2;
			}
		} else if ( strcmp( argv[iArg], "--iterations" ) == 0 && iArg + 1 < argc ) {
			const unsigned long nValue = strtoul( argv[++iArg], nullptr, 10 );
			if ( nValue == 0 || nValue > 4000 ) {
				return 3;
			}
			nIterations = ( uint32_t )nValue;
		} else if ( strcmp( argv[iArg], "--warmup" ) == 0 && iArg + 1 < argc ) {
			const unsigned long nValue = strtoul( argv[++iArg], nullptr, 10 );
			if ( nValue > 96 ) {
				return 4;
			}
			nWarmup = ( uint32_t )nValue;
		} else if ( strcmp( argv[iArg], "--output" ) == 0 && iArg + 1 < argc ) {
			pszOutput = argv[++iArg];
		} else {
			return 5;
		}
	}
	if ( nIterations > 4096 - nWarmup ) {
		return 6;
	}
	LARGE_INTEGER nFrequency = {};
	if ( !QueryPerformanceFrequency( &nFrequency ) || nFrequency.QuadPart <= 0 ) {
		return 7;
	}
	SIRD3D12Driver driver;
	sirGraphicsAdapterInfo_t adapter = {};
	uint32_t nAdapterIndex = 0;
	if ( !sirBenchAdapter( driver, bHardware, &nAdapterIndex, &adapter ) ) {
		fprintf( stderr, "Requested %s adapter unavailable\\n", bHardware ? "hardware" : "WARP" );
		return 8;
	}
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nAdapterIndex;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = 3 * 64 * 1024;
	config.bHeadless = true;
	config.bAllowSoftware = !bHardware;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 9;
	}
	bool bUma = false;
	sirGraphicsCapabilities_t capability = {};
	if ( pDevice->queryCapabilities( &capability ) == SIR_GRAPHICS_SUCCESS ) {
		bUma = capability.bUnifiedMemory;
	}
	sirD3D12BenchmarkResults_t results = {};
	results.nSamples = nIterations;
	results.nWarmup = nWarmup;
	const bool bCpuOkay = sirBenchCpuCases( pDevice, nWarmup, nIterations, nFrequency, &results );
	const sirGraphicsResult_t eShutdown = driver.destroyDevice( pDevice );
	if ( !bCpuOkay || eShutdown != SIR_GRAPHICS_SUCCESS ) {
		fprintf( stderr, "CPU D3D12 performance benchmark failed\\n" );
		return 10;
	}
	if ( !sirBenchGpuTimestamps( !bHardware, &results ) ) {
		fprintf( stderr, "GPU timestamp copy workload failed\\n" );
		return 11;
	}
	FILE *pOutput = stdout;
	if ( pszOutput != nullptr && fopen_s( &pOutput, pszOutput, "wb" ) != 0 ) {
		return 12;
	}
	sirBenchWriteJson( pOutput, adapter, bUma, results );
	if ( pOutput != stdout ) {
		fclose( pOutput );
	}
	return 0;
}
