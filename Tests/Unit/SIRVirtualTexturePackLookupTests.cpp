/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D
contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackLookup.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

template <typename T>
static void sirVTSetTestRecord( sirVirtualTexturePackView_t *pView, sirVirtualTexturePackDirectory_t eDirectory, const T& record,
                                uint8_t *pBytes ) noexcept
{
	memcpy( pBytes + 1, &record, sizeof( record ) );
	pView->szDirectories[eDirectory].pData = pBytes + 1;
	pView->szDirectories[eDirectory].nBytes = sizeof( record );
}

int main( void )
{
	sirVirtualTexturePackView_t view = {};
	view.bMetadataOnly = true;
	view.header.nAllocationCount = 1;
	view.header.nChannelAllocationCount = SIR_VT_PACK_CHANNEL_COUNT;
	view.header.nPayloadCount = 1;
	view.header.nClusterCount = 1;
	view.header.nTailCount = 1;
	view.header.nQualityViewCount = 1;
	view.header.nFormatFamilyCount = SIR_VT_PACK_CHANNEL_COUNT;

	uint8_t szAllocation[sizeof( sirVirtualTexturePackAllocationRecord_t ) + 1] = {};
	uint8_t szKey[sizeof( sirVirtualTexturePackAllocationKey_t ) + 1] = {};
	uint8_t szChannel[sizeof( sirVirtualTexturePackChannelAllocation_t ) + 1] = {};
	uint8_t szPayload[sizeof( sirVirtualTexturePackPayloadRecord_t ) + 1] = {};
	uint8_t szCluster[sizeof( sirVirtualTexturePackClusterRecord_t ) + 1] = {};
	uint8_t szTail[sizeof( sirVirtualTexturePackTailRecord_t ) + 1] = {};
	uint8_t szQuality[sizeof( sirVirtualTexturePackQualityView_t ) + 1] = {};
	uint8_t szFamily[sizeof( sirVirtualTexturePackFamilyRecord_t ) + 1] = {};

	sirVirtualTexturePackAllocationRecord_t allocation = {};
	allocation.nGeneration = 91;
	sirVirtualTexturePackAllocationKey_t key = {};
	key.nStableKey = 123;
	sirVirtualTexturePackChannelAllocation_t channel = {};
	channel.nTailIndex = 5;
	sirVirtualTexturePackPayloadRecord_t payload = {};
	payload.nClusterId = 17;
	sirVirtualTexturePackClusterRecord_t cluster = {};
	cluster.nDecodedBytes = 4096;
	sirVirtualTexturePackTailRecord_t tail = {};
	tail.nFirstMip = 3;
	sirVirtualTexturePackQualityView_t quality = {};
	quality.nProfile = 2;
	sirVirtualTexturePackFamilyRecord_t family = {};
	family.nSemantic = 4;

	sirVTSetTestRecord( &view, SIR_VT_PACK_ALLOCATIONS, allocation, szAllocation );
	sirVTSetTestRecord( &view, SIR_VT_PACK_ALLOCATION_KEYS, key, szKey );
	sirVTSetTestRecord( &view, SIR_VT_PACK_CHANNEL_ALLOCATIONS, channel, szChannel );
	sirVTSetTestRecord( &view, SIR_VT_PACK_PAYLOADS, payload, szPayload );
	sirVTSetTestRecord( &view, SIR_VT_PACK_CLUSTERS, cluster, szCluster );
	sirVTSetTestRecord( &view, SIR_VT_PACK_TAILS, tail, szTail );
	sirVTSetTestRecord( &view, SIR_VT_PACK_QUALITY_VIEWS, quality, szQuality );
	sirVTSetTestRecord( &view, SIR_VT_PACK_FAMILIES, family, szFamily );

	sirVirtualTexturePackAllocationRecord_t actualAllocation = {};
	sirVirtualTexturePackAllocationKey_t actualKey = {};
	sirVirtualTexturePackChannelAllocation_t actualChannel = {};
	sirVirtualTexturePackPayloadRecord_t actualPayload = {};
	sirVirtualTexturePackClusterRecord_t actualCluster = {};
	sirVirtualTexturePackTailRecord_t actualTail = {};
	sirVirtualTexturePackQualityView_t actualQuality = {};
	sirVirtualTexturePackFamilyRecord_t actualFamily = {};
	SIR_TEST( sirVTPackGetAllocation( view, 0, &actualAllocation ) && actualAllocation.nGeneration == 91 );
	SIR_TEST( sirVTPackGetAllocationKey( view, 0, &actualKey ) && actualKey.nStableKey == 123 );
	SIR_TEST( sirVTPackGetChannelAllocation( view, 0, 0, &actualChannel ) && actualChannel.nTailIndex == 5 );
	SIR_TEST( sirVTPackGetPayload( view, 0, &actualPayload ) && actualPayload.nClusterId == 17 );
	SIR_TEST( sirVTPackGetCluster( view, 0, &actualCluster ) && actualCluster.nDecodedBytes == 4096 );
	SIR_TEST( sirVTPackGetTail( view, 0, &actualTail ) && actualTail.nFirstMip == 3 );
	SIR_TEST( sirVTPackGetQualityView( view, 0, &actualQuality ) && actualQuality.nProfile == 2 );
	SIR_TEST( sirVTPackGetFamily( view, 0, &actualFamily ) && actualFamily.nSemantic == 4 );

	SIR_TEST( !sirVTPackGetPayload( view, 1, &actualPayload ) );
	SIR_TEST( !sirVTPackGetPayload( view, 0, nullptr ) );
	SIR_TEST( !sirVTPackGetCluster( view, 1, &actualCluster ) );
	SIR_TEST( !sirVTPackGetChannelAllocation( view, 1, 0, &actualChannel ) );
	SIR_TEST( !sirVTPackGetChannelAllocation( view, 0, SIR_VT_PACK_CHANNEL_COUNT, &actualChannel ) );
	SIR_TEST( !sirVTPackGetAllocation( view, 1, &actualAllocation ) );
	SIR_TEST( !sirVTPackGetTail( view, 1, &actualTail ) );
	SIR_TEST( !sirVTPackGetQualityView( view, 1, &actualQuality ) );
	SIR_TEST( !sirVTPackGetFamily( view, SIR_VT_PACK_CHANNEL_COUNT, &actualFamily ) );
	view.szDirectories[SIR_VT_PACK_PAYLOADS].nBytes--;
	SIR_TEST( !sirVTPackGetPayload( view, 0, &actualPayload ) );
	view.szDirectories[SIR_VT_PACK_CLUSTERS].pData = nullptr;
	SIR_TEST( !sirVTPackGetCluster( view, 0, &actualCluster ) );
	SIR_TEST( view.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].pData == nullptr );
	return 0;
}
