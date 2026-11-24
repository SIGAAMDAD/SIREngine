/* SIREngine - MPL-2.0. Immutable rank/access wavelet matrix. */
#ifndef SIR_SUCCINCT_WAVELET_MATRIX_H
#define SIR_SUCCINCT_WAVELET_MATRIX_H
#include <Core/Containers/SIRRankSelectBitVector.h>
#include <Core/Containers/SIRVector.h>

class SIRSuccinctWaveletMatrix
{
	struct Level
	{
		SIRRankSelectBitVector bits;
		uint32_t nZeros = 0;
		explicit Level( const sirContainerAllocator_t *pAllocator ) : bits( pAllocator )
		{
		}
	};
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<Level> m_levels;
	uint32_t m_nCount = 0;
	uint32_t m_nBits = 0;

public:
	explicit SIRSuccinctWaveletMatrix( const sirContainerAllocator_t *pAllocator ) : m_pAllocator( pAllocator ), m_levels( pAllocator )
	{
	}
	SIRContainerResult tryBuild( const uint32_t *pValues, uint32_t nCount, uint32_t nBits = 32 )
	{
		if ( nBits == 0 || nBits > 32 || ( nCount > 0 && pValues == nullptr ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<uint32_t> current( m_pAllocator );
		SIRVector<uint32_t> next( m_pAllocator );
		SIRVector<Level> levels( m_pAllocator );
		SIRContainerResult eResult = current.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = next.tryResize( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = levels.tryReserve( nBits );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			if ( nBits < 32 && pValues[iIndex] >= ( uint32_t( 1 ) << nBits ) ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
			current.tryEmplaceBack( pValues[iIndex] );
		}
		for ( uint32_t iLevel = 0; iLevel < nBits; ++iLevel ) {
			const uint32_t nShift = nBits - 1 - iLevel;
			SIRVector<uint64_t> words( m_pAllocator );
			eResult = words.tryResize( ( nCount + 63ull ) / 64 );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			uint32_t nZeros = 0;
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				if ( ( ( current[iIndex] >> nShift ) & 1 ) != 0 ) {
					words[iIndex / 64] |= uint64_t( 1 ) << ( iIndex % 64 );
				} else {
					++nZeros;
				}
			}
			eResult = levels.tryEmplaceBack( m_pAllocator );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			Level& level = levels[levels.getSize() - 1];
			eResult = level.bits.tryBuild( words.data(), nCount );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			level.nZeros = nZeros;
			uint32_t iZero = 0;
			uint32_t iOne = nZeros;
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				if ( ( ( current[iIndex] >> nShift ) & 1 ) != 0 ) {
					next[iOne++] = current[iIndex];
				} else {
					next[iZero++] = current[iIndex];
				}
			}
			current = std::move( next );
			next = SIRVector<uint32_t>( m_pAllocator );
			eResult = next.tryResize( nCount );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		m_levels = std::move( levels );
		m_nCount = nCount;
		m_nBits = nBits;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	uint32_t access( uint32_t iIndex ) const noexcept
	{
		if ( iIndex >= m_nCount ) {
			return UINT32_MAX;
		}
		uint32_t nValue = 0;
		for ( uint32_t iLevel = 0; iLevel < m_nBits; ++iLevel ) {
			const Level& level = m_levels[iLevel];
			const uint64_t nOnes = level.bits.rank1( iIndex );
			if ( level.bits.contains( iIndex ) ) {
				nValue |= uint32_t( 1 ) << ( m_nBits - 1 - iLevel );
				iIndex = level.nZeros + static_cast<uint32_t>(nOnes);
			} else {
				iIndex -= static_cast<uint32_t>(nOnes);
			}
		}
		return nValue;
	}
	uint32_t rank( uint32_t nValue, uint32_t nExclusive ) const noexcept
	{
		if ( m_nBits < 32 && nValue >= ( uint32_t( 1 ) << m_nBits ) ) {
			return 0;
		}
		if ( nExclusive > m_nCount ) {
			nExclusive = m_nCount;
		}
		uint32_t iFirst = 0;
		uint32_t iLast = nExclusive;
		for ( uint32_t iLevel = 0; iLevel < m_nBits; ++iLevel ) {
			const Level& level = m_levels[iLevel];
			const uint32_t nStartOnes = static_cast<uint32_t>(level.bits.rank1( iFirst ));
			const uint32_t nEndOnes = static_cast<uint32_t>(level.bits.rank1( iLast ));
			if ( ( ( nValue >> ( m_nBits - 1 - iLevel ) ) & 1 ) != 0 ) {
				iFirst = level.nZeros + nStartOnes;
				iLast = level.nZeros + nEndOnes;
			} else {
				iFirst -= nStartOnes;
				iLast -= nEndOnes;
			}
		}
		return iLast - iFirst;
	}
	// Descend to the value's final interval, then invert each stable
	// partition with select0/select1. This avoids O(log N) full rank walks.
	uint32_t select( uint32_t nValue, uint32_t nOrdinal ) const noexcept
	{
		if ( m_nBits == 0 ) {
			return UINT32_MAX;
		}
		if ( m_nBits < 32 && nValue >= ( uint32_t( 1 ) << m_nBits ) ) {
			return UINT32_MAX;
		}
		uint32_t iFirst = 0;
		uint32_t iLast = m_nCount;
		for ( uint32_t iLevel = 0; iLevel < m_nBits; ++iLevel ) {
			const Level& level = m_levels[iLevel];
			const uint32_t nFirstOnes = static_cast<uint32_t>(level.bits.rank1( iFirst ));
			const uint32_t nLastOnes = static_cast<uint32_t>(level.bits.rank1( iLast ));
			const uint32_t nBit = ( nValue >> ( m_nBits - iLevel - 1 ) ) & 1;
			if ( nBit != 0 ) {
				iFirst = level.nZeros + nFirstOnes;
				iLast = level.nZeros + nLastOnes;
			} else {
				iFirst -= nFirstOnes;
				iLast -= nLastOnes;
			}
		}
		if ( nOrdinal >= iLast - iFirst ) {
			return UINT32_MAX;
		}
		uint32_t iPosition = iFirst + nOrdinal;
		for ( uint32_t iLevel = m_nBits; iLevel != 0; --iLevel ) {
			const Level& level = m_levels[iLevel - 1];
			const uint32_t nBit = ( nValue >> ( m_nBits - iLevel ) ) & 1;
			if ( nBit != 0 ) {
				iPosition = level.bits.select1Fast( iPosition - level.nZeros );
			} else {
				iPosition = level.bits.select0( iPosition );
			}
		}
		return iPosition;
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nCount;
	}
};
#endif
