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

#include <Terrain/Integration/SIRTerrainResourceBridge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

#define CHECK( condition )                                                                                                                 \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d failure: %s\n", __FILE__, __LINE__, #condition );                                                      \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

typedef struct sirTerrainTestHeap_s
{
	uint64_t nOutstanding;
	bool bFailNext;
} sirTerrainTestHeap_t;

static void *sirTerrainTestAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	sirTerrainTestHeap_t *pHeap = ( sirTerrainTestHeap_t * )pContext;
	if ( pHeap->bFailNext || nBytes == 0 || nBytes > SIZE_MAX ) {
		pHeap->bFailNext = false;
		return nullptr;
	}
	const size_t nAlign = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	void *pData = nullptr;
#ifdef _WIN32
	pData = _aligned_malloc( ( size_t )nBytes, nAlign );
#else
	if ( posix_memalign( &pData, nAlign, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pData ) {
		pHeap->nOutstanding += nBytes;
	}
	return pData;
}

static void sirTerrainTestFree( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nAlignment;
	( void )nDomain;
	sirTerrainTestHeap_t *pHeap = ( sirTerrainTestHeap_t * )pContext;
	pHeap->nOutstanding -= nBytes;
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

int main( void )
{
	sirTerrainTestHeap_t heap = {};
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &heap, sirTerrainTestAllocate, sirTerrainTestFree };
	config.m_nMetadataDomain = 7;
	config.m_nMaxResources = 16;
	config.m_nMaxStaging = 8;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 1024 * 1024;
	{
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		sirTerrainRegionDescriptor_t descriptor = { -8, 9, 2, -10.0f, 10.0f, 1 };
		float szSamples[4] = { -10.0f, 0.0f, 5.0f, 10.0f };
		std::vector<uint8_t> cooked( SIRTerrainHeightfieldCodec::getEncodedByteCount( 2 ) );
		CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szSamples, 4 }, { cooked.data(), cooked.size() } ) ==
		       SIR_TERRAIN_CODEC_SUCCESS );
		const sirTerrainRegionIdentity_t identity = { UINT64_C( 0x1122334455667788 ), 0, -8, 9 };
		const SIRResourceKey key = SIRTerrainResourceBridge::getKey( identity );
		CHECK( key.m_nAssetId == identity.nAssetId && key.m_nVariantId == 0 );
		const sirTerrainRegionIdentity_t otherWorld = { UINT64_C( 0x8899aabbccddeeff ), 0, -8, 9 };
		CHECK( SIRTerrainResourceBridge::getKey( otherWorld ).m_nAssetId != key.m_nAssetId );
		sirTerrainRegionPublishRequest_t request = { identity, 7, 0, 1 };
		SIRResourceHandle handle = {};
		CHECK( SIRTerrainResourceBridge::publish( graph, { nullptr, 0 }, request, handle ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		std::vector<uint8_t> bad = cooked;
		bad[66] ^= 0x10;
		CHECK( SIRTerrainResourceBridge::publish( graph, { bad.data(), bad.size() }, request, handle ) ==
		       SIR_RESOURCE_RESULT_INVALID_FORMAT );
		CHECK( graph.getStats().m_nLiveResources == 0 );
		CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
		       SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( graph.getStats().m_nLiveResources == 1 );
		CHECK( graph.getStats().m_nUniqueChunks == 1 );
		{
			SIRResourceReadScope scope( graph );
			CHECK( scope.isActive() );
			sirTerrainR16View_t oldView = {};
			CHECK( SIRTerrainResourceBridge::open( graph, scope, identity, oldView ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( SIRTerrainResourceBridge::open( graph, scope, { identity.nAssetId, 0, -7, 9 }, oldView ) ==
			       SIR_RESOURCE_RESULT_INVALID_FORMAT );
			CHECK( oldView.descriptor.nRevision == 1 && oldView.descriptor.iRegionX == -8 );
			float fHeight = 0;
			CHECK( SIRTerrainHeightfieldView::read( oldView, 0, 0, fHeight ) == SIR_TERRAIN_SAMPLE_SUCCESS && fHeight == -10.0f );
			CHECK( SIRTerrainResourceBridge::open( graph, scope, otherWorld, oldView ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			CHECK( SIRTerrainResourceBridge::open( graph, scope, { 0, 0, -8, 9 }, oldView ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
			       SIR_RESOURCE_RESULT_STALE_REVISION );
			request.nExpectedGraphRevision = 1;
			CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
			       SIR_RESOURCE_RESULT_STALE_REVISION );
			descriptor.nRevision = 2;
			szSamples[0] = 10.0f;
			CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szSamples, 4 }, { cooked.data(), cooked.size() } ) ==
			       SIR_TERRAIN_CODEC_SUCCESS );
			request.nMinimumTerrainRevision = 3;
			CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
			       SIR_RESOURCE_RESULT_STALE_REVISION );
			request.nMinimumTerrainRevision = 2;
			CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( SIRTerrainHeightfieldView::read( oldView, 0, 0, fHeight ) == SIR_TERRAIN_SAMPLE_SUCCESS && fHeight == -10.0f );
			sirTerrainR16View_t currentView = {};
			CHECK( SIRTerrainResourceBridge::open( graph, scope, identity, currentView ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( currentView.descriptor.nRevision == 2 );
			CHECK( SIRTerrainHeightfieldView::read( currentView, 0, 0, fHeight ) == SIR_TERRAIN_SAMPLE_SUCCESS && fHeight == 10.0f );
		}
		CHECK( graph.getStats().m_nLiveResources == 1 );
		graph.getWriter()->collectRetired( 16 );
		request.nExpectedGraphRevision = 1;
		CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
		       SIR_RESOURCE_RESULT_STALE_REVISION );
		descriptor.nRevision = 3;
		CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szSamples, 4 }, { cooked.data(), cooked.size() } ) ==
		       SIR_TERRAIN_CODEC_SUCCESS );
		request.nExpectedGraphRevision = 2;
		heap.bFailNext = true;
		CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
		       SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		CHECK( SIRTerrainResourceBridge::publish( graph, { cooked.data(), cooked.size() }, request, handle ) ==
		       SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( graph.getStats().m_nLiveResources == 1 );
		CHECK( graph.getWriter()->retire( handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		graph.getWriter()->collectRetired( 16 );
	}
	CHECK( heap.nOutstanding == 0 );
	return 0;
}
