/* SIREngine - MPL-2.0. Elias-Fano monotone integer sequence. */
#ifndef SIR_ELIAS_FANO_SET_H
#define SIR_ELIAS_FANO_SET_H
#include <Core/Containers/SIRRankSelectBitVector.h>
#include <Core/Containers/SIRVector.h>

class SIREliasFanoSet
{
	const sirContainerAllocator_t *m_pAllocator;
	SIRRankSelectBitVector m_high;
	SIRVector<uint64_t> m_low;
	uint32_t m_nSize = 0;
	uint32_t m_nLowBits = 0;
	uint64_t m_nLowMask = 0;
	static uint64_t readLow( const SIRVector<uint64_t>& words, uint32_t nBits, uint64_t nMask, uint32_t iIndex ) noexcept
	{
		if ( nBits == 0 ) {
			return 0;
		}
		const uint64_t nPosition = static_cast<uint64_t>(iIndex) * nBits;
		const uint32_t iWord = static_cast<uint32_t>( nPosition / 64 );
		const uint32_t nShift = static_cast<uint32_t>( nPosition % 64 );
		uint64_t nResult = words[iWord] >> nShift;
		if ( nShift + nBits > 64 ) {
			nResult |= words[iWord + 1] << ( 64 - nShift );
		}
		return nResult & nMask;
	}

public:
	explicit SIREliasFanoSet( const sirContainerAllocator_t *pAllocator )
	    : m_pAllocator( pAllocator ), m_high( pAllocator ), m_low( pAllocator )
	{
	}
	SIRContainerResult tryBuildSorted( const uint64_t *pValues, uint32_t nCount )
	{
		if ( nCount != 0 && pValues == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		for ( uint32_t iIndex = 1; iIndex < nCount; ++iIndex ) {
			if ( pValues[iIndex] <= pValues[iIndex - 1] ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
		}
		const uint64_t nMax = nCount == 0 ? 0 : pValues[nCount - 1];
		uint32_t nLowBits = 0;
		while ( nLowBits < 63 && ( nMax >> nLowBits ) > static_cast<uint64_t>(nCount) * 2 ) {
			++nLowBits;
		}
		if ( nLowBits > 0 && nCount > UINT64_MAX / nLowBits ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const uint64_t nLowTotal = static_cast<uint64_t>(nCount) * nLowBits;
		const uint64_t nHighBits = ( nMax >> nLowBits ) + nCount + 1;
		if ( nHighBits > UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( ( nLowTotal + 63 ) / 64 > UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		SIRVector<uint64_t> lows( m_pAllocator );
		SIRVector<uint64_t> highWords( m_pAllocator );
		SIRContainerResult eResult = lows.tryResize( static_cast<uint32_t>(  ( nLowTotal + 63 ) / 64  ) );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = highWords.tryResize( static_cast<uint32_t>(  ( nHighBits + 63 ) / 64  ) );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		const uint64_t nMask = nLowBits == 0 ? 0 : ( uint64_t( 1 ) << nLowBits ) - 1;
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			const uint64_t nBit = static_cast<uint64_t>(iIndex) * nLowBits;
			const uint32_t iWord = static_cast<uint32_t>( nBit / 64 );
			const uint32_t nShift = static_cast<uint32_t>( nBit % 64 );
			if ( nLowBits != 0 ) {
				lows[iWord] |= ( pValues[iIndex] & nMask ) << nShift;
				if ( nShift + nLowBits > 64 ) {
					lows[iWord + 1] |= ( pValues[iIndex] & nMask ) >> ( 64 - nShift );
				}
			}
			const uint64_t nHighBit = ( pValues[iIndex] >> nLowBits ) + iIndex;
			highWords[static_cast<uint32_t>( nHighBit / 64 )] |= uint64_t( 1 ) << ( nHighBit % 64 );
		}
		SIRRankSelectBitVector high( m_pAllocator );
		eResult = high.tryBuild( highWords.data(), static_cast<uint32_t>(nHighBits) );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		// The rank/select view has no implicit move assignment; install from staged words after successful validation.
		m_low = std::move( lows );
		m_high = std::move( high );
		m_nSize = nCount;
		m_nLowBits = nLowBits;
		m_nLowMask = nMask;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	uint64_t select( uint32_t iIndex ) const noexcept
	{
		if ( iIndex >= m_nSize ) {
			return UINT64_MAX;
		}
		return ( static_cast<uint64_t>(  m_high.select1( iIndex ) - iIndex  ) << m_nLowBits ) | readLow( m_low, m_nLowBits, m_nLowMask, iIndex );
	}
	bool contains( uint64_t nValue ) const noexcept
	{
		uint32_t nLo = 0;
		uint32_t nHi = m_nSize;
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( select( iMid ) < nValue ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		return nLo < m_nSize && select( nLo ) == nValue;
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
};
#endif
