/* SIREngine - MPL-2.0. Frozen sorted multimap with grouped duplicate runs. */
#ifndef SIR_COMPACT_SPARSE_MULTIMAP_H
#define SIR_COMPACT_SPARSE_MULTIMAP_H
#include <Core/Containers/SIRVector.h>
#include <algorithm>
#include <functional>

template <typename K, typename V, typename Less = std::less<K>>
class SIRCompactSparseMultimap
{
public:
	struct Entry
	{
		K key;
		V value;
	};

private:
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<K> m_keys;
	SIRVector<uint32_t> m_offsets;
	SIRVector<V> m_values;
	Less m_less;

public:
	explicit SIRCompactSparseMultimap( const sirContainerAllocator_t *pAllocator, Less less = Less() )
	    : m_pAllocator( pAllocator ), m_keys( pAllocator ), m_offsets( pAllocator ), m_values( pAllocator ), m_less( std::move( less ) )
	{
	}
	sirContainerResult_t tryBuild( const Entry *pEntries, uint32_t nCount )
	{
		if ( nCount == UINT32_MAX || ( nCount && pEntries == nullptr ) ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<Entry> sorted( m_pAllocator );
		sirContainerResult_t eResult = sorted.tryReserve( nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			sorted.tryEmplaceBack( pEntries[iIndex] );
		}
		if ( sorted.getSize() > 1 ) {
			std::sort( sorted.begin(), sorted.end(), [&]( const Entry& a, const Entry& b ) { return m_less( a.key, b.key ); } );
		}
		SIRVector<K> keys( m_pAllocator );
		SIRVector<uint32_t> offsets( m_pAllocator );
		SIRVector<V> values( m_pAllocator );
		eResult = keys.tryReserve( nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = offsets.tryReserve( nCount + 1 );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = values.tryReserve( nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			if ( keys.isEmpty() || m_less( keys[keys.getSize() - 1], sorted[iIndex].key ) ) {
				keys.tryEmplaceBack( sorted[iIndex].key );
				offsets.tryEmplaceBack( iIndex );
			}
			values.tryEmplaceBack( sorted[iIndex].value );
		}
		offsets.tryEmplaceBack( nCount );
		m_keys = std::move( keys );
		m_offsets = std::move( offsets );
		m_values = std::move( values );
		return SIR_CONTAINER_OK;
	}
	bool find( const K& key, const V **ppValues, uint32_t *pCount ) const
	{
		if ( ppValues == nullptr || pCount == nullptr ) {
			return false;
		}
		uint32_t nLo = 0;
		uint32_t nHi = m_keys.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( m_less( m_keys[iMid], key ) ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		if ( nLo == m_keys.getSize() || m_less( key, m_keys[nLo] ) ) {
			return false;
		}
		*ppValues = m_values.data() + m_offsets[nLo];
		*pCount = m_offsets[nLo + 1] - m_offsets[nLo];
		return true;
	}
	uint32_t getKeyCount( void ) const noexcept
	{
		return m_keys.getSize();
	}
	uint32_t getValueCount( void ) const noexcept
	{
		return m_values.getSize();
	}
};
#endif
