/* SIREngine - MPL-2.0. Static segmented XOR8 filter with three adjacent fuse segments. */
#ifndef SIR_BINARY_FUSE_FILTER_H
#define SIR_BINARY_FUSE_FILTER_H
#include <Core/Containers/SIRVector.h>
#include <algorithm>

class SIRBinaryFuseFilter
{
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<uint8_t> m_fingerprints;
	uint64_t m_nSeed = 0;
	uint32_t m_nPartSize = 0;
	uint32_t m_nSegmentCount = 0;
	uint32_t m_nKeys = 0;
	static uint64_t mix( uint64_t nValue ) noexcept
	{
		nValue ^= nValue >> 33;
		nValue *= 0xff51afd7ed558ccdull;
		nValue ^= nValue >> 33;
		nValue *= 0xc4ceb9fe1a85ec53ull;
		return nValue ^ ( nValue >> 33 );
	}
	static uint8_t fingerprint( uint64_t nValue ) noexcept
	{
		return ( uint8_t )mix( nValue ^ 0xaf9d527b6e37c1d5ull );
	}
	static uint32_t index( uint64_t nValue, uint64_t nSeed, uint32_t iPart, uint32_t nSegmentSize, uint32_t nSegments ) noexcept
	{
		const uint64_t nHash = mix( nValue ^ nSeed );
		const uint32_t iStart = ( uint32_t )( nHash % ( nSegments - 2 ) );
		const uint32_t iOffset = ( uint32_t )mix( nHash ^ ( 0x9e3779b97f4a7c15ull * ( iPart + 1 ) ) ) & ( nSegmentSize - 1 );
		return ( iStart + iPart ) * nSegmentSize + iOffset;
	}

public:
	explicit SIRBinaryFuseFilter( const sirContainerAllocator_t *pAllocator ) : m_pAllocator( pAllocator ), m_fingerprints( pAllocator )
	{
	}
	sirContainerResult_t tryBuild( const uint64_t *pValues, uint32_t nCount )
	{
		if ( nCount != 0 && pValues == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount > UINT32_MAX / 6 - 16 ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		SIRVector<uint64_t> keys( m_pAllocator );
		sirContainerResult_t eResult = keys.tryReserve( nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			keys.tryEmplaceBack( pValues[iIndex] );
		}
		if ( nCount > 1 ) {
			std::sort( keys.begin(), keys.end() );
		}
		for ( uint32_t iIndex = 1; iIndex < nCount; ++iIndex ) {
			if ( keys[iIndex - 1] == keys[iIndex] ) {
				return SIR_CONTAINER_INVALID_ARGUMENT;
			}
		}
		uint32_t nPart = 8;
		while ( nPart < 256 && nCount / nPart > 10 ) {
			nPart *= 2;
		}
		const uint64_t nSegmentsRequired = ( ( uint64_t )nCount * 13 / 10 + nPart - 1 ) / nPart + 2;
		if ( nSegmentsRequired > UINT32_MAX / nPart ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const uint32_t nSegments = ( uint32_t )( nSegmentsRequired < 3 ? 3 : nSegmentsRequired );
		const uint32_t nSlots = nPart * nSegments;
		SIRVector<uint32_t> degree( m_pAllocator );
		SIRVector<uint64_t> xors( m_pAllocator );
		SIRVector<uint32_t> stack( m_pAllocator );
		SIRVector<uint64_t> peeledKeys( m_pAllocator );
		SIRVector<uint32_t> peeledPositions( m_pAllocator );
		SIRVector<uint8_t> tags( m_pAllocator );
		eResult = degree.tryResize( nSlots );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = xors.tryResize( nSlots );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = stack.tryReserve( nSlots );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = peeledKeys.tryReserve( nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = peeledPositions.tryReserve( nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = tags.tryResize( nSlots );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t nAttempt = 0; nAttempt < 64; ++nAttempt ) {
			const uint64_t nSeed = mix( nAttempt + 1 );
			for ( uint32_t iIndex = 0; iIndex < nSlots; ++iIndex ) {
				degree[iIndex] = 0;
				xors[iIndex] = 0;
				tags[iIndex] = 0;
			}
			stack.clear();
			peeledKeys.clear();
			peeledPositions.clear();
			for ( uint32_t iKey = 0; iKey < nCount; ++iKey ) {
				for ( uint32_t iPart = 0; iPart < 3; ++iPart ) {
					const uint32_t iSlot = index( keys[iKey], nSeed, iPart, nPart, nSegments );
					++degree[iSlot];
					xors[iSlot] ^= keys[iKey];
				}
			}
			for ( uint32_t iIndex = 0; iIndex < nSlots; ++iIndex ) {
				if ( degree[iIndex] == 1 ) {
					stack.tryEmplaceBack( iIndex );
				}
			}
			while ( !stack.isEmpty() ) {
				const uint32_t iSlot = stack[stack.getSize() - 1];
				stack.tryPopBack();
				if ( degree[iSlot] != 1 ) {
					continue;
				}
				const uint64_t nKey = xors[iSlot];
				peeledKeys.tryEmplaceBack( nKey );
				peeledPositions.tryEmplaceBack( iSlot );
				for ( uint32_t iPart = 0; iPart < 3; ++iPart ) {
					const uint32_t iTarget = index( nKey, nSeed, iPart, nPart, nSegments );
					if ( degree[iTarget] == 0 ) {
						continue;
					}
					--degree[iTarget];
					xors[iTarget] ^= nKey;
					if ( degree[iTarget] == 1 ) {
						stack.tryEmplaceBack( iTarget );
					}
				}
			}
			if ( peeledKeys.getSize() != nCount ) {
				continue;
			}
			for ( uint32_t iEntry = nCount; iEntry > 0; --iEntry ) {
				const uint64_t nKey = peeledKeys[iEntry - 1];
				const uint32_t iAssigned = peeledPositions[iEntry - 1];
				uint8_t nTag = fingerprint( nKey );
				for ( uint32_t iPart = 0; iPart < 3; ++iPart ) {
					const uint32_t iSlot = index( nKey, nSeed, iPart, nPart, nSegments );
					if ( iSlot != iAssigned ) {
						nTag ^= tags[iSlot];
					}
				}
				tags[iAssigned] = nTag;
			}
			m_fingerprints = std::move( tags );
			m_nSeed = nSeed;
			m_nPartSize = nPart;
			m_nSegmentCount = nSegments;
			m_nKeys = nCount;
			return SIR_CONTAINER_OK;
		}
		return SIR_CONTAINER_BUSY;
	}
	bool mayContain( uint64_t nValue ) const noexcept
	{
		if ( m_nKeys == 0 ) {
			return false;
		}
		uint8_t nTag = 0;
		for ( uint32_t iPart = 0; iPart < 3; ++iPart ) {
			nTag ^= m_fingerprints[index( nValue, m_nSeed, iPart, m_nPartSize, m_nSegmentCount )];
		}
		return nTag == fingerprint( nValue );
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nKeys;
	}
};
#endif
