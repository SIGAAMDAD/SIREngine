/* SIREngine - MPL-2.0. Adaptive high16 directory and array/dense Roaring chunks. */
#ifndef SIR_ADAPTIVE_ROARING_DIRECTORY_H
#define SIR_ADAPTIVE_ROARING_DIRECTORY_H
#include <Core/Containers/SIRVector.h>
#include <algorithm>

class SIRAdaptiveRoaringDirectory
{
	struct Chunk
	{
		uint16_t nHigh;
		uint32_t nCount = 0;
		bool bDense = false;
		SIRVector<uint16_t> lows;
		SIRVector<uint64_t> words;
		explicit Chunk( uint16_t nValue, const sirContainerAllocator_t *pAllocator )
		    : nHigh( nValue ), lows( pAllocator ), words( pAllocator )
		{
		}
		Chunk( Chunk&& ) noexcept = default;
		Chunk& operator=( Chunk&& ) noexcept = default;
		Chunk( const Chunk& ) = delete;
		Chunk& operator=( const Chunk& ) = delete;
		static uint32_t pop( uint64_t nBits ) noexcept
		{
#if defined( __clang__ ) || defined( __GNUC__ )
			return static_cast<uint32_t>(__builtin_popcountll( nBits ));
#else
			uint32_t nCount = 0;
			while ( nBits ) {
				nBits &= nBits - 1;
				++nCount;
			}
			return nCount;
#endif
		}
		bool contains( uint16_t nLow ) const noexcept
		{
			if ( bDense ) {
				return ( words[nLow / 64] & ( uint64_t( 1 ) << ( nLow % 64 ) ) ) != 0;
			}
			uint32_t nLo = 0;
			uint32_t nHi = lows.getSize();
			while ( nLo < nHi ) {
				const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
				if ( lows[iMid] < nLow ) {
					nLo = iMid + 1;
				} else {
					nHi = iMid;
				}
			}
			return nLo < lows.getSize() && lows[nLo] == nLow;
		}
		SIRContainerResult tryAdd( uint16_t nLow, const sirContainerAllocator_t *pAllocator, bool *pInserted = nullptr )
		{
			if ( contains( nLow ) ) {
				return SIRContainerResult::SIR_CONTAINER_OK;
			}
			if ( bDense ) {
				words[nLow / 64] |= uint64_t( 1 ) << ( nLow % 64 );
				++nCount;
				if ( pInserted != nullptr ) {
					*pInserted = true;
				}
				return SIRContainerResult::SIR_CONTAINER_OK;
			}
			if ( nCount == 4096 ) {
				SIRVector<uint64_t> staged( pAllocator );
				SIRContainerResult eResult = staged.tryResize( 1024 );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
				for ( uint32_t iIndex = 0; iIndex < lows.getSize(); ++iIndex ) {
					const uint32_t nValue = lows[iIndex];
					staged[nValue / 64] |= uint64_t( 1 ) << ( nValue % 64 );
				}
				staged[nLow / 64] |= uint64_t( 1 ) << ( nLow % 64 );
				words = std::move( staged );
				lows.clear();
				bDense = true;
				++nCount;
				if ( pInserted != nullptr ) {
					*pInserted = true;
				}
				return SIRContainerResult::SIR_CONTAINER_OK;
			}
			uint32_t nLo = 0;
			uint32_t nHi = lows.getSize();
			while ( nLo < nHi ) {
				const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
				if ( lows[iMid] < nLow ) {
					nLo = iMid + 1;
				} else {
					nHi = iMid;
				}
			}
			SIRContainerResult eResult = lows.tryEmplaceBack( nLow );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			for ( uint32_t iMove = lows.getSize() - 1; iMove > nLo; --iMove ) {
				lows[iMove] = lows[iMove - 1];
			}
			lows[nLo] = nLow;
			++nCount;
			if ( pInserted != nullptr ) {
				*pInserted = true;
			}
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		bool erase( uint16_t nLow ) noexcept
		{
			if ( bDense ) {
				const uint64_t nMask = uint64_t( 1 ) << ( nLow % 64 );
				uint64_t& nWord = words[nLow / 64];
				if ( ( nWord & nMask ) == 0 ) {
					return false;
				}
				nWord &= ~nMask;
			} else {
				uint32_t nLo = 0;
				uint32_t nHi = lows.getSize();
				while ( nLo < nHi ) {
					const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
					if ( lows[iMid] < nLow ) {
						nLo = iMid + 1;
					} else {
						nHi = iMid;
					}
				}
				if ( nLo == lows.getSize() || lows[nLo] != nLow ) {
					return false;
				}
				lows.tryEraseOrdered( nLo );
			}
			--nCount;
			return true;
		}
		template <typename Visitor>
		void forEach( Visitor&& visitor ) const
		{
			if ( !bDense ) {
				for ( uint32_t iIndex = 0; iIndex < lows.getSize(); ++iIndex ) {
					visitor( lows[iIndex] );
				}
				return;
			}
			for ( uint32_t iWord = 0; iWord < words.getSize(); ++iWord ) {
				uint64_t nBits = words[iWord];
				while ( nBits != 0 ) {
#if defined( __clang__ ) || defined( __GNUC__ )
					const uint32_t nLow = iWord * 64 + static_cast<uint32_t>(__builtin_ctzll( nBits ));
#else
					uint32_t nLow = iWord * 64;
					uint64_t nTmp = nBits;
					while ( ( nTmp & 1 ) == 0 ) {
						nTmp >>= 1;
						++nLow;
					}
#endif
					visitor( static_cast<uint16_t>(nLow) );
					nBits &= nBits - 1;
				}
			}
		}
	};
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<Chunk> m_chunks;
	uint64_t m_nCount = 0;
	// Build a private copy of a chunk without per-value lookup or insertion.
	// The caller publishes only after every allocation has succeeded.
	static SIRContainerResult tryCopyChunk( const Chunk& source, Chunk& target )
	{
		target.nCount = source.nCount;
		target.bDense = source.bDense;
		if ( source.bDense ) {
			SIRContainerResult eResult = target.words.tryReserve( source.words.getSize() );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			for ( uint32_t iWord = 0; iWord < source.words.getSize(); ++iWord ) {
				target.words.tryEmplaceBack( source.words[iWord] );
			}
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		SIRContainerResult eResult = target.lows.tryReserve( source.lows.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iLow = 0; iLow < source.lows.getSize(); ++iLow ) {
			target.lows.tryEmplaceBack( source.lows[iLow] );
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	static SIRContainerResult tryUnionChunks( const Chunk& left, const Chunk& right, Chunk& target )
	{
		if ( left.bDense || right.bDense ) {
			SIRContainerResult eResult = target.words.tryResize( 1024 );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			const Chunk& dense = left.bDense ? left : right;
			const Chunk& other = left.bDense ? right : left;
			for ( uint32_t iWord = 0; iWord < 1024; ++iWord ) {
				target.words[iWord] = dense.words[iWord];
			}
			if ( other.bDense ) {
				for ( uint32_t iWord = 0; iWord < 1024; ++iWord ) {
					target.words[iWord] |= other.words[iWord];
				}
			} else {
				for ( uint32_t iLow = 0; iLow < other.lows.getSize(); ++iLow ) {
					const uint16_t nLow = other.lows[iLow];
					target.words[nLow / 64] |= uint64_t( 1 ) << ( nLow % 64 );
				}
			}
			for ( uint32_t iWord = 0; iWord < 1024; ++iWord ) {
				target.nCount += Chunk::pop( target.words[iWord] );
			}
			target.bDense = true;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}

		// The sparse arrays are already ordered and unique. Merge them in
		// linear time, then convert the result if it exceeds array capacity.
		const uint32_t nUpperBound = left.nCount + right.nCount;
		SIRContainerResult eResult = target.lows.tryReserve( nUpperBound );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint32_t iLeft = 0;
		uint32_t iRight = 0;
		while ( iLeft < left.lows.getSize() && iRight < right.lows.getSize() ) {
			const uint16_t nLeft = left.lows[iLeft];
			const uint16_t nRight = right.lows[iRight];
			const uint16_t nNext = nLeft < nRight ? nLeft : nRight;
			target.lows.tryEmplaceBack( nNext );
			iLeft += nLeft == nNext;
			iRight += nRight == nNext;
		}
		while ( iLeft < left.lows.getSize() ) {
			target.lows.tryEmplaceBack( left.lows[iLeft++] );
		}
		while ( iRight < right.lows.getSize() ) {
			target.lows.tryEmplaceBack( right.lows[iRight++] );
		}
		target.nCount = target.lows.getSize();
		if ( target.nCount <= 4096 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		eResult = target.words.tryResize( 1024 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iLow = 0; iLow < target.lows.getSize(); ++iLow ) {
			const uint16_t nLow = target.lows[iLow];
			target.words[nLow / 64] |= uint64_t( 1 ) << ( nLow % 64 );
		}
		target.lows.clear();
		target.bDense = true;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	uint32_t lowerBound( uint16_t nHigh ) const noexcept
	{
		uint32_t nLo = 0;
		uint32_t nHi = m_chunks.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( m_chunks[iMid].nHigh < nHigh ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		return nLo;
	}

public:
	explicit SIRAdaptiveRoaringDirectory( const sirContainerAllocator_t *pAllocator ) : m_pAllocator( pAllocator ), m_chunks( pAllocator )
	{
	}
	SIRContainerResult tryAdd( uint32_t nValue )
	{
		const uint16_t nHigh = static_cast<uint16_t>( nValue >> 16 );
		const uint16_t nLow = static_cast<uint16_t>(nValue);
		const uint32_t iChunk = lowerBound( nHigh );
		if ( iChunk < m_chunks.getSize() && m_chunks[iChunk].nHigh == nHigh ) {
			bool bInserted = false;
			SIRContainerResult eResult = m_chunks[iChunk].tryAdd( nLow, m_pAllocator, &bInserted );
			if ( eResult == SIRContainerResult::SIR_CONTAINER_OK && bInserted ) {
				++m_nCount;
			}
			return eResult;
		}
		Chunk staged( nHigh, m_pAllocator );
		SIRContainerResult eResult = staged.tryAdd( nLow, m_pAllocator );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = m_chunks.tryEmplaceBack( std::move( staged ) );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iMove = m_chunks.getSize() - 1; iMove > iChunk; --iMove ) {
			Chunk hold = std::move( m_chunks[iMove] );
			m_chunks[iMove] = std::move( m_chunks[iMove - 1] );
			m_chunks[iMove - 1] = std::move( hold );
		}
		++m_nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	bool erase( uint32_t nValue ) noexcept
	{
		const uint16_t nHigh = static_cast<uint16_t>( nValue >> 16 );
		const uint32_t iChunk = lowerBound( nHigh );
		if ( iChunk == m_chunks.getSize() || m_chunks[iChunk].nHigh != nHigh ) {
			return false;
		}
		if ( !m_chunks[iChunk].erase( static_cast<uint16_t>(nValue) ) ) {
			return false;
		}
		--m_nCount;
		if ( m_chunks[iChunk].nCount == 0 ) {
			m_chunks.tryEraseOrdered( iChunk );
		}
		return true;
	}
	bool contains( uint32_t nValue ) const noexcept
	{
		const uint16_t nHigh = static_cast<uint16_t>( nValue >> 16 );
		const uint32_t iChunk = lowerBound( nHigh );
		return iChunk < m_chunks.getSize() && m_chunks[iChunk].nHigh == nHigh && m_chunks[iChunk].contains( static_cast<uint16_t>(nValue) );
	}
	template <typename Visitor>
	void forEach( Visitor&& visitor ) const
	{
		for ( uint32_t iChunk = 0; iChunk < m_chunks.getSize(); ++iChunk ) {
			const uint32_t nBase = uint32_t( m_chunks[iChunk].nHigh ) << 16;
			m_chunks[iChunk].forEach( [&]( uint16_t nLow ) { visitor( nBase | nLow ); } );
		}
	}
	SIRContainerResult tryUnionWith( const SIRAdaptiveRoaringDirectory& other )
	{
		if ( this == &other || other.m_chunks.isEmpty() ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		SIRAdaptiveRoaringDirectory staged( m_pAllocator );
		SIRContainerResult eResult = staged.m_chunks.tryReserve( m_chunks.getSize() + other.m_chunks.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint32_t iLeft = 0;
		uint32_t iRight = 0;
		while ( iLeft < m_chunks.getSize() || iRight < other.m_chunks.getSize() ) {
			const Chunk *pLeft = iLeft < m_chunks.getSize() ? &m_chunks[iLeft] : nullptr;
			const Chunk *pRight = iRight < other.m_chunks.getSize() ? &other.m_chunks[iRight] : nullptr;
			const bool bTakeLeft = pRight == nullptr || ( pLeft != nullptr && pLeft->nHigh < pRight->nHigh );
			const bool bTakeRight = pLeft == nullptr || ( pRight != nullptr && pRight->nHigh < pLeft->nHigh );
			Chunk result( bTakeLeft ? pLeft->nHigh : pRight->nHigh, m_pAllocator );
			if ( bTakeLeft ) {
				eResult = tryCopyChunk( *pLeft, result );
				++iLeft;
			} else if ( bTakeRight ) {
				eResult = tryCopyChunk( *pRight, result );
				++iRight;
			} else {
				eResult = tryUnionChunks( *pLeft, *pRight, result );
				++iLeft;
				++iRight;
			}
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			staged.m_nCount += result.nCount;
			eResult = staged.m_chunks.tryEmplaceBack( std::move( result ) );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		m_chunks = std::move( staged.m_chunks );
		m_nCount = staged.m_nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	// Transactionally intersect matching high-key chunks. The dense-dense
	// case operates on whole words and sparse cases visit only existing lows.
	SIRContainerResult tryIntersectWith( const SIRAdaptiveRoaringDirectory& other )
	{
		if ( this == &other ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		SIRAdaptiveRoaringDirectory staged( m_pAllocator );
		const uint32_t nMaxChunks = m_chunks.getSize() < other.m_chunks.getSize() ? m_chunks.getSize() : other.m_chunks.getSize();
		SIRContainerResult eResult = staged.m_chunks.tryReserve( nMaxChunks );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint32_t iLeft = 0;
		uint32_t iRight = 0;
		while ( iLeft < m_chunks.getSize() && iRight < other.m_chunks.getSize() ) {
			const Chunk& left = m_chunks[iLeft];
			const Chunk& right = other.m_chunks[iRight];
			if ( left.nHigh != right.nHigh ) {
				iLeft += left.nHigh < right.nHigh;
				iRight += right.nHigh < left.nHigh;
				continue;
			}
			++iLeft;
			++iRight;
			Chunk result( left.nHigh, m_pAllocator );
			if ( left.bDense && right.bDense ) {
				eResult = result.words.tryResize( 1024 );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
				for ( uint32_t iWord = 0; iWord < 1024; ++iWord ) {
					const uint64_t nBits = left.words[iWord] & right.words[iWord];
					result.words[iWord] = nBits;
					result.nCount += Chunk::pop( nBits );
				}
				result.bDense = true;
				if ( result.nCount != 0 && result.nCount <= 4096 ) {
					eResult = result.lows.tryReserve( result.nCount );
					if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
						return eResult;
					}
					result.forEach( [&]( uint16_t nLow ) { result.lows.tryEmplaceBack( nLow ); } );
					result.words.clear();
					result.bDense = false;
				}
			} else if ( !left.bDense && !right.bDense ) {
				const uint32_t nCapacity = left.nCount < right.nCount ? left.nCount : right.nCount;
				eResult = result.lows.tryReserve( nCapacity );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
				uint32_t iLowLeft = 0;
				uint32_t iLowRight = 0;
				while ( iLowLeft < left.lows.getSize() && iLowRight < right.lows.getSize() ) {
					const uint16_t nLeft = left.lows[iLowLeft];
					const uint16_t nRight = right.lows[iLowRight];
					if ( nLeft == nRight ) {
						result.lows.tryEmplaceBack( nLeft );
					}
					iLowLeft += nLeft <= nRight;
					iLowRight += nRight <= nLeft;
				}
				result.nCount = result.lows.getSize();
			} else {
				const Chunk& sparse = left.bDense ? right : left;
				const Chunk& dense = left.bDense ? left : right;
				eResult = result.lows.tryReserve( sparse.nCount );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
				for ( uint32_t iLow = 0; iLow < sparse.lows.getSize(); ++iLow ) {
					const uint16_t nLow = sparse.lows[iLow];
					if ( ( dense.words[nLow / 64] & ( uint64_t( 1 ) << ( nLow % 64 ) ) ) != 0 ) {
						result.lows.tryEmplaceBack( nLow );
					}
				}
				result.nCount = result.lows.getSize();
			}
			if ( result.nCount == 0 ) {
				continue;
			}
			staged.m_nCount += result.nCount;
			eResult = staged.m_chunks.tryEmplaceBack( std::move( result ) );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		m_chunks = std::move( staged.m_chunks );
		m_nCount = staged.m_nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	uint64_t getSize( void ) const noexcept
	{
		return m_nCount;
	}
	uint32_t getChunkCount( void ) const noexcept
	{
		return m_chunks.getSize();
	}
};
#endif
