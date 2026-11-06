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

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackFileSource.h>

// Port of Terrain3D's terrain_3d_vt_file_source.cpp. Native platform I/O is
// the single I/O path; SIREngine's platform layer owns OS-specific behavior.
static bool sirVTReadFull( const SIRPlatformFile& file, uint64_t nOffset, void *pDestination, uint64_t nBytes ) noexcept
{
	uint64_t nTransferred = 0;
	return file.readAt( nOffset, pDestination, nBytes, &nTransferred ) == SIR_FILE_OK && nTransferred == nBytes;
}

SIRVirtualTexturePackFileSource::SIRVirtualTexturePackFileSource( void ) noexcept
    : m_File(), m_nFileBytes( 0 ), m_nPayloadOffset( 0 ), m_nPayloadBytes( 0 ), m_nReadCount( 0 ), m_nPackageGeneration( 0 ),
      m_nPackageSignature( 0 )
{
}

SIRVirtualTexturePackFileSource::~SIRVirtualTexturePackFileSource( void ) noexcept
{
	close();
}

uint64_t SIRVirtualTexturePackFileSource::getFileBytes( void ) const noexcept
{
	return m_nFileBytes;
}

uint64_t SIRVirtualTexturePackFileSource::getPayloadBytes( void ) const noexcept
{
	return m_nPayloadBytes;
}

uint32_t SIRVirtualTexturePackFileSource::getPackageGeneration( void ) const noexcept
{
	return m_nPackageGeneration;
}

uint64_t SIRVirtualTexturePackFileSource::getReadCount( void ) const noexcept
{
	return m_nReadCount;
}

bool SIRVirtualTexturePackFileSource::isOpen( void ) const noexcept
{
	return m_File.isOpen();
}

void SIRVirtualTexturePackFileSource::close( void ) noexcept
{
	if ( m_File.isOpen() ) {
		m_File.close();
	}
	m_nFileBytes = 0;
	m_nPayloadOffset = 0;
	m_nPayloadBytes = 0;
	m_nReadCount = 0;
	m_nPackageGeneration = 0;
	m_nPackageSignature = 0;
}

bool SIRVirtualTexturePackFileSource::open( const char *pszPath, const sirVirtualTexturePackMetadataBuffer_t& buffer, bool bStrictHash,
                                            sirVirtualTexturePackView_t *pView ) noexcept
{
	close();
	if ( pView != nullptr ) {
		*pView = {};
	}
	if ( pView == nullptr || pszPath == nullptr || *pszPath == '\0' || buffer.pBytes == nullptr ||
	     buffer.nCapacity < SIR_VT_PACK_HEADER_BYTES || m_File.open( pszPath, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
		return false;
	}
	bool bSuccess = false;
	do {
		uint64_t nLength = 0;
		if ( m_File.getSize( &nLength ) != SIR_FILE_OK || nLength < SIR_VT_PACK_HEADER_BYTES || nLength > INT64_MAX ) {
			break;
		}
		sirVirtualTexturePackHeader_t header = {};
		if ( !sirVTReadFull( m_File, 0, &header, sizeof( header ) ) || header.nPackageBytes != nLength ||
		     header.nPayloadDataOffset < SIR_VT_PACK_HEADER_BYTES || header.nPayloadDataOffset > nLength ||
		     header.nPayloadDataOffset > 256U * 1024U * 1024U || header.nPayloadDataOffset > buffer.nCapacity ||
		     !sirVTReadFull( m_File, 0, buffer.pBytes, header.nPayloadDataOffset ) ) {
			break;
		}
		sirVirtualTexturePackView_t candidate = {};
		const sirVirtualTexturePackInput_t input = { buffer.pBytes, header.nPayloadDataOffset, nLength, true };
		if ( !sirVTPackOpenValidated( input, &candidate ) ) {
			break;
		}
		if ( bStrictHash ) {
			uint8_t szScratch[16384] = {};
			uint64_t nHash = UINT64_C( 1469598103934665603 );
			for ( uint64_t nOffset = SIR_VT_PACK_HEADER_BYTES; nOffset < nLength; ) {
				const uint64_t nRemaining = nLength - nOffset;
				const uint64_t nChunk = nRemaining < sizeof( szScratch ) ? nRemaining : sizeof( szScratch );
				if ( !sirVTReadFull( m_File, nOffset, szScratch, nChunk ) ) {
					nHash = 0;
					break;
				}
				for ( uint64_t iByte = 0; iByte < nChunk; ++iByte ) {
					nHash = ( nHash ^ szScratch[iByte] ) * UINT64_C( 1099511628211 );
				}
				nOffset += nChunk;
			}
			if ( nHash != candidate.header.nContentSignature ) {
				break;
			}
		}
		m_nFileBytes = nLength;
		m_nPayloadOffset = candidate.header.nPayloadDataOffset;
		m_nPayloadBytes = candidate.header.nPayloadDataBytes;
		m_nPackageGeneration = candidate.header.nPackageGeneration;
		m_nPackageSignature = candidate.header.nContentSignature;
		*pView = candidate;
		bSuccess = true;
	} while ( false );
	if ( !bSuccess ) {
		close();
	}
	return bSuccess;
}

bool SIRVirtualTexturePackFileSource::readPayload( uint64_t nOffset, void *pDestination, uint32_t nBytes ) noexcept
{
	if ( !m_File.isOpen() || pDestination == nullptr || nBytes == 0 || nOffset > m_nPayloadBytes || nBytes > m_nPayloadBytes - nOffset ||
	     m_nPayloadOffset > ( uint64_t )INT64_MAX - nOffset || nBytes > ( uint64_t )INT64_MAX - m_nPayloadOffset - nOffset ) {
		return false;
	}
	if ( !sirVTReadFull( m_File, m_nPayloadOffset + nOffset, pDestination, nBytes ) ) {
		return false;
	}
	++m_nReadCount;
	return true;
}

// Read exactly one persisted V1 cluster before decoding, so no unbounded
// package payload can be mistaken for an individual GPU tile.
bool SIRVirtualTexturePackFileSource::readCluster( const sirVirtualTexturePackView_t& view, uint32_t nClusterId,
                                                   const sirVirtualTextureClusterBuffers_t& buffers,
                                                   const sirVirtualTextureClusterCodec_t *pCodec ) noexcept
{
	if ( !isOpen() || !view.bMetadataOnly || view.header.nPackageGeneration != m_nPackageGeneration ||
	     view.header.nPayloadDataBytes != m_nPayloadBytes || view.header.nContentSignature != m_nPackageSignature ||
	     nClusterId >= view.header.nClusterCount || view.szDirectories[SIR_VT_PACK_CLUSTERS].pData == nullptr ) {
		return false;
	}
	sirVirtualTexturePackClusterRecord_t cluster = {};
	memcpy( &cluster, view.szDirectories[SIR_VT_PACK_CLUSTERS].pData + ( uint64_t )nClusterId * sizeof( cluster ), sizeof( cluster ) );
	if ( cluster.nStoredBytes == 0 || cluster.nStoredBytes > buffers.nStoredCapacity || buffers.pStoredBytes == nullptr ||
	     !readPayload( cluster.nPayloadDataOffset, buffers.pStoredBytes, cluster.nStoredBytes ) ) {
		return false;
	}
	return sirVTDecodePackCluster( view, nClusterId, buffers, pCodec );
}
