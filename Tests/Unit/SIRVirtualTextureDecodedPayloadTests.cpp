/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureDecodedPayload.h>

#include <stdio.h>
#include <string.h>
#include <vector>

#define SIR_TEST( expression )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "FAIL %s:%u: %s\n", __FILE__, ( unsigned )__LINE__, #expression );                                            \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	const uint32_t nPhysicalBytes = sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC1 );
	SIR_TEST( nPhysicalBytes == 9248 );
	std::vector<uint8_t> decoded( nPhysicalBytes + 16, 0x37 );
	uint8_t szPayloadRecord[sizeof( sirVirtualTexturePackPayloadRecord_t ) + 1] = {};
	uint8_t szClusterRecord[sizeof( sirVirtualTexturePackClusterRecord_t ) + 1] = {};
	sirVirtualTexturePackView_t view = {};
	view.bMetadataOnly = true;
	view.header.nPayloadCount = 1;
	view.header.nClusterCount = 1;
	view.szDirectories[SIR_VT_PACK_PAYLOADS] = { szPayloadRecord + 1, sizeof( sirVirtualTexturePackPayloadRecord_t ), 0 };
	view.szDirectories[SIR_VT_PACK_CLUSTERS] = { szClusterRecord + 1, sizeof( sirVirtualTexturePackClusterRecord_t ), 0 };
	sirVirtualTexturePackPayloadRecord_t payload = {};
	payload.nClusterId = 0;
	payload.nFormat = SIR_VT_PHYSICAL_BC1;
	payload.nClusterOffset = 16;
	payload.nStoredBytes = nPhysicalBytes;
	payload.nDecodedBytes = nPhysicalBytes;
	payload.nFlags = 1;
	payload.nChecksum = sirVTPackChecksum32( decoded.data() + 16, nPhysicalBytes );
	payload.nContentSignature = sirVTPackHash64( decoded.data() + 16, nPhysicalBytes );
	sirVirtualTexturePackClusterRecord_t cluster = {};
	cluster.nDecodedBytes = decoded.size();
	cluster.nFirstPayload = 0;
	cluster.nPayloadCount = 1;
	memcpy( szPayloadRecord + 1, &payload, sizeof( payload ) );
	memcpy( szClusterRecord + 1, &cluster, sizeof( cluster ) );
	const sirVirtualTextureDecodedCluster_t input = { decoded.data(), ( uint32_t )decoded.size(), 0 };
	sirVirtualTextureDecodedPayload_t result = {};
	SIR_TEST( sirVTGetDecodedPayload( view, 0, input, &result ) );
	SIR_TEST( result.pBytes == decoded.data() + 16 && result.nBytes == nPhysicalBytes && result.eFormat == SIR_VT_PHYSICAL_BC1 &&
	          result.nFlags == 1 );
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, nullptr ) );
	SIR_TEST( !sirVTGetDecodedPayload( view, 1, input, &result ) && result.pBytes == nullptr );
	sirVirtualTextureDecodedCluster_t bad = input;
	bad.nClusterId = 1;
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, bad, &result ) && result.pBytes == nullptr );
	bad = input;
	bad.nBytes--;
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, bad, &result ) && result.pBytes == nullptr );
	bad = input;
	bad.pBytes = nullptr;
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, bad, &result ) );
	decoded[16] ^= 1;
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) && result.pBytes == nullptr );
	decoded[16] ^= 1;
	payload.nClusterOffset = 17;
	memcpy( szPayloadRecord + 1, &payload, sizeof( payload ) );
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) );
	payload.nClusterOffset = UINT32_MAX;
	memcpy( szPayloadRecord + 1, &payload, sizeof( payload ) );
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) );
	payload.nClusterOffset = 16;
	payload.nFormat = UINT32_MAX;
	memcpy( szPayloadRecord + 1, &payload, sizeof( payload ) );
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) );
	payload.nFormat = SIR_VT_PHYSICAL_BC1;
	payload.nCodec = 1;
	memcpy( szPayloadRecord + 1, &payload, sizeof( payload ) );
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) );
	payload.nCodec = 0;
	memcpy( szPayloadRecord + 1, &payload, sizeof( payload ) );
	cluster.nFirstPayload = UINT32_MAX;
	memcpy( szClusterRecord + 1, &cluster, sizeof( cluster ) );
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) );
	cluster.nFirstPayload = 0;
	memcpy( szClusterRecord + 1, &cluster, sizeof( cluster ) );
	view.szDirectories[SIR_VT_PACK_PAYLOADS].nBytes--;
	SIR_TEST( !sirVTGetDecodedPayload( view, 0, input, &result ) );
	return 0;
}
