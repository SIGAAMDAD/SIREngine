/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Resources/SIRPackSourceMount.h>

#include <string.h>

SIRPackSourceMount::SIRPackSourceMount( void ) noexcept
    : m_pIO( nullptr ), m_pCatalog( nullptr ), m_pHandles( nullptr ), m_nHandles( 0 ), m_nGeneration( 0 )
{
}

sirPackResult_t SIRPackSourceMount::mount( SIRAsyncIOService& io, const SIRPackCatalog& catalog, const char *const *pShardPaths,
                                           sirIOSourceHandle_t *pHandles, uint32_t nCapacity ) noexcept
{
	if ( isMounted() || !catalog.isOpen() || pShardPaths == nullptr || pHandles == nullptr || nCapacity < catalog.getShardCount() ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	uint32_t nOpened = 0;
	sirPackResult_t eResult = SIR_PACK_OK;
	for ( uint32_t iShard = 0; iShard < catalog.getShardCount(); ++iShard ) {
		if ( io.openSource( pShardPaths[iShard], pHandles + iShard ) != SIR_IO_OK ) {
			eResult = SIR_PACK_STALE_SOURCE;
			break;
		}
		++nOpened;
		uint8_t szHeader[SIR_PACK_SHARD_HEADER_BYTES] = {};
		uint64_t nFileBytes = 0;
		if ( io.inspectSource( pHandles[iShard], szHeader, sizeof( szHeader ), &nFileBytes ) != SIR_IO_OK ) {
			eResult = SIR_PACK_TRUNCATED;
			break;
		}
		sirPackShard_t shard = {};
		if ( catalog.shardAt( iShard, &shard ) != SIR_PACK_OK || nFileBytes != shard.nBytes ) {
			eResult = SIR_PACK_STALE_SOURCE;
			break;
		}
		if ( memcmp( szHeader, "SIRP", 4 ) != 0 || sirPackRead16( szHeader + 6 ) != SIR_PACK_SHARD_HEADER_BYTES ||
		     sirPackRead32( szHeader + 8 ) != SIR_PACK_ENDIAN_MARKER ) {
			eResult = SIR_PACK_INVALID_FORMAT;
			break;
		}
		if ( sirPackRead16( szHeader + 4 ) != SIR_PACK_VERSION ) {
			eResult = SIR_PACK_UNSUPPORTED_VERSION;
			break;
		}
		if ( sirPackRead32( szHeader + 12 ) != iShard || sirPackRead64( szHeader + 16 ) != catalog.getPackageId() ||
		     sirPackRead64( szHeader + 24 ) != catalog.getGeneration() || sirPackRead64( szHeader + 32 ) != shard.nBytes ) {
			eResult = SIR_PACK_STALE_SOURCE;
			break;
		}
		for ( uint32_t i = 40; i < sizeof( szHeader ); ++i ) {
			if ( szHeader[i] != 0 ) {
				eResult = SIR_PACK_INVALID_FORMAT;
				break;
			}
		}
		if ( eResult != SIR_PACK_OK ) {
			break;
		}
	}
	if ( eResult != SIR_PACK_OK ) {
		for ( uint32_t i = nOpened; i != 0; --i ) {
			io.closeSource( pHandles[i - 1] );
			pHandles[i - 1] = {};
		}
		return eResult;
	}
	m_pIO = &io;
	m_pCatalog = &catalog;
	m_pHandles = pHandles;
	m_nHandles = nOpened;
	m_nGeneration = catalog.getGeneration();
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackSourceMount::unmount( void ) noexcept
{
	if ( !isMounted() ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	// Close in reverse order, but preserve partial progress if a source is
	// currently pinned by an async read. A retry completes after IO ACK.
	while ( m_nHandles != 0 ) {
		const uint32_t iLast = m_nHandles - 1;
		if ( m_pIO->closeSource( m_pHandles[iLast] ) != SIR_IO_OK ) {
			return SIR_PACK_STALE_SOURCE;
		}
		m_pHandles[iLast] = {};
		--m_nHandles;
	}
	m_pIO = nullptr;
	m_pCatalog = nullptr;
	m_pHandles = nullptr;
	m_nGeneration = 0;
	return SIR_PACK_OK;
}

bool SIRPackSourceMount::isMounted( void ) const noexcept
{
	return m_pIO != nullptr;
}

const sirIOSourceHandle_t *SIRPackSourceMount::getSources( void ) const noexcept
{
	return isMounted() ? m_pHandles : nullptr;
}

uint32_t SIRPackSourceMount::getSourceCount( void ) const noexcept
{
	return m_nHandles;
}
