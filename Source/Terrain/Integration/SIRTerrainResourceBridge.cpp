/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Terrain/Integration/SIRTerrainResourceBridge.h>

#include <limits.h>
#include <string.h>

SIRResourceKey SIRTerrainResourceBridge::getKey( const sirTerrainRegionIdentity_t& identity ) noexcept
{
	return { identity.nAssetId, SIR_TERRAIN_HEIGHTFIELD_RESOURCE_TYPE, identity.nCookedVariantId };
}

SIRResourceContentKey SIRTerrainResourceBridge::fingerprint( sirTerrainConstByteSpan_t source ) noexcept
{
	uint64_t nLo = UINT64_C( 14695981039346656037 );
	uint64_t nHi = UINT64_C( 7809847782465536322 );
	for ( size_t nIndex = 0; nIndex < source.nCapacity; ++nIndex ) {
		nLo = ( nLo ^ source.pData[nIndex] ) * UINT64_C( 1099511628211 );
		nHi = ( nHi ^ static_cast<uint8_t>( source.pData[nIndex] ^ 0xA5u ) ) * UINT64_C( 1099511628211 );
	}
	return { nLo, nHi, SIR_TERRAIN_HEIGHTFIELD_FORMAT, 0 };
}

SIRResourceResult SIRTerrainResourceBridge::publish( SIRResourceGraph& graph, sirTerrainConstByteSpan_t source,
                                                     const sirTerrainRegionPublishRequest_t& request, SIRResourceHandle& handle ) noexcept
{
	sirTerrainRegionDescriptor_t descriptor {};
	const SIRTerrainCodecResult eCodec = SIRTerrainHeightfieldCodec::inspect( source, descriptor, request.nMinimumTerrainRevision );
	if ( eCodec == SIRTerrainCodecResult::SIR_TERRAIN_CODEC_STALE_REVISION ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	if ( eCodec == SIRTerrainCodecResult::SIR_TERRAIN_CODEC_INVALID_ARGUMENT ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( eCodec != SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( request.identity.nAssetId == 0 ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( descriptor.iRegionX != request.identity.iRegionX || descriptor.iRegionZ != request.identity.iRegionZ ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	if ( !pWriter ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	const SIRResourceKey key = getKey( request.identity );
	if ( request.nExpectedGraphRevision != 0 ) {
		SIRResourceReadScope scope( graph );
		if ( !scope.isActive() ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
		}
		SIRResourceHandle previous {};
		const SIRResourceResult eFound = graph.find( scope, key, &previous );
		if ( eFound == SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_STALE_REVISION;
		}
		if ( eFound != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eFound;
		}
		SIRResourceView previousView;
		const SIRResourceResult eResolve = graph.resolve( scope, previous, &previousView );
		if ( eResolve != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResolve;
		}
		SIRResourceInfo info {};
		const SIRResourceResult eInfo = previousView.getInfo( &info );
		if ( eInfo != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eInfo;
		}
		if ( info.m_nRevision != request.nExpectedGraphRevision ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_STALE_REVISION;
		}
		if ( info.m_eKind != SIRResourceKind::SIR_RESOURCE_KIND_FIELD || info.m_nFormatId != SIR_TERRAIN_HEIGHTFIELD_FORMAT || info.m_nSchemaVersion != 1 ||
		     info.m_nChunkCount != 1 ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_TYPE_MISMATCH;
		}
		SIRResourceConstBytes previousBytes {};
		if ( previousView.getChunk( { 0 }, &previousBytes ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS || previousBytes.m_nSize > SIZE_MAX ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		sirTerrainRegionDescriptor_t previousDescriptor {};
		if ( SIRTerrainHeightfieldCodec::inspect( { previousBytes.m_pData, static_cast<size_t>(previousBytes.m_nSize) }, previousDescriptor ) !=
		         SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS ||
		     previousDescriptor.iRegionX != descriptor.iRegionX || previousDescriptor.iRegionZ != descriptor.iRegionZ ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( descriptor.nRevision <= previousDescriptor.nRevision ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_STALE_REVISION;
		}
	}
	SIRResourceStagingHandle staging {};
	SIRResourceMutableBytes stagingBytes {};
	const SIRResourceResult eStaging = pWriter->allocateStaging( source.nCapacity, 16, request.nStagingDomain, &staging, &stagingBytes );
	if ( eStaging != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eStaging;
	}
	memcpy( stagingBytes.m_pData, source.pData, source.nCapacity );
	const SIRResourceChunkPublication chunk = { { 0 }, fingerprint( source ), staging, false };
	const SIRResourcePublication publication = { key, SIRResourceKind::SIR_RESOURCE_KIND_FIELD, 1, SIR_TERRAIN_HEIGHTFIELD_FORMAT, &chunk, 1, nullptr, 0 };
	const SIRResourceResult ePublished = pWriter->publishExpected( publication, request.nExpectedGraphRevision, &handle );
	if ( ePublished != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		pWriter->cancelStaging( staging );
	}
	return ePublished;
}

SIRResourceResult SIRTerrainResourceBridge::open( const SIRResourceGraph& graph, const SIRResourceReadScope& scope,
                                                  const sirTerrainRegionIdentity_t& identity, sirTerrainR16View_t& view ) noexcept
{
	if ( !identity.nAssetId ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRResourceKey key = getKey( identity );
	SIRResourceHandle handle {};
	const SIRResourceResult eFind = graph.find( scope, key, &handle );
	if ( eFind != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eFind;
	}
	SIRResourceView resource;
	const SIRResourceResult eResolve = graph.resolve( scope, handle, &resource );
	if ( eResolve != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResolve;
	}
	SIRResourceInfo info {};
	const SIRResourceResult eInfo = resource.getInfo( &info );
	if ( eInfo != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eInfo;
	}
	if ( info.m_eKind != SIRResourceKind::SIR_RESOURCE_KIND_FIELD || info.m_nFormatId != SIR_TERRAIN_HEIGHTFIELD_FORMAT || info.m_nSchemaVersion != 1 ||
	     info.m_nChunkCount != 1 ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	SIRResourceConstBytes payload {};
	const SIRResourceResult eChunk = resource.getChunk( { 0 }, &payload );
	if ( eChunk != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eChunk;
	}
	if ( payload.m_nSize > SIZE_MAX ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	sirTerrainR16View_t candidate {};
	if ( SIRTerrainHeightfieldView::open( { payload.m_pData, static_cast<size_t>(payload.m_nSize) }, candidate ) != SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS ||
	     candidate.descriptor.iRegionX != identity.iRegionX || candidate.descriptor.iRegionZ != identity.iRegionZ ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	view = candidate;
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}
