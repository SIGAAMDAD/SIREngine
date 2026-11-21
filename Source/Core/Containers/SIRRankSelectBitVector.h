/* SIREngine - MPL-2.0. Rank/select over packed bits, rebuilt transactionally. */
#ifndef SIR_RANK_SELECT_BIT_VECTOR_H
#define SIR_RANK_SELECT_BIT_VECTOR_H
#include <Core/Containers/SIRVector.h>
#include <stdint.h>

class SIRRankSelectBitVector
{
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<uint64_t> m_words;
	SIRVector<uint64_t> m_prefix;
	uint32_t m_nBits = 0;
	static uint32_t pop( uint64_t nWord ) noexcept
	{
#if defined( __clang__ ) || defined( __GNUC__ )
		return ( uint32_t )__builtin_popcountll( nWord );
#else
		uint32_t nCount = 0;
		while ( nWord ) {
			nWord &= nWord - 1;
			++nCount;
		}
		return nCount;
#endif
	}

	// Find the nth set bit inside a nonempty 64-bit word using six bounded
	// population counts instead of clearing up to 63 preceding bits.
	static uint32_t selectBitInWord( uint64_t nWord, uint32_t nOrdinal ) noexcept
	{
		uint32_t iOffset = 0;
		for ( uint32_t nWidth = 32; nWidth != 0; nWidth >>= 1 ) {
			const uint64_t nLowerMask = ( uint64_t( 1 ) << nWidth ) - 1;
			const uint32_t nLowerCount = pop( nWord & nLowerMask );
			if ( nOrdinal >= nLowerCount ) {
				nOrdinal -= nLowerCount;
				iOffset += nWidth;
				nWord >>= nWidth;
			}
		}
		return iOffset;
	}

public:
	explicit SIRRankSelectBitVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator ), m_words( pAllocator ), m_prefix( pAllocator )
	{
	}
	sirContainerResult_t tryBuild( const uint64_t *pWords, uint32_t nBits )
	{
		if ( nBits != 0 && pWords == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint32_t nWords = ( nBits + 63ull ) / 64;
		SIRVector<uint64_t> words( m_pAllocator );
		SIRVector<uint64_t> prefix( m_pAllocator );
		sirContainerResult_t eResult = words.tryReserve( nWords );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = prefix.tryReserve( nWords + 1 );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint64_t nCount = 0;
		prefix.tryEmplaceBack( uint64_t( 0 ) );
		for ( uint32_t iWord = 0; iWord < nWords; ++iWord ) {
			uint64_t nValue = pWords[iWord];
			if ( iWord + 1 == nWords && nBits % 64 != 0 ) {
				nValue &= ( uint64_t( 1 ) << ( nBits % 64 ) ) - 1;
			}
			words.tryEmplaceBack( nValue );
			nCount += pop( nValue );
			prefix.tryEmplaceBack( nCount );
		}
		m_words = std::move( words );
		m_prefix = std::move( prefix );
		m_nBits = nBits;
		return SIR_CONTAINER_OK;
	}
	uint64_t rank1( uint32_t nExclusive ) const noexcept
	{
		if ( nExclusive > m_nBits ) {
			nExclusive = m_nBits;
		}
		const uint32_t iWord = nExclusive / 64;
		uint64_t nResult = m_prefix[iWord];
		if ( nExclusive % 64 != 0 ) {
			nResult += pop( m_words[iWord] & ( ( uint64_t( 1 ) << ( nExclusive % 64 ) ) - 1 ) );
		}
		return nResult;
	}
	uint32_t select1( uint64_t nOrdinal ) const noexcept
	{
		if ( m_prefix.isEmpty() || nOrdinal >= m_prefix[m_prefix.getSize() - 1] ) {
			return UINT32_MAX;
		}
		uint32_t nLo = 0;
		uint32_t nHi = m_words.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( m_prefix[iMid + 1] <= nOrdinal ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		uint64_t nWord = m_words[nLo];
		uint64_t nRemaining = nOrdinal - m_prefix[nLo];
		while ( nRemaining-- != 0 ) {
			nWord &= nWord - 1;
		}
#if defined( __clang__ ) || defined( __GNUC__ )
		return nLo * 64 + ( uint32_t )__builtin_ctzll( nWord );
#else
		uint32_t nBit = 0;
		while ( ( nWord & 1 ) == 0 ) {
			nWord >>= 1;
			++nBit;
		}
		return nLo * 64 + nBit;
#endif
	}
	// Locate the zero-based ordinal among unset bits. Tail bits beyond
	// m_nBits must not be counted, including in the last partial word.
	// Optional faster choice for repeated wavelet-partition inversion; the
	// public select1 path retains its faster low-ordinal behavior.
	uint32_t select1Fast( uint64_t nOrdinal ) const noexcept
	{
		if ( m_prefix.isEmpty() || nOrdinal >= m_prefix[m_prefix.getSize() - 1] ) {
			return UINT32_MAX;
		}
		uint32_t nLo = 0;
		uint32_t nHi = m_words.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( m_prefix[iMid + 1] <= nOrdinal ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		return nLo * 64 + selectBitInWord( m_words[nLo], ( uint32_t )( nOrdinal - m_prefix[nLo] ) );
	}
	uint32_t select0( uint64_t nOrdinal ) const noexcept
	{
		if ( nOrdinal >= ( uint64_t )m_nBits - getOnes() ) {
			return UINT32_MAX;
		}
		uint32_t nLo = 0;
		uint32_t nHi = m_words.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			const uint64_t nEnd = ( uint64_t )( iMid + 1 ) * 64 < m_nBits ? ( uint64_t )( iMid + 1 ) * 64 : m_nBits;
			if ( nEnd - m_prefix[iMid + 1] <= nOrdinal ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		uint64_t nWord = ~m_words[nLo];
		const uint32_t nTail = m_nBits % 64;
		if ( nTail != 0 && nLo + 1 == m_words.getSize() ) {
			nWord &= ( uint64_t( 1 ) << nTail ) - 1;
		}
		return nLo * 64 + selectBitInWord( nWord, ( uint32_t )( nOrdinal - ( ( uint64_t )nLo * 64 - m_prefix[nLo] ) ) );
	}
	bool contains( uint32_t iBit ) const noexcept
	{
		return iBit < m_nBits && ( m_words[iBit / 64] & ( uint64_t( 1 ) << ( iBit % 64 ) ) ) != 0;
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nBits;
	}
	uint64_t getOnes( void ) const noexcept
	{
		return m_prefix.isEmpty() ? 0 : m_prefix[m_prefix.getSize() - 1];
	}
};
#endif
