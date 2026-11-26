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

#include <Core/Strings/SIROffsetIndex.h>

SIRContainerResult SIROffsetIndex::tryGetLayout( uint32_t nCount, uint32_t nMaximum, sirOffsetIndexLayout_t& layout ) noexcept
{
	if ( nCount == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t nRatio = nMaximum / nCount;
	uint32_t nLowBits = 0;
	while ( nRatio > 1 ) {
		nRatio >>= 1;
		++nLowBits;
	}
	const uint64_t nHighBits = static_cast<uint64_t>( nMaximum >> nLowBits ) + nCount;
	if ( nHighBits > UINT32_MAX ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const uint64_t nLowWords = ( static_cast<uint64_t>( nCount ) * nLowBits + 63 ) / 64;
	const uint64_t nHighWords = ( nHighBits + 63 ) / 64;
	if ( nLowWords > UINT32_MAX / sizeof( uint64_t ) || nHighWords > UINT32_MAX / sizeof( uint64_t ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	layout = { static_cast<uint32_t>( nLowWords ),
	           static_cast<uint32_t>( nHighWords ),
	           static_cast<uint32_t>( ( static_cast<uint64_t>( nCount ) + SAMPLE_STRIDE - 1 ) / SAMPLE_STRIDE ),
	           nLowBits };
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIROffsetIndex::tryBuild( const uint32_t *pOffsets, uint32_t nCount, const sirOffsetIndexStorage_t& storage ) noexcept
{
	if ( pOffsets == nullptr || nCount == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 1; i < nCount; ++i ) {
		if ( pOffsets[i] < pOffsets[i - 1] ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	sirOffsetIndexLayout_t layout{};
	const SIRContainerResult eResult = tryGetLayout( nCount, pOffsets[nCount - 1], layout );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	if ( storage.capacity.nLowWords < layout.nLowWords || storage.capacity.nHighWords < layout.nHighWords || storage.capacity.nSamples < layout.nSamples ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( storage.pHigh == nullptr || storage.pSamples == nullptr || ( layout.nLowWords != 0 && storage.pLow == nullptr ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( layout.nLowWords != 0 ) {
		memset( storage.pLow, 0, static_cast<size_t>( layout.nLowWords ) * sizeof( uint64_t ) );
	}
	memset( storage.pHigh, 0, static_cast<size_t>( layout.nHighWords ) * sizeof( uint64_t ) );
	const uint64_t nMask = ( uint64_t{ 1 } << layout.nLowBits ) - 1;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const uint32_t nValue = pOffsets[i];
		const uint32_t iHigh = ( nValue >> layout.nLowBits ) + i;
		storage.pHigh[iHigh / 64] |= uint64_t{ 1 } << ( iHigh % 64 );
		if ( i % SAMPLE_STRIDE == 0 ) {
			storage.pSamples[i / SAMPLE_STRIDE] = iHigh;
		}
		if ( layout.nLowBits == 0 ) {
			continue;
		}
		const uint64_t iBit = static_cast<uint64_t>( i ) * layout.nLowBits;
		const uint32_t iWord = static_cast<uint32_t>( iBit / 64 );
		const uint32_t nShift = static_cast<uint32_t>( iBit % 64 );
		const uint64_t nLow = nValue & nMask;
		storage.pLow[iWord] |= nLow << nShift;
		if ( nShift + layout.nLowBits > 64 ) {
			storage.pLow[iWord + 1] |= nLow >> ( 64 - nShift );
		}
	}
	m_pLow = storage.pLow;
	m_pHigh = storage.pHigh;
	m_pSamples = storage.pSamples;
	m_nCount = nCount;
	m_nLowBits = layout.nLowBits;
	m_nHighWords = layout.nHighWords;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

uint32_t SIROffsetIndex::countBits( uint64_t nWord ) noexcept
{
	nWord -= ( nWord >> 1 ) & UINT64_C( 0x5555555555555555 );
	nWord = ( nWord & UINT64_C( 0x3333333333333333 ) ) + ( ( nWord >> 2 ) & UINT64_C( 0x3333333333333333 ) );
	nWord = ( nWord + ( nWord >> 4 ) ) & UINT64_C( 0x0f0f0f0f0f0f0f0f );
	return static_cast<uint32_t>( ( nWord * UINT64_C( 0x0101010101010101 ) ) >> 56 );
}

uint32_t SIROffsetIndex::firstBit( uint64_t nWord ) noexcept
{
	// Count lower zero bits using the isolated least significant one.
	return countBits( ( nWord & ( uint64_t{ 0 } - nWord ) ) - 1 );
}

uint32_t SIROffsetIndex::select( uint32_t iOffset ) const noexcept
{
	const uint32_t iSample = m_pSamples[iOffset / SAMPLE_STRIDE];
	uint32_t nSkip = iOffset % SAMPLE_STRIDE;
	if ( nSkip == 0 ) {
		return iSample;
	}
	uint32_t iWord = iSample / 64;
	uint64_t nWord = m_pHigh[iWord] & ( UINT64_MAX << ( iSample % 64 ) );
	for ( ; iWord < m_nHighWords; ++iWord ) {
		const uint32_t nOnes = countBits( nWord );
		if ( nSkip < nOnes ) {
			while ( nSkip != 0 ) {
				nWord &= nWord - 1;
				--nSkip;
			}
			return iWord * 64 + firstBit( nWord );
		}
		nSkip -= nOnes;
		if ( iWord + 1 < m_nHighWords ) {
			nWord = m_pHigh[iWord + 1];
		}
	}
	return UINT32_MAX;
}

SIRContainerResult SIROffsetIndex::tryGet( uint32_t iOffset, uint32_t& nOffset ) const noexcept
{
	if ( iOffset >= m_nCount ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t iHigh = select( iOffset );
	if ( iHigh == UINT32_MAX || iHigh < iOffset ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint64_t nLow = 0;
	if ( m_nLowBits != 0 ) {
		const uint64_t iBit = static_cast<uint64_t>( iOffset ) * m_nLowBits;
		const uint32_t iWord = static_cast<uint32_t>( iBit / 64 );
		const uint32_t nShift = static_cast<uint32_t>( iBit % 64 );
		nLow = m_pLow[iWord] >> nShift;
		if ( nShift + m_nLowBits > 64 ) {
			nLow |= m_pLow[iWord + 1] << ( 64 - nShift );
		}
		nLow &= ( uint64_t{ 1 } << m_nLowBits ) - 1;
	}
	const uint64_t nValue = ( static_cast<uint64_t>( iHigh - iOffset ) << m_nLowBits ) | nLow;
	if ( nValue > UINT32_MAX ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	nOffset = static_cast<uint32_t>( nValue );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIROffsetIndex::tryResolve( uint32_t iString, sirStringView_t table, sirStringView_t& output ) const noexcept
{
	if ( m_nCount == 0 || iString >= m_nCount - 1 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iBegin = 0;
	uint32_t iEnd = 0;
	if ( tryGet( iString, iBegin ) != SIRContainerResult::SIR_CONTAINER_OK || tryGet( iString + 1, iEnd ) != SIRContainerResult::SIR_CONTAINER_OK ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	SIROffsetString offset;
	const SIRContainerResult eResult = SIROffsetString::tryFromBoundaries( iBegin, iEnd, offset );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	return offset.tryResolve( table, output );
}
