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

#include <Resources/SIRResourceMemoryAdapter.h>

#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/SIRGeneralHeap.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition );                                                             \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

class SIRResourceTestDomain
{
public:
	SIRCompactBucketAllocator m_Small;
	SIRFreeListAllocator m_Medium;
	SIRBuddyAllocator m_Large;
	SIRGeneralHeap m_Heap;

	bool init( SIRMemoryManager &rMemory, sirMemoryDomain_t eDomain, const char *szSmall, const char *szMedium, const char *szLarge )
	{
		const uint64_t nPage = SIRVirtualMemory::getPageSize();
		SIRMemorySegment *pSmall = rMemory.createSegment( szSmall, nPage * 256, 0, eDomain );
		SIRMemorySegment *pMedium = rMemory.createSegment( szMedium, nPage * 256, 0, eDomain );
		SIRMemorySegment *pLarge = rMemory.createSegment( szLarge, nPage * 1024, 0, eDomain );
		if ( pSmall == nullptr || pMedium == nullptr || pLarge == nullptr ) {
			return false;
		}
		const sirCompactBucketConfig_t bucketConfig = { 256, 16, 16384, 2, 0, 0 };
		if ( !m_Small.init( pSmall, bucketConfig ) || !m_Medium.init( pMedium ) || !m_Large.init( pLarge ) ) {
			return false;
		}
		const sirGeneralHeapConfig_t heapConfig = { &m_Small, &m_Medium, nullptr, &m_Large, pSmall, pMedium, pLarge, 256, 4096, false };
		return m_Heap.init( heapConfig );
	}

	bool shutdown( bool bAttached )
	{
		return ( !bAttached || m_Heap.detachThread() ) && m_Heap.shutdown() && m_Small.shutdown() && m_Medium.shutdown();
	}
};

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage != 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t memoryConfig = { nPage * 8192, nPage * 6144, nPage * 768, 16 };
	REQUIRE( memory.init( memoryConfig ) );
	SIRResourceTestDomain resident;
	SIRResourceTestDomain streaming;
	REQUIRE( resident.init( memory, SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT, "grdg.meta.small", "grdg.meta.medium", "grdg.meta.large" ) );
	REQUIRE( streaming.init( memory, SIR_MEMORY_DOMAIN_STREAMING, "grdg.io.small", "grdg.io.medium", "grdg.io.large" ) );
	// Compact bucket bindings are per-thread; the streaming heap uses the
	// synchronized medium-heap fallback while the resident heap is attached.
	REQUIRE( resident.m_Heap.attachThread() );

	SIRResourceMemoryAdapter adapter;
	REQUIRE( adapter.bindDomain( SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT, &resident.m_Heap ) );
	REQUIRE( adapter.bindDomain( SIR_MEMORY_DOMAIN_STREAMING, &streaming.m_Heap ) );
	REQUIRE( !adapter.bindDomain( SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT, &resident.m_Heap ) );
	REQUIRE( !adapter.bindDomain( SIR_MEMORY_DOMAIN_AUDIO, &resident.m_Heap ) );

	SIRResourceGraphConfig graphConfig = {};
	graphConfig.m_Allocator = adapter.getAllocator();
	graphConfig.m_nMetadataDomain = SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT;
	graphConfig.m_nMaxResources = 32;
	graphConfig.m_nMaxStaging = 8;
	graphConfig.m_nMaxProviderPins = 8;
	graphConfig.m_nMaxCommittedBytes = 1024 * 1024;
	{
		SIRResourceGraph graph( graphConfig );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourceStagingHandle missing = {};
		SIRResourceMutableBytes missingBytes = {};
		REQUIRE( pWriter->allocateStaging( 1024, 16, SIR_MEMORY_DOMAIN_AUDIO, &missing, &missingBytes ) ==
		         SIR_RESOURCE_RESULT_OUT_OF_BUDGET );

		SIRResourceStagingHandle staging = {};
		SIRResourceMutableBytes payload = {};
		REQUIRE( pWriter->allocateStaging( 8192, 64, SIR_MEMORY_DOMAIN_STREAMING, &staging, &payload ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( streaming.m_Heap.owns( payload.m_pData ) );
		REQUIRE( !resident.m_Heap.owns( payload.m_pData ) );
		memset( payload.m_pData, 0x6d, ( size_t )payload.m_nSize );
		SIRResourceChunkPublication chunk = { { 0 }, { 0x51, 0x97, 4, 0 }, staging, false };
		SIRResourcePublication publication = { { 1234, 17, 0 }, SIR_RESOURCE_KIND_BLOB, 1, 4, &chunk, 1, nullptr, 0 };
		SIRResourceHandle handle = {};
		REQUIRE( pWriter->publish( publication, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
		REQUIRE( memory.getDomainStats( SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT ).nCommittedBytes > 0 );
		REQUIRE( memory.getDomainStats( SIR_MEMORY_DOMAIN_STREAMING ).nCommittedBytes > 0 );

		SIRResourcePinHandle pin = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceConstBytes bytes = {};
			REQUIRE( view.getChunk( { 0 }, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( bytes.m_nSize == 8192 && bytes.m_pData[8191] == 0x6d );
			REQUIRE( graph.pinForProvider( scope, view, 7, &pin ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( pWriter->retire( handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 0 );
		REQUIRE( graph.releaseProviderPin( pin ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 0 );
	}
	REQUIRE( resident.m_Small.getLiveCount() == 0 && resident.m_Medium.getAllocationCount() == 0 &&
	         resident.m_Large.getAllocationCount() == 0 );
	REQUIRE( streaming.m_Small.getLiveCount() == 0 && streaming.m_Medium.getAllocationCount() == 0 &&
	         streaming.m_Large.getAllocationCount() == 0 );
	REQUIRE( streaming.shutdown( false ) && resident.shutdown( true ) );
	puts( "PASS: GRDG physical domain-backed allocation and retirement" );
	return 0;
}
