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

#include <Terrain/Data/SIRTerrainHeightfieldCodec.h>

#include <math.h>
#include <string.h>

static constexpr uint8_t SIR_TERRAIN_MAGIC[4] = { 'S', 'T', 'H', 'F' };
static constexpr uint16_t SIR_TERRAIN_FORMAT_VERSION = 1;
static constexpr uint32_t SIR_TERRAIN_CHECKSUM_OFFSET = 40;
static constexpr uint32_t SIR_TERRAIN_HEADER_CHECKSUM_OFFSET = 44;
static constexpr uint32_t SIR_TERRAIN_PAYLOAD_OFFSET = 64;

static uint16_t sirTerrainRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )( ( uint16_t )pBytes[0] | ( ( uint16_t )pBytes[1] << 8u ) );
}

static uint32_t sirTerrainRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8u ) | ( ( uint32_t )pBytes[2] << 16u ) | ( ( uint32_t )pBytes[3] << 24u );
}

static uint64_t sirTerrainRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirTerrainRead32( pBytes ) | ( ( uint64_t )sirTerrainRead32( pBytes + 4 ) << 32u );
}

static void sirTerrainWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8u );
}

static void sirTerrainWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t nIndex = 0; nIndex < 4; ++nIndex ) {
		pBytes[nIndex] = ( uint8_t )( nValue >> ( nIndex * 8u ) );
	}
}

static void sirTerrainWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	sirTerrainWrite32( pBytes, ( uint32_t )nValue );
	sirTerrainWrite32( pBytes + 4, ( uint32_t )( nValue >> 32u ) );
}

static uint32_t sirTerrainFloatBits( float fValue ) noexcept
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	return nBits;
}

static float sirTerrainBitsFloat( uint32_t nBits ) noexcept
{
	float fValue = 0;
	memcpy( &fValue, &nBits, sizeof( fValue ) );
	return fValue;
}

static uint32_t sirTerrainChecksum( const uint8_t *pData, size_t nBytes ) noexcept
{
	uint32_t nHash = 2166136261u;
	for ( size_t nIndex = 0; nIndex < nBytes; ++nIndex ) {
		nHash = ( nHash ^ pData[nIndex] ) * 16777619u;
	}
	return nHash;
}

bool SIRTerrainHeightfieldCodec::isValidDescriptor( const sirTerrainRegionDescriptor_t& descriptor ) noexcept
{
	return descriptor.nRevision != 0 && SIRTerrainHeightfieldCodec::getEncodedByteCount( descriptor.nResolution ) != 0 &&
	       isfinite( descriptor.fMinHeight ) && isfinite( descriptor.fMaxHeight ) && descriptor.fMinHeight < descriptor.fMaxHeight &&
	       isfinite( descriptor.fMaxHeight - descriptor.fMinHeight );
}

size_t SIRTerrainHeightfieldCodec::getEncodedByteCount( uint32_t nResolution ) noexcept
{
	if ( nResolution < MIN_RESOLUTION || nResolution > MAX_RESOLUTION ) {
		return 0;
	}
	return HEADER_BYTES + ( size_t )nResolution * ( size_t )nResolution * sizeof( uint16_t );
}

sirTerrainCodecResult_t SIRTerrainHeightfieldCodec::encode( const sirTerrainRegionDescriptor_t& descriptor,
                                                            sirTerrainConstHeightSpan_t heights,
                                                            sirTerrainMutableByteSpan_t output ) noexcept
{
	if ( !isValidDescriptor( descriptor ) || !heights.pData || !output.pData ) {
		return SIR_TERRAIN_CODEC_INVALID_ARGUMENT;
	}
	const size_t nCount = ( size_t )descriptor.nResolution * descriptor.nResolution;
	const size_t nBytes = getEncodedByteCount( descriptor.nResolution );
	if ( heights.nCapacity < nCount || output.nCapacity < nBytes ) {
		return SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL;
	}
	for ( size_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		if ( !isfinite( heights.pData[nIndex] ) ) {
			return SIR_TERRAIN_CODEC_NONFINITE_HEIGHT;
		}
	}

	memset( output.pData, 0, HEADER_BYTES );
	memcpy( output.pData, SIR_TERRAIN_MAGIC, sizeof( SIR_TERRAIN_MAGIC ) );
	sirTerrainWrite16( output.pData + 4, SIR_TERRAIN_FORMAT_VERSION );
	sirTerrainWrite16( output.pData + 6, HEADER_BYTES );
	sirTerrainWrite32( output.pData + 8, descriptor.nResolution );
	sirTerrainWrite32( output.pData + 12, ( uint32_t )descriptor.iRegionX );
	sirTerrainWrite32( output.pData + 16, ( uint32_t )descriptor.iRegionZ );
	sirTerrainWrite32( output.pData + 20, sirTerrainFloatBits( descriptor.fMinHeight ) );
	sirTerrainWrite32( output.pData + 24, sirTerrainFloatBits( descriptor.fMaxHeight ) );
	sirTerrainWrite64( output.pData + 28, descriptor.nRevision );
	sirTerrainWrite32( output.pData + 36, ( uint32_t )( nBytes - HEADER_BYTES ) );
	const double fRange = ( double )descriptor.fMaxHeight - descriptor.fMinHeight;
	for ( size_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		double fNormalized = ( ( double )heights.pData[nIndex] - descriptor.fMinHeight ) / fRange;
		if ( fNormalized < 0.0 ) {
			fNormalized = 0.0;
		} else if ( fNormalized > 1.0 ) {
			fNormalized = 1.0;
		}
		const uint16_t nQuantized = ( uint16_t )( fNormalized * 65535.0 + 0.5 );
		sirTerrainWrite16( output.pData + SIR_TERRAIN_PAYLOAD_OFFSET + 2 * nIndex, nQuantized );
	}
	sirTerrainWrite32( output.pData + SIR_TERRAIN_CHECKSUM_OFFSET,
	                   sirTerrainChecksum( output.pData + SIR_TERRAIN_PAYLOAD_OFFSET, nBytes - HEADER_BYTES ) );
	sirTerrainWrite32( output.pData + SIR_TERRAIN_HEADER_CHECKSUM_OFFSET,
	                   sirTerrainChecksum( output.pData, SIR_TERRAIN_HEADER_CHECKSUM_OFFSET ) );
	return SIR_TERRAIN_CODEC_SUCCESS;
}

sirTerrainCodecResult_t SIRTerrainHeightfieldCodec::inspect( sirTerrainConstByteSpan_t input, sirTerrainRegionDescriptor_t& descriptor,
                                                             uint64_t nMinimumRevision ) noexcept
{
	if ( !input.pData ) {
		return SIR_TERRAIN_CODEC_INVALID_ARGUMENT;
	}
	if ( input.nCapacity < HEADER_BYTES ) {
		return SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL;
	}
	if ( memcmp( input.pData, SIR_TERRAIN_MAGIC, sizeof( SIR_TERRAIN_MAGIC ) ) != 0 ) {
		return SIR_TERRAIN_CODEC_INVALID_FORMAT;
	}
	if ( sirTerrainRead16( input.pData + 4 ) != SIR_TERRAIN_FORMAT_VERSION ) {
		return SIR_TERRAIN_CODEC_UNSUPPORTED_VERSION;
	}
	if ( sirTerrainRead16( input.pData + 6 ) != HEADER_BYTES ) {
		return SIR_TERRAIN_CODEC_INVALID_FORMAT;
	}
	sirTerrainRegionDescriptor_t decoded = {};
	decoded.nResolution = sirTerrainRead32( input.pData + 8 );
	decoded.iRegionX = ( int32_t )sirTerrainRead32( input.pData + 12 );
	decoded.iRegionZ = ( int32_t )sirTerrainRead32( input.pData + 16 );
	decoded.fMinHeight = sirTerrainBitsFloat( sirTerrainRead32( input.pData + 20 ) );
	decoded.fMaxHeight = sirTerrainBitsFloat( sirTerrainRead32( input.pData + 24 ) );
	decoded.nRevision = sirTerrainRead64( input.pData + 28 );
	if ( !isValidDescriptor( decoded ) ) {
		return SIR_TERRAIN_CODEC_INVALID_FORMAT;
	}
	const size_t nExpected = getEncodedByteCount( decoded.nResolution );
	if ( sirTerrainRead32( input.pData + 36 ) != nExpected - HEADER_BYTES ) {
		return SIR_TERRAIN_CODEC_INVALID_FORMAT;
	}
	for ( size_t nIndex = 48; nIndex < HEADER_BYTES; ++nIndex ) {
		if ( input.pData[nIndex] != 0 ) {
			return SIR_TERRAIN_CODEC_INVALID_FORMAT;
		}
	}
	if ( sirTerrainRead32( input.pData + SIR_TERRAIN_HEADER_CHECKSUM_OFFSET ) !=
	     sirTerrainChecksum( input.pData, SIR_TERRAIN_HEADER_CHECKSUM_OFFSET ) ) {
		return SIR_TERRAIN_CODEC_BAD_CHECKSUM;
	}
	if ( input.nCapacity < nExpected ) {
		return SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL;
	}
	if ( input.nCapacity != nExpected ) {
		return SIR_TERRAIN_CODEC_INVALID_FORMAT;
	}
	if ( sirTerrainChecksum( input.pData + SIR_TERRAIN_PAYLOAD_OFFSET, nExpected - HEADER_BYTES ) !=
	     sirTerrainRead32( input.pData + SIR_TERRAIN_CHECKSUM_OFFSET ) ) {
		return SIR_TERRAIN_CODEC_BAD_CHECKSUM;
	}
	if ( decoded.nRevision < nMinimumRevision ) {
		return SIR_TERRAIN_CODEC_STALE_REVISION;
	}
	descriptor = decoded;
	return SIR_TERRAIN_CODEC_SUCCESS;
}

sirTerrainCodecResult_t SIRTerrainHeightfieldCodec::decode( sirTerrainConstByteSpan_t input, sirTerrainRegionDescriptor_t& descriptor,
                                                            sirTerrainMutableHeightSpan_t output, uint64_t nMinimumRevision ) noexcept
{
	sirTerrainRegionDescriptor_t decoded = {};
	const sirTerrainCodecResult_t eResult = inspect( input, decoded, nMinimumRevision );
	if ( eResult != SIR_TERRAIN_CODEC_SUCCESS ) {
		return eResult;
	}
	if ( !output.pData ) {
		return SIR_TERRAIN_CODEC_INVALID_ARGUMENT;
	}
	const size_t nCount = ( size_t )decoded.nResolution * decoded.nResolution;
	if ( output.nCapacity < nCount ) {
		return SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL;
	}
	const double fRange = ( double )decoded.fMaxHeight - decoded.fMinHeight;
	for ( size_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		const uint16_t nQuantized = sirTerrainRead16( input.pData + SIR_TERRAIN_PAYLOAD_OFFSET + 2 * nIndex );
		output.pData[nIndex] = ( float )( decoded.fMinHeight + fRange * ( ( double )nQuantized / 65535.0 ) );
	}
	descriptor = decoded;
	return SIR_TERRAIN_CODEC_SUCCESS;
}
