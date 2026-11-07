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

#include <string.h>

// Directory data may begin at an unaligned address. Never cast it to a record
// pointer or use a record count as a substitute for byte-range validation.
template <typename T>
static bool sirVTLookupRecord( const sirVirtualTexturePackSpan_t& span, uint32_t nIndex, uint32_t nCount, T *pRecord ) noexcept
{
	if ( pRecord == nullptr || nIndex >= nCount ) {
		return false;
	}
	const uint64_t nOffset = ( uint64_t )nIndex * sizeof( T );
	if ( span.pData == nullptr || nOffset > span.nBytes || sizeof( T ) > span.nBytes - nOffset ) {
		return false;
	}
	memcpy( pRecord, span.pData + nOffset, sizeof( T ) );
	return true;
}

bool sirVTPackGetAllocation( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                             sirVirtualTexturePackAllocationRecord_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_ALLOCATIONS], nIndex, view.header.nAllocationCount, pRecord );
}

bool sirVTPackGetAllocationKey( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                                sirVirtualTexturePackAllocationKey_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_ALLOCATION_KEYS], nIndex, view.header.nAllocationCount, pRecord );
}

bool sirVTPackGetChannelAllocation( const sirVirtualTexturePackView_t& view, uint32_t nAllocation, uint32_t nChannel,
                                    sirVirtualTexturePackChannelAllocation_t *pRecord ) noexcept
{
	if ( nAllocation >= view.header.nAllocationCount || nChannel >= SIR_VT_PACK_CHANNEL_COUNT ||
	     view.header.nAllocationCount > UINT32_MAX / SIR_VT_PACK_CHANNEL_COUNT ) {
		return false;
	}
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_CHANNEL_ALLOCATIONS], nAllocation * SIR_VT_PACK_CHANNEL_COUNT + nChannel,
	                          view.header.nChannelAllocationCount, pRecord );
}

bool sirVTPackGetPayload( const sirVirtualTexturePackView_t& view, uint32_t nIndex, sirVirtualTexturePackPayloadRecord_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_PAYLOADS], nIndex, view.header.nPayloadCount, pRecord );
}

bool sirVTPackGetCluster( const sirVirtualTexturePackView_t& view, uint32_t nIndex, sirVirtualTexturePackClusterRecord_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_CLUSTERS], nIndex, view.header.nClusterCount, pRecord );
}

bool sirVTPackGetTail( const sirVirtualTexturePackView_t& view, uint32_t nIndex, sirVirtualTexturePackTailRecord_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_TAILS], nIndex, view.header.nTailCount, pRecord );
}

bool sirVTPackGetQualityView( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                              sirVirtualTexturePackQualityView_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_QUALITY_VIEWS], nIndex, view.header.nQualityViewCount, pRecord );
}

bool sirVTPackGetFamily( const sirVirtualTexturePackView_t& view, uint32_t nChannel, sirVirtualTexturePackFamilyRecord_t *pRecord ) noexcept
{
	return sirVTLookupRecord( view.szDirectories[SIR_VT_PACK_FAMILIES], nChannel, view.header.nFormatFamilyCount, pRecord );
}
