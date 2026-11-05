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

#include <Renderer/VirtualTexturing/SIRVirtualTextureClusterDecode.h>

#include <string.h>

// Source contract: Terrain3D's Sparse VT V1 cluster/payload record layout.
// Metadata is borrowed and may be unaligned, hence memcpy for each record.
template <typename T>
static T sirVTReadClusterRecord( const sirVirtualTexturePackView_t& view, sirVirtualTexturePackDirectory_t eDirectory,
                                 uint32_t nIndex ) noexcept
{
	T record = {};
	memcpy( &record, view.szDirectories[eDirectory].pData + ( uint64_t )nIndex * sizeof( T ), sizeof( T ) );
	return record;
}

static bool sirVTClusterOverlaps( const sirVirtualTextureClusterBuffers_t& buffers, uint32_t nStored, uint32_t nDecoded ) noexcept
{
	const uintptr_t nInput = ( uintptr_t )buffers.pStoredBytes;
	const uintptr_t nOutput = ( uintptr_t )buffers.pDecodedBytes;
	if ( nStored > UINTPTR_MAX - nInput || nDecoded > UINTPTR_MAX - nOutput ) {
		return true;
	}
	return nInput < nOutput + nDecoded && nOutput < nInput + nStored;
}

static bool sirVTVerifyDecodedCluster( const sirVirtualTexturePackView_t& view, const sirVirtualTexturePackClusterRecord_t& cluster,
                                       const uint8_t *pDecoded ) noexcept
{
	for ( uint32_t i = 0; i < cluster.nPayloadCount; ++i ) {
		const sirVirtualTexturePackPayloadRecord_t payload =
		    sirVTReadClusterRecord<sirVirtualTexturePackPayloadRecord_t>( view, SIR_VT_PACK_PAYLOADS, cluster.nFirstPayload + i );
		if ( payload.nClusterOffset > cluster.nDecodedBytes || payload.nDecodedBytes > cluster.nDecodedBytes - payload.nClusterOffset ) {
			return false;
		}
		const uint8_t *pTile = pDecoded + payload.nClusterOffset;
		if ( payload.nChecksum != sirVTPackChecksum32( pTile, payload.nDecodedBytes ) ||
		     payload.nContentSignature != sirVTPackHash64( pTile, payload.nDecodedBytes ) ) {
			return false;
		}
	}
	return true;
}

bool sirVTDecodePackCluster( const sirVirtualTexturePackView_t& view, uint32_t nClusterId, const sirVirtualTextureClusterBuffers_t& buffers,
                             const sirVirtualTextureClusterCodec_t *pCodec ) noexcept
{
	if ( view.szDirectories[SIR_VT_PACK_CLUSTERS].pData == nullptr || view.szDirectories[SIR_VT_PACK_PAYLOADS].pData == nullptr ||
	     nClusterId >= view.header.nClusterCount || buffers.pStoredBytes == nullptr || buffers.pDecodedBytes == nullptr ) {
		return false;
	}
	const sirVirtualTexturePackClusterRecord_t cluster =
	    sirVTReadClusterRecord<sirVirtualTexturePackClusterRecord_t>( view, SIR_VT_PACK_CLUSTERS, nClusterId );
	if ( cluster.nStoredBytes == 0 || cluster.nDecodedBytes == 0 || cluster.nStoredBytes > buffers.nStoredCapacity ||
	     cluster.nDecodedBytes > buffers.nDecodedCapacity || cluster.nCodec > 1 ||
	     cluster.nPayloadDataOffset > view.header.nPayloadDataBytes ||
	     cluster.nStoredBytes > view.header.nPayloadDataBytes - cluster.nPayloadDataOffset ) {
		return false;
	}
	if ( sirVTPackChecksum32( buffers.pStoredBytes, cluster.nStoredBytes ) != cluster.nChecksum ||
	     sirVTPackHash64( buffers.pStoredBytes, cluster.nStoredBytes ) != cluster.nContentSignature ) {
		return false;
	}

	if ( cluster.nCodec == 0 ) {
		if ( cluster.nStoredBytes != cluster.nDecodedBytes ) {
			return false;
		}
		memmove( buffers.pDecodedBytes, buffers.pStoredBytes, cluster.nStoredBytes );
	} else {
		if ( pCodec == nullptr || pCodec->pfnDecodeZstd == nullptr ||
		     sirVTClusterOverlaps( buffers, cluster.nStoredBytes, cluster.nDecodedBytes ) ) {
			return false;
		}
		const sirVirtualTextureCodecFrame_t frame = { buffers.pStoredBytes, cluster.nStoredBytes, buffers.pDecodedBytes,
		                                              cluster.nDecodedBytes };
		uint32_t nProduced = 0;
		if ( !pCodec->pfnDecodeZstd( pCodec->pContext, frame, &nProduced ) || nProduced != cluster.nDecodedBytes ) {
			memset( buffers.pDecodedBytes, 0, cluster.nDecodedBytes );
			return false;
		}
	}

	if ( !sirVTVerifyDecodedCluster( view, cluster, buffers.pDecodedBytes ) ) {
		memset( buffers.pDecodedBytes, 0, cluster.nDecodedBytes );
		return false;
	}
	return true;
}
