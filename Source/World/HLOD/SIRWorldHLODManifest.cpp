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

#include <World/HLOD/SIRWorldHLODManifest.h>

#include <math.h>
#include <string.h>

static_assert( sizeof( float ) == sizeof( uint32_t ), "HLOD cooked data requires 32-bit IEEE floats" );

static uint16_t sirHLODRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )pBytes[0] | ( uint16_t )( ( uint16_t )pBytes[1] << 8 );
}

static uint32_t sirHLODRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static uint64_t sirHLODRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirHLODRead32( pBytes ) | ( ( uint64_t )sirHLODRead32( pBytes + 4 ) << 32 );
}

static void sirHLODWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirHLODWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( 8 * iByte ) );
	}
}

static void sirHLODWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	sirHLODWrite32( pBytes, ( uint32_t )nValue );
	sirHLODWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static float sirHLODReadFloat( const uint8_t *pBytes ) noexcept
{
	const uint32_t nBits = sirHLODRead32( pBytes );
	float fValue = 0.0f;
	memcpy( &fValue, &nBits, sizeof( fValue ) );
	return fValue;
}

static void sirHLODWriteFloat( uint8_t *pBytes, float fValue ) noexcept
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	sirHLODWrite32( pBytes, nBits );
}

static uint64_t sirHLODChecksum( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static bool sirHLODValidIdentity( const sirWorldHLODManifestIdentity_t& identity ) noexcept
{
	return identity.nWorldId != 0 && ( identity.nSourceHashLo != 0 || identity.nSourceHashHi != 0 );
}

static bool sirHLODValidNode( const sirWorldHLODManifestNode_t& record, uint32_t iNode, uint32_t nCount ) noexcept
{
	const sirWorldHLODNode_t& node = record.node;
	if ( node.keyGeometry.m_nAssetId == 0 || node.keyGeometry.m_nTypeId == 0 || node.nSourceRevision == 0 ||
	     !isfinite( record.fGeometricError ) || record.fGeometricError < 0.0f ) {
		return false;
	}
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		if ( !isfinite( record.fMinimum[iAxis] ) || !isfinite( record.fMaximum[iAxis] ) ||
		     record.fMinimum[iAxis] > record.fMaximum[iAxis] ) {
			return false;
		}
	}
	if ( node.nChildCount == 0 ) {
		return node.nFirstChild == 0;
	}
	return node.nFirstChild > iNode && node.nFirstChild < nCount && node.nChildCount <= nCount - node.nFirstChild;
}

static bool sirHLODContains( const sirWorldHLODManifestNode_t& parent, const sirWorldHLODManifestNode_t& child ) noexcept
{
	if ( parent.fGeometricError < child.fGeometricError ) {
		return false;
	}
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		if ( child.fMinimum[iAxis] < parent.fMinimum[iAxis] || child.fMaximum[iAxis] > parent.fMaximum[iAxis] ) {
			return false;
		}
	}
	return true;
}

static sirWorldHLODManifestNode_t sirHLODDecodeNode( const uint8_t *pData ) noexcept
{
	sirWorldHLODManifestNode_t record = {};
	record.node.keyGeometry.m_nAssetId = sirHLODRead64( pData );
	record.node.keyGeometry.m_nTypeId = sirHLODRead32( pData + 8 );
	record.node.keyGeometry.m_nVariantId = sirHLODRead32( pData + 12 );
	record.node.nSourceRevision = sirHLODRead64( pData + 16 );
	record.node.nFirstChild = sirHLODRead32( pData + 24 );
	record.node.nChildCount = sirHLODRead32( pData + 28 );
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		record.fMinimum[iAxis] = sirHLODReadFloat( pData + 32 + 4 * iAxis );
		record.fMaximum[iAxis] = sirHLODReadFloat( pData + 44 + 4 * iAxis );
	}
	record.fGeometricError = sirHLODReadFloat( pData + 56 );
	return record;
}

static void sirHLODEncodeNode( uint8_t *pData, const sirWorldHLODManifestNode_t& record ) noexcept
{
	sirHLODWrite64( pData, record.node.keyGeometry.m_nAssetId );
	sirHLODWrite32( pData + 8, record.node.keyGeometry.m_nTypeId );
	sirHLODWrite32( pData + 12, record.node.keyGeometry.m_nVariantId );
	sirHLODWrite64( pData + 16, record.node.nSourceRevision );
	sirHLODWrite32( pData + 24, record.node.nFirstChild );
	sirHLODWrite32( pData + 28, record.node.nChildCount );
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		sirHLODWriteFloat( pData + 32 + 4 * iAxis, record.fMinimum[iAxis] );
		sirHLODWriteFloat( pData + 44 + 4 * iAxis, record.fMaximum[iAxis] );
	}
	sirHLODWriteFloat( pData + 56, record.fGeometricError );
	sirHLODWrite32( pData + 60, 0 );
}

typedef struct sirHLODAccessor_s
{
	const sirWorldHLODManifestNode_t *pNodes;
	const uint8_t *pEncoded;
} sirHLODAccessor_t;

static sirWorldHLODManifestNode_t sirHLODGetNode( const sirHLODAccessor_t& accessor, uint32_t iNode ) noexcept
{
	return accessor.pNodes != nullptr ? accessor.pNodes[iNode]
	                                  : sirHLODDecodeNode( accessor.pEncoded + ( uint64_t )iNode * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES );
}

static sirWorldHLODManifestResult_t sirHLODValidateTree( const sirHLODAccessor_t& accessor, uint32_t nCount,
                                                         sirWorldHLODScratch_t *pScratch ) noexcept
{
	memset( pScratch, 0, sizeof( sirWorldHLODScratch_t ) * nCount );
	for ( uint32_t iNode = 0; iNode < nCount; ++iNode ) {
		const sirWorldHLODManifestNode_t record = sirHLODGetNode( accessor, iNode );
		if ( !sirHLODValidNode( record, iNode, nCount ) ) {
			return SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY;
		}
		const uint32_t nFirstChild = record.node.nFirstChild;
		for ( uint32_t iChild = nFirstChild; iChild < nFirstChild + record.node.nChildCount; ++iChild ) {
			if ( ++pScratch[iChild].nParentReferences != 1 ) {
				return SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY;
			}
		}
	}
	if ( pScratch[0].nParentReferences != 0 ) {
		return SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY;
	}
	for ( uint32_t iNode = 1; iNode < nCount; ++iNode ) {
		if ( pScratch[iNode].nParentReferences != 1 ) {
			return SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY;
		}
	}
	for ( uint32_t iNode = 0; iNode < nCount; ++iNode ) {
		const sirWorldHLODManifestNode_t parent = sirHLODGetNode( accessor, iNode );
		for ( uint32_t iChild = parent.node.nFirstChild; iChild < parent.node.nFirstChild + parent.node.nChildCount; ++iChild ) {
			if ( !sirHLODContains( parent, sirHLODGetNode( accessor, iChild ) ) ) {
				return SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY;
			}
		}
	}
	return SIR_WORLD_HLOD_MANIFEST_OK;
}

sirWorldHLODManifestResult_t sirWorldHLODReadManifest( const sirWorldHLODManifestRead_t& input,
                                                       sirWorldHLODManifestDecode_t& output ) noexcept
{
	if ( input.pBytes == nullptr || output.pNodes == nullptr || output.pBounds == nullptr || output.pGeometricErrors == nullptr ||
	     output.pScratch == nullptr || output.pnDecoded == nullptr || input.nBytes < SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ||
	     !sirHLODValidIdentity( input.expected ) || input.nMaxNodes == 0 ) {
		return SIR_WORLD_HLOD_MANIFEST_INVALID_ARGUMENT;
	}
	const uint8_t *pData = input.pBytes;
	if ( sirHLODRead32( pData ) != SIR_WORLD_HLOD_MANIFEST_MAGIC || sirHLODRead16( pData + 4 ) != SIR_WORLD_HLOD_MANIFEST_VERSION ||
	     sirHLODRead16( pData + 6 ) != SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ||
	     sirHLODRead32( pData + 8 ) != SIR_WORLD_HLOD_MANIFEST_NODE_BYTES ) {
		return SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT;
	}
	const uint32_t nCount = sirHLODRead32( pData + 12 );
	if ( nCount == 0 || ( uint64_t )nCount > ( UINT64_MAX - SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ) / SIR_WORLD_HLOD_MANIFEST_NODE_BYTES ||
	     input.nBytes != SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES + ( uint64_t )nCount * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES ) {
		return SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT;
	}
	if ( sirHLODRead64( pData + 16 ) != input.expected.nWorldId || sirHLODRead64( pData + 24 ) != input.expected.nSourceHashLo ||
	     sirHLODRead64( pData + 32 ) != input.expected.nSourceHashHi ) {
		return SIR_WORLD_HLOD_MANIFEST_STALE_SOURCE;
	}
	if ( nCount > input.nMaxNodes || nCount > output.nNodeCapacity || nCount > output.nScratchCapacity ||
	     ( size_t )nCount > SIZE_MAX / sizeof( sirWorldHLODScratch_t ) ) {
		return SIR_WORLD_HLOD_MANIFEST_CAPACITY;
	}
	const uint8_t *pRecords = pData + SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES;
	if ( sirHLODRead64( pData + 40 ) != sirHLODChecksum( pRecords, input.nBytes - SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ) ) {
		return SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT;
	}
	for ( uint32_t iNode = 0; iNode < nCount; ++iNode ) {
		if ( sirHLODRead32( pRecords + ( uint64_t )iNode * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES + 60 ) != 0 ) {
			return SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT;
		}
	}
	const sirHLODAccessor_t accessor = { nullptr, pRecords };
	const sirWorldHLODManifestResult_t eResult = sirHLODValidateTree( accessor, nCount, output.pScratch );
	if ( eResult != SIR_WORLD_HLOD_MANIFEST_OK ) {
		return eResult;
	}
	for ( uint32_t iNode = 0; iNode < nCount; ++iNode ) {
		const sirWorldHLODManifestNode_t record = sirHLODDecodeNode( pRecords + ( uint64_t )iNode * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES );
		output.pNodes[iNode] = record.node;
		for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
			output.pBounds[iNode].fMinimum[iAxis] = record.fMinimum[iAxis];
			output.pBounds[iNode].fMaximum[iAxis] = record.fMaximum[iAxis];
		}
		output.pGeometricErrors[iNode] = record.fGeometricError;
	}
	*output.pnDecoded = nCount;
	return SIR_WORLD_HLOD_MANIFEST_OK;
}

sirWorldHLODManifestResult_t sirWorldHLODWriteManifest( const sirWorldHLODManifestWrite_t& input,
                                                        sirWorldHLODManifestEncode_t& output ) noexcept
{
	if ( input.pNodes == nullptr || output.pBytes == nullptr || output.pScratch == nullptr || output.pnWritten == nullptr ||
	     input.nNodeCount == 0 || !sirHLODValidIdentity( input.identity ) ) {
		return SIR_WORLD_HLOD_MANIFEST_INVALID_ARGUMENT;
	}
	if ( input.nNodeCount > output.nScratchCapacity || ( size_t )input.nNodeCount > SIZE_MAX / sizeof( sirWorldHLODScratch_t ) ||
	     ( uint64_t )input.nNodeCount > ( UINT64_MAX - SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ) / SIR_WORLD_HLOD_MANIFEST_NODE_BYTES ||
	     output.nCapacity < SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES + ( uint64_t )input.nNodeCount * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES ) {
		return SIR_WORLD_HLOD_MANIFEST_CAPACITY;
	}
	const sirHLODAccessor_t accessor = { input.pNodes, nullptr };
	const sirWorldHLODManifestResult_t eResult = sirHLODValidateTree( accessor, input.nNodeCount, output.pScratch );
	if ( eResult != SIR_WORLD_HLOD_MANIFEST_OK ) {
		return eResult;
	}
	uint8_t *pData = output.pBytes;
	const uint64_t nBytes = SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES + ( uint64_t )input.nNodeCount * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES;
	sirHLODWrite32( pData, SIR_WORLD_HLOD_MANIFEST_MAGIC );
	sirHLODWrite16( pData + 4, SIR_WORLD_HLOD_MANIFEST_VERSION );
	sirHLODWrite16( pData + 6, ( uint16_t )SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES );
	sirHLODWrite32( pData + 8, SIR_WORLD_HLOD_MANIFEST_NODE_BYTES );
	sirHLODWrite32( pData + 12, input.nNodeCount );
	sirHLODWrite64( pData + 16, input.identity.nWorldId );
	sirHLODWrite64( pData + 24, input.identity.nSourceHashLo );
	sirHLODWrite64( pData + 32, input.identity.nSourceHashHi );
	uint8_t *pRecords = pData + SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES;
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		sirHLODEncodeNode( pRecords + ( uint64_t )iNode * SIR_WORLD_HLOD_MANIFEST_NODE_BYTES, input.pNodes[iNode] );
	}
	sirHLODWrite64( pData + 40, sirHLODChecksum( pRecords, nBytes - SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ) );
	*output.pnWritten = nBytes;
	return SIR_WORLD_HLOD_MANIFEST_OK;
}
