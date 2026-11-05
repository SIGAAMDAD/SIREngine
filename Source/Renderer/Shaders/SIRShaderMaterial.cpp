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

#include <Renderer/Shaders/SIRShaderMaterial.h>

static uint16_t sirMaterialRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )( ( uint16_t )pBytes[0] | ( ( uint16_t )pBytes[1] << 8 ) );
}

static uint32_t sirMaterialRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static uint64_t sirMaterialRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirMaterialRead32( pBytes ) | ( ( uint64_t )sirMaterialRead32( pBytes + 4 ) << 32 );
}

static void sirMaterialWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirMaterialWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static void sirMaterialWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	sirMaterialWrite32( pBytes, ( uint32_t )nValue );
	sirMaterialWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirMaterialHashScalar( uint64_t nHash, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nHash ^= ( uint8_t )( nValue >> ( iByte * 8 ) );
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static bool sirMaterialHeaderValid( const sirShaderMaterialHeader_t& header ) noexcept
{
	if ( header.nMagic != SIR_SHADER_MATERIAL_MAGIC || header.nSchema != SIR_SHADER_MATERIAL_SCHEMA ||
	     header.nHeaderBytes != SIR_SHADER_MATERIAL_HEADER_BYTES || header.nReserved != 0 || header.nAssetId == 0 ||
	     header.nSourceRevision == 0 ) {
		return false;
	}
	if ( header.nPassCount == 0 || header.nPassCount > SIR_SHADER_MATERIAL_MAX_PASSES ||
	     header.nDependencyCount > SIR_SHADER_MATERIAL_MAX_DEPENDENCIES ) {
		return false;
	}
	return header.nTotalBytes == SIR_SHADER_MATERIAL_HEADER_BYTES + header.nPassCount * SIR_SHADER_MATERIAL_PASS_BYTES;
}

static bool sirMaterialPassValid( const sirShaderMaterialPassRecord_t& pass, uint32_t nDependencies ) noexcept
{
	if ( pass.nPass < SIR_SHADER_PASS_COLOR || pass.nPass > SIR_SHADER_PASS_COMPUTE ) {
		return false;
	}
	constexpr uint64_t VALID_FEATURES = ( SIR_GRAPHICS_FEATURE_PRESENT << 1 ) - 1ULL;
	if ( ( pass.nRequiredFeatures & ~VALID_FEATURES ) != 0 ) {
		return false;
	}
	if ( pass.iShaderDependency == SIR_SHADER_MATERIAL_NO_SHADER ) {
		return pass.nPass != SIR_SHADER_PASS_COMPUTE && ( pass.nRequiredFeatures & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) != 0;
	}
	return pass.iShaderDependency < nDependencies && ( pass.nRequiredFeatures & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) == 0;
}

sirGraphicsResult_t sirShaderEncodeMaterialHeader( const sirShaderMaterialHeader_t& header, void *pBytes, uint64_t nCapacity ) noexcept
{
	if ( pBytes == nullptr || nCapacity < SIR_SHADER_MATERIAL_HEADER_BYTES || !sirMaterialHeaderValid( header ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t *pData = ( uint8_t * )pBytes;
	sirMaterialWrite32( pData, header.nMagic );
	sirMaterialWrite16( pData + 4, header.nSchema );
	sirMaterialWrite16( pData + 6, header.nHeaderBytes );
	sirMaterialWrite32( pData + 8, header.nTotalBytes );
	sirMaterialWrite32( pData + 12, header.nDependencyCount );
	sirMaterialWrite32( pData + 16, header.nPassCount );
	sirMaterialWrite64( pData + 20, header.nAssetId );
	sirMaterialWrite64( pData + 28, header.nSourceRevision );
	sirMaterialWrite64( pData + 36, header.nManifestHash );
	sirMaterialWrite32( pData + 44, header.nReserved );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirShaderDecodeMaterialHeader( const void *pBytes, uint64_t nLength, sirShaderMaterialHeader_t *pHeader ) noexcept
{
	if ( pBytes == nullptr || pHeader == nullptr || nLength < SIR_SHADER_MATERIAL_HEADER_BYTES ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint8_t *pData = ( const uint8_t * )pBytes;
	sirShaderMaterialHeader_t header = {};
	header.nMagic = sirMaterialRead32( pData );
	header.nSchema = sirMaterialRead16( pData + 4 );
	header.nHeaderBytes = sirMaterialRead16( pData + 6 );
	header.nTotalBytes = sirMaterialRead32( pData + 8 );
	header.nDependencyCount = sirMaterialRead32( pData + 12 );
	header.nPassCount = sirMaterialRead32( pData + 16 );
	header.nAssetId = sirMaterialRead64( pData + 20 );
	header.nSourceRevision = sirMaterialRead64( pData + 28 );
	header.nManifestHash = sirMaterialRead64( pData + 36 );
	header.nReserved = sirMaterialRead32( pData + 44 );
	if ( !sirMaterialHeaderValid( header ) || header.nTotalBytes > nLength ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHeader = header;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirShaderEncodeMaterialPass( const sirShaderMaterialPassRecord_t& pass, void *pBytes, uint64_t nCapacity ) noexcept
{
	if ( pBytes == nullptr || nCapacity < SIR_SHADER_MATERIAL_PASS_BYTES ||
	     !sirMaterialPassValid( pass, SIR_SHADER_MATERIAL_MAX_DEPENDENCIES ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t *pData = ( uint8_t * )pBytes;
	sirMaterialWrite32( pData, pass.nPass );
	sirMaterialWrite32( pData + 4, pass.iShaderDependency );
	sirMaterialWrite64( pData + 8, pass.nRequiredFeatures );
	sirMaterialWrite64( pData + 16, pass.nPermutationKey );
	sirMaterialWrite64( pData + 24, pass.nRenderStateHash );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirShaderDecodeMaterialPass( const void *pBytes, uint64_t nLength, sirShaderMaterialPassRecord_t *pPass ) noexcept
{
	if ( pBytes == nullptr || pPass == nullptr || nLength < SIR_SHADER_MATERIAL_PASS_BYTES ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint8_t *pData = ( const uint8_t * )pBytes;
	sirShaderMaterialPassRecord_t pass = {};
	pass.nPass = sirMaterialRead32( pData );
	pass.iShaderDependency = sirMaterialRead32( pData + 4 );
	pass.nRequiredFeatures = sirMaterialRead64( pData + 8 );
	pass.nPermutationKey = sirMaterialRead64( pData + 16 );
	pass.nRenderStateHash = sirMaterialRead64( pData + 24 );
	if ( !sirMaterialPassValid( pass, SIR_SHADER_MATERIAL_MAX_DEPENDENCIES ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pPass = pass;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirShaderBuildMaterialHash( const sirShaderMaterialHeader_t& header, const void *pPassBytes, uint64_t nLength,
                                                uint64_t *pHash ) noexcept
{
	if ( pHash == nullptr || pPassBytes == nullptr || !sirMaterialHeaderValid( header ) ||
	     nLength != ( uint64_t )header.nPassCount * SIR_SHADER_MATERIAL_PASS_BYTES ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nHash = 14695981039346656037ULL;
	nHash = sirMaterialHashScalar( nHash, header.nSchema );
	nHash = sirMaterialHashScalar( nHash, header.nAssetId );
	nHash = sirMaterialHashScalar( nHash, header.nSourceRevision );
	nHash = sirMaterialHashScalar( nHash, header.nDependencyCount );
	nHash = sirMaterialHashScalar( nHash, header.nPassCount );
	const uint8_t *pData = ( const uint8_t * )pPassBytes;
	for ( uint64_t iByte = 0; iByte < nLength; ++iByte ) {
		nHash ^= pData[iByte];
		nHash *= 1099511628211ULL;
	}
	*pHash = nHash;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirShaderValidateMaterial( const void *pBytes, uint64_t nLength ) noexcept
{
	sirShaderMaterialHeader_t header = {};
	if ( sirShaderDecodeMaterialHeader( pBytes, nLength, &header ) != SIR_GRAPHICS_SUCCESS || nLength != header.nTotalBytes ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint8_t *pData = ( const uint8_t * )pBytes;
	uint64_t nHash = 0;
	if ( sirShaderBuildMaterialHash( header, pData + SIR_SHADER_MATERIAL_HEADER_BYTES, nLength - SIR_SHADER_MATERIAL_HEADER_BYTES,
	                                 &nHash ) != SIR_GRAPHICS_SUCCESS ||
	     nHash != header.nManifestHash ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nSeenPasses = 0;
	for ( uint32_t iPass = 0; iPass < header.nPassCount; ++iPass ) {
		sirShaderMaterialPassRecord_t pass = {};
		const uint8_t *pRecord = pData + SIR_SHADER_MATERIAL_HEADER_BYTES + iPass * SIR_SHADER_MATERIAL_PASS_BYTES;
		if ( sirShaderDecodeMaterialPass( pRecord, SIR_SHADER_MATERIAL_PASS_BYTES, &pass ) != SIR_GRAPHICS_SUCCESS ||
		     !sirMaterialPassValid( pass, header.nDependencyCount ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint32_t nBit = 1U << pass.nPass;
		if ( ( nSeenPasses & nBit ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nSeenPasses |= nBit;
	}
	return SIR_GRAPHICS_SUCCESS;
}

