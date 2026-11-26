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

#include <Core/Strings/SIRStringOps.h>
#include <Core/Strings/SIRStringSIMD.h>

bool SIRStringOps::isValid( sirStringView_t view ) noexcept
{
	return view.pBytes != nullptr || view.nBytes == 0;
}

bool SIRStringOps::overlaps( sirStringView_t left, sirStringView_t right ) noexcept
{
	if ( left.nBytes == 0 || right.nBytes == 0 ) {
		return false;
	}
	// Numeric address differences avoid relational comparison of unrelated pointers.
	const uintptr_t nLeft = reinterpret_cast<uintptr_t>( left.pBytes );
	const uintptr_t nRight = reinterpret_cast<uintptr_t>( right.pBytes );
	return nLeft <= nRight ? nRight - nLeft < left.nBytes : nLeft - nRight < right.nBytes;
}

SIRContainerResult SIRStringOps::copyTerminated( sirStringView_t view, char *pszOutput, uint32_t nCapacity ) noexcept
{
	if ( !isValid( view ) || pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes >= nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( view.nBytes != 0 ) {
		memmove( pszOutput, view.pBytes, view.nBytes );
	}
	pszOutput[view.nBytes] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

bool SIRStringOps::equals( sirStringView_t left, sirStringView_t right ) noexcept
{
	if ( left.nBytes != right.nBytes || !isValid( left ) || !isValid( right ) ) {
		return false;
	}
	return SIRStringSIMD::equals( left, right );
}

uint32_t SIRStringOps::findByte( sirStringView_t input, char nByte ) noexcept
{
	if ( !isValid( input ) || input.nBytes == 0 ) {
		return UINT32_MAX;
	}
	return SIRStringSIMD::findByte( input, nByte );
}

uint32_t SIRStringOps::find( sirStringView_t input, sirStringView_t pattern ) noexcept
{
	if ( !isValid( input ) || !isValid( pattern ) || pattern.nBytes > input.nBytes ) {
		return UINT32_MAX;
	}
	if ( pattern.nBytes == 0 ) {
		return 0;
	}
	if ( pattern.nBytes == 1 ) {
		return findByte( input, pattern.pBytes[0] );
	}
	uint32_t nPeriodA = 0;
	uint32_t nPeriodB = 0;
	const uint32_t nA = maximalSuffix( pattern, false, nPeriodA );
	const uint32_t nB = maximalSuffix( pattern, true, nPeriodB );
	const uint32_t nSplit = nA > nB ? nA : nB;
	uint32_t nPeriod = nA > nB ? nPeriodA : nPeriodB;
	uint32_t nMemoryAfterShift = 0;
	if ( nPeriod <= pattern.nBytes - nSplit && ( nSplit == 0 || memcmp( pattern.pBytes, pattern.pBytes + nPeriod, nSplit ) == 0 ) ) {
		nMemoryAfterShift = pattern.nBytes - nPeriod;
	} else {
		const uint32_t nLeft = nSplit == 0 ? 0 : nSplit - 1;
		const uint32_t nRight = pattern.nBytes - nSplit;
		nPeriod = ( nLeft > nRight ? nLeft : nRight ) + 1;
	}
	const uint32_t nLast = input.nBytes - pattern.nBytes;
	uint32_t iStart = 0;
	uint32_t nMemory = 0;
	while ( iStart <= nLast ) {
		uint32_t iMatch = nSplit > nMemory ? nSplit : nMemory;
		while ( iMatch < pattern.nBytes && pattern.pBytes[iMatch] == input.pBytes[iStart + iMatch] ) {
			++iMatch;
		}
		if ( iMatch < pattern.nBytes ) {
			const uint32_t nShift = iMatch - nSplit + 1;
			if ( nShift > nLast - iStart ) {
				return UINT32_MAX;
			}
			iStart += nShift;
			nMemory = 0;
			continue;
		}
		iMatch = nSplit;
		while ( iMatch > nMemory && pattern.pBytes[iMatch - 1] == input.pBytes[iStart + iMatch - 1] ) {
			--iMatch;
		}
		if ( iMatch <= nMemory ) {
			return iStart;
		}
		if ( nPeriod > nLast - iStart ) {
			return UINT32_MAX;
		}
		iStart += nPeriod;
		nMemory = nMemoryAfterShift;
	}
	return UINT32_MAX;
}

bool SIRStringOps::matchesCookedLZ( sirStringView_t query, const uint8_t *pEncoded, uint32_t nEncoded ) noexcept
{
	if ( !isValid( query ) || ( pEncoded == nullptr && nEncoded != 0 ) ) {
		return false;
	}
	uint32_t iInput = 0;
	uint32_t iOutput = 0;
	while ( iInput < nEncoded ) {
		const uint8_t nToken = pEncoded[iInput++];
		if ( nToken < 128 ) {
			const uint32_t nLength = static_cast<uint32_t>( nToken ) + 1;
			if ( nLength > nEncoded - iInput || nLength > query.nBytes - iOutput ) {
				return false;
			}
			if ( memcmp( query.pBytes + iOutput, pEncoded + iInput, nLength ) != 0 ) {
				return false;
			}
			iInput += nLength;
			iOutput += nLength;
		} else {
			const uint32_t nLength = ( nToken & 127u ) + 3u;
			if ( iInput == nEncoded || nLength > query.nBytes - iOutput ) {
				return false;
			}
			const uint32_t nDistance = pEncoded[iInput++];
			if ( nDistance == 0 || nDistance > iOutput ) {
				return false;
			}
			// Prior prefix already matches. Overlapping backreferences are periodic in the query itself.
			if ( memcmp( query.pBytes + iOutput, query.pBytes + iOutput - nDistance, nLength ) != 0 ) {
				return false;
			}
			iOutput += nLength;
		}
	}
	return iOutput == query.nBytes;
}

uint32_t SIRStringOps::maximalSuffix( sirStringView_t pattern, bool bReverse, uint32_t& nPeriod ) noexcept
{
	// Index iSplit represents maximal-suffix index plus one (zero is the -1 sentinel).
	uint32_t iSplit = 0;
	uint32_t iCandidate = 0;
	uint32_t nMatched = 1;
	nPeriod = 1;
	while ( nMatched < pattern.nBytes - iCandidate ) {
		const uint8_t nLeft = static_cast<uint8_t>( pattern.pBytes[iSplit + nMatched - 1] );
		const uint8_t nRight = static_cast<uint8_t>( pattern.pBytes[iCandidate + nMatched] );
		if ( nLeft == nRight ) {
			if ( nMatched == nPeriod ) {
				iCandidate += nPeriod;
				nMatched = 1;
			} else {
				++nMatched;
			}
		} else if ( bReverse ? nRight > nLeft : nRight < nLeft ) {
			iCandidate += nMatched;
			nMatched = 1;
			nPeriod = iCandidate - iSplit + 1;
		} else {
			iSplit = iCandidate + 1;
			iCandidate = iSplit;
			nMatched = 1;
			nPeriod = 1;
		}
	}
	return iSplit;
}
