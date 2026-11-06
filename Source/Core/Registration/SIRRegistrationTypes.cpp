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

#include <Core/Registration/SIRRegistrationTypes.h>

static uint16_t sirRegistrationRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )pBytes[0] | ( ( uint16_t )pBytes[1] << 8 );
}

static uint32_t sirRegistrationRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )sirRegistrationRead16( pBytes ) | ( ( uint32_t )sirRegistrationRead16( pBytes + 2 ) << 16 );
}

static uint64_t sirRegistrationRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirRegistrationRead32( pBytes ) | ( ( uint64_t )sirRegistrationRead32( pBytes + 4 ) << 32 );
}

static uint64_t sirRegistrationHashBytes( const uint8_t *pBytes, size_t nBytes, uint64_t nSeed ) noexcept
{
	uint64_t nHash = nSeed;
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * SIR_REGISTRATION_HASH_PRIME;
	}
	return nHash;
}

static bool sirRegistrationNameCharacter( uint8_t nCharacter, bool bSegmentStart ) noexcept
{
	if ( nCharacter == '_' || ( nCharacter >= 'a' && nCharacter <= 'z' ) || ( nCharacter >= 'A' && nCharacter <= 'Z' ) ) {
		return true;
	}
	return !bSegmentStart && nCharacter >= '0' && nCharacter <= '9';
}

uint8_t sirCVarSourcePriority( sirCVarSource_t eSource ) noexcept
{
	return eSource < SIR_CVAR_SOURCE_COUNT ? ( uint8_t )eSource * 10 : 0;
}

bool sirValidateCVarFlags( uint32_t nFlags ) noexcept
{
	constexpr uint32_t nAllowed = ( 1u << 11 ) - 1u;
	if ( ( nFlags & ~nAllowed ) != 0 || ( ( nFlags & SIR_CVAR_FLAG_ARCHIVE ) != 0 && ( nFlags & SIR_CVAR_FLAG_TRANSIENT ) != 0 ) ) {
		return false;
	}
	if ( ( nFlags & SIR_CVAR_FLAG_REPLICATED ) != 0 && ( nFlags & SIR_CVAR_FLAG_SENSITIVE ) != 0 ) {
		return false;
	}
	return true;
}

bool sirValidateConsoleName( const char *pszName, size_t nLength ) noexcept
{
	if ( pszName == nullptr || nLength == 0 || nLength > SIR_REGISTRATION_MAX_NAME_BYTES ) {
		return false;
	}
	bool bSegmentStart = true;
	for ( size_t iByte = 0; iByte < nLength; ++iByte ) {
		const uint8_t nCharacter = ( uint8_t )pszName[iByte];
		if ( nCharacter == '.' ) {
			if ( bSegmentStart ) {
				return false;
			}
			bSegmentStart = true;
			continue;
		}
		if ( !sirRegistrationNameCharacter( nCharacter, bSegmentStart ) ) {
			return false;
		}
		bSegmentStart = false;
	}
	return !bSegmentStart;
}

bool sirNormalizeConsoleName( const char *pszName, size_t nLength, char *pszOutput, size_t nCapacity ) noexcept
{
	if ( !sirValidateConsoleName( pszName, nLength ) || pszOutput == nullptr || nCapacity <= nLength ) {
		return false;
	}
	// Reject overlapping source/destination unless an exact in-place transform is requested.
	const uintptr_t nStart = ( uintptr_t )pszName;
	const uintptr_t nOutput = ( uintptr_t )pszOutput;
	if ( nStart != nOutput && nOutput >= nStart && nOutput - nStart < nLength ) {
		return false;
	}
	if ( nStart != nOutput && nStart > nOutput && nStart - nOutput <= nLength ) {
		return false;
	}
	for ( size_t iByte = 0; iByte < nLength; ++iByte ) {
		const char nCharacter = pszName[iByte];
		pszOutput[iByte] = nCharacter >= 'A' && nCharacter <= 'Z' ? ( char )( nCharacter + ( 'a' - 'A' ) ) : nCharacter;
	}
	pszOutput[nLength] = '\0';
	return true;
}

static sirRegistrationResult_t sirRegistrationCheckSection( const sirRegistrationSection_t& section, uint32_t nRequiredStride,
                                                            uint64_t nTotalBytes ) noexcept
{
	if ( section.nCount > SIR_REGISTRATION_MAX_RECORDS ) {
		return SIR_REGISTRATION_BAD_BOUNDS;
	}
	if ( section.nCount == 0 ) {
		return section.nOffset == 0 && section.nStride == 0 ? SIR_REGISTRATION_OK : SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( section.nStride != nRequiredStride ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( section.nOffset % 8 != 0 ) {
		return SIR_REGISTRATION_BAD_ALIGNMENT;
	}
	if ( section.nOffset < SIR_REGISTRATION_CATALOG_HEADER_BYTES || section.nOffset > nTotalBytes ) {
		return SIR_REGISTRATION_BAD_BOUNDS;
	}
	if ( section.nCount > ( nTotalBytes - section.nOffset ) / section.nStride ) {
		return SIR_REGISTRATION_BAD_BOUNDS;
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t sirValidateRegistrationCatalog( const uint8_t *pBytes, size_t nBytes, uint64_t nExpectedStringHash ) noexcept
{
	if ( pBytes == nullptr || nExpectedStringHash == 0 || nBytes < SIR_REGISTRATION_CATALOG_HEADER_BYTES ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( sirRegistrationRead32( pBytes ) != SIR_REGISTRATION_CATALOG_MAGIC ) {
		return SIR_REGISTRATION_BAD_MAGIC;
	}
	if ( sirRegistrationRead16( pBytes + 4 ) != SIR_REGISTRATION_CATALOG_VERSION ||
	     sirRegistrationRead16( pBytes + 6 ) != SIR_REGISTRATION_CATALOG_HEADER_BYTES ) {
		return SIR_REGISTRATION_BAD_VERSION;
	}
	if ( sirRegistrationRead32( pBytes + 8 ) != SIR_REGISTRATION_CATALOG_BYTE_ORDER ) {
		return SIR_REGISTRATION_BAD_BYTE_ORDER;
	}
	const uint64_t nTotalBytes = sirRegistrationRead64( pBytes + 16 );
	if ( nTotalBytes != nBytes ) {
		return SIR_REGISTRATION_BAD_BOUNDS;
	}
	if ( sirRegistrationRead64( pBytes + 88 ) != nExpectedStringHash || sirRegistrationRead64( pBytes + 112 ) != 0 ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	const uint32_t szStrides[4] = { SIR_REGISTRATION_NAME_STRIDE, SIR_REGISTRATION_COMMAND_STRIDE, SIR_REGISTRATION_CVAR_STRIDE,
	                                SIR_REGISTRATION_SCALABILITY_STRIDE };
	sirRegistrationSection_t szSections[4] = {};
	for ( size_t iSection = 0; iSection < 4; ++iSection ) {
		const uint8_t *pSection = pBytes + 24 + iSection * 16;
		szSections[iSection].nOffset = sirRegistrationRead64( pSection );
		szSections[iSection].nCount = sirRegistrationRead32( pSection + 8 );
		szSections[iSection].nStride = sirRegistrationRead32( pSection + 12 );
		const sirRegistrationResult_t eCheck = sirRegistrationCheckSection( szSections[iSection], szStrides[iSection], nTotalBytes );
		if ( eCheck != SIR_REGISTRATION_OK ) {
			return eCheck;
		}
	}
	for ( size_t iFirst = 0; iFirst < 4; ++iFirst ) {
		const sirRegistrationSection_t& first = szSections[iFirst];
		if ( first.nCount == 0 ) {
			continue;
		}
		const uint64_t nFirstEnd = first.nOffset + ( uint64_t )first.nCount * first.nStride;
		for ( size_t iSecond = iFirst + 1; iSecond < 4; ++iSecond ) {
			const sirRegistrationSection_t& second = szSections[iSecond];
			if ( second.nCount == 0 ) {
				continue;
			}
			const uint64_t nSecondEnd = second.nOffset + ( uint64_t )second.nCount * second.nStride;
			if ( first.nOffset < nSecondEnd && second.nOffset < nFirstEnd ) {
				return SIR_REGISTRATION_OVERLAPPING_SECTIONS;
			}
		}
	}
	const uint64_t nPayloadHash = sirRegistrationHashBytes( pBytes + SIR_REGISTRATION_CATALOG_HEADER_BYTES,
	                                                        nBytes - SIR_REGISTRATION_CATALOG_HEADER_BYTES, SIR_REGISTRATION_HASH_BASIS );
	if ( nPayloadHash != sirRegistrationRead64( pBytes + 104 ) ) {
		return SIR_REGISTRATION_BAD_HASH;
	}
	uint64_t nDescriptorHash = SIR_REGISTRATION_HASH_BASIS;
	for ( size_t iSection = 1; iSection < 4; ++iSection ) {
		const sirRegistrationSection_t& section = szSections[iSection];
		if ( section.nCount != 0 ) {
			nDescriptorHash =
			    sirRegistrationHashBytes( pBytes + section.nOffset, ( size_t )section.nCount * section.nStride, nDescriptorHash );
		}
	}
	return nDescriptorHash == sirRegistrationRead64( pBytes + 96 ) ? SIR_REGISTRATION_OK : SIR_REGISTRATION_BAD_HASH;
}
