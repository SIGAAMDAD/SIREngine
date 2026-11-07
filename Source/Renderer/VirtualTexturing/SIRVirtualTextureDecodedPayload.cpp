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

bool sirVTGetDecodedPayload( const sirVirtualTexturePackView_t& view, uint32_t nPayloadId, const sirVirtualTextureDecodedCluster_t& decoded,
                             sirVirtualTextureDecodedPayload_t *pOutput ) noexcept
{
	if ( pOutput == nullptr ) {
		return false;
	}
	*pOutput = {};
	if ( decoded.pBytes == nullptr || decoded.nClusterId >= view.header.nClusterCount ) {
		return false;
	}
	sirVirtualTexturePackPayloadRecord_t payload = {};
	sirVirtualTexturePackClusterRecord_t cluster = {};
	if ( !sirVTPackGetPayload( view, nPayloadId, &payload ) || !sirVTPackGetCluster( view, decoded.nClusterId, &cluster ) ||
	     payload.nClusterId != decoded.nClusterId || cluster.nFirstPayload > view.header.nPayloadCount ||
	     cluster.nPayloadCount > view.header.nPayloadCount - cluster.nFirstPayload || nPayloadId < cluster.nFirstPayload ||
	     nPayloadId - cluster.nFirstPayload >= cluster.nPayloadCount || cluster.nDecodedBytes != decoded.nBytes || payload.nCodec != 0 ||
	     payload.nStoredBytes != payload.nDecodedBytes || payload.nFormat >= SIR_VT_PHYSICAL_COUNT ||
	     payload.nDecodedBytes != sirVTPackPhysicalTileBytes( ( sirVirtualTexturePhysicalFamily_t )payload.nFormat ) ||
	     payload.nClusterOffset > decoded.nBytes || payload.nDecodedBytes > decoded.nBytes - payload.nClusterOffset ) {
		return false;
	}
	const uint8_t *pTile = decoded.pBytes + payload.nClusterOffset;
	if ( sirVTPackChecksum32( pTile, payload.nDecodedBytes ) != payload.nChecksum ||
	     sirVTPackHash64( pTile, payload.nDecodedBytes ) != payload.nContentSignature ) {
		return false;
	}
	*pOutput = { pTile, payload.nDecodedBytes, ( sirVirtualTexturePhysicalFamily_t )payload.nFormat, payload.nFlags };
	return true;
}
