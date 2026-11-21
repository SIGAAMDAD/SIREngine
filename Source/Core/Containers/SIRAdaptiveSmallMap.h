/* SIREngine - MPL-2.0. Inline linear map with transactional hash spill. */
#ifndef SIR_ADAPTIVE_SMALL_MAP_H
#define SIR_ADAPTIVE_SMALL_MAP_H
#include <Core/Containers/SIRStaticVector.h>
#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Containers/SIRRawStorage.h>
#include <functional>

template <typename K, typename V, uint32_t INLINE_COUNT = 8, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRAdaptiveSmallMap
{
	using Entry = std::pair<K, V>;
	const sirContainerAllocator_t *m_pAllocator;
	SIRStaticVector<Entry, INLINE_COUNT> m_inline;
	SIRFlatHashMap<K, V, Hash, Equal> *m_pHash = nullptr;
	Hash m_hash;
	Equal m_equal;

public:
	explicit SIRAdaptiveSmallMap( const sirContainerAllocator_t *pAllocator ) noexcept : m_pAllocator( pAllocator )
	{
	}
	~SIRAdaptiveSmallMap( void ) noexcept
	{
		if ( m_pHash != nullptr ) {
			m_pHash->~SIRFlatHashMap<K, V, Hash, Equal>();
			SIRRawStorage::deallocate( m_pAllocator, m_pHash, sizeof( *m_pHash ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ) );
		}
	}
	SIRAdaptiveSmallMap( const SIRAdaptiveSmallMap& ) = delete;
	SIRAdaptiveSmallMap& operator=( const SIRAdaptiveSmallMap& ) = delete;
	const V *find( const K& key ) const
	{
		if ( m_pHash != nullptr ) {
			return m_pHash->find( key );
		}
		for ( uint32_t iIndex = 0; iIndex < m_inline.getSize(); ++iIndex ) {
			if ( m_equal( m_inline[iIndex].first, key ) ) {
				return &m_inline[iIndex].second;
			}
		}
		return nullptr;
	}
	sirContainerResult_t tryInsert( const K& key, const V& value )
	{
		if ( find( key ) != nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_pHash != nullptr ) {
			return m_pHash->tryInsert( key, value );
		}
		if ( m_inline.getSize() < INLINE_COUNT ) {
			return m_inline.tryEmplaceBack( key, value );
		}
		void *pMemory = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, sizeof( SIRFlatHashMap<K, V, Hash, Equal> ),
		                                                           alignof( SIRFlatHashMap<K, V, Hash, Equal> ), &pMemory );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		auto *pNew = new ( pMemory ) SIRFlatHashMap<K, V, Hash, Equal>( m_pAllocator, m_hash, m_equal );
		try {
			eResult = pNew->tryReserve( INLINE_COUNT + 1 );
			for ( uint32_t iIndex = 0; eResult == SIR_CONTAINER_OK && iIndex < m_inline.getSize(); ++iIndex ) {
				eResult = pNew->tryInsert( m_inline[iIndex].first, m_inline[iIndex].second );
			}
			if ( eResult == SIR_CONTAINER_OK ) {
				eResult = pNew->tryInsert( key, value );
			}
		} catch ( ... ) {
			pNew->~SIRFlatHashMap<K, V, Hash, Equal>();
			SIRRawStorage::deallocate( m_pAllocator, pNew, sizeof( *pNew ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ) );
			throw;
		}
		if ( eResult != SIR_CONTAINER_OK ) {
			pNew->~SIRFlatHashMap<K, V, Hash, Equal>();
			SIRRawStorage::deallocate( m_pAllocator, pNew, sizeof( *pNew ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ) );
			return eResult;
		}
		m_inline.clear();
		m_pHash = pNew;
		return SIR_CONTAINER_OK;
	}
	bool erase( const K& key )
	{
		if ( m_pHash != nullptr ) {
			return m_pHash->erase( key );
		}
		static_assert( std::is_nothrow_move_assignable<Entry>::value, "Inline map needs nonthrowing erase shift" );
		for ( uint32_t iIndex = 0; iIndex < m_inline.getSize(); ++iIndex ) {
			if ( !m_equal( m_inline[iIndex].first, key ) ) {
				continue;
			}
			for ( uint32_t iMove = iIndex + 1; iMove < m_inline.getSize(); ++iMove ) {
				m_inline[iMove - 1] = std::move( m_inline[iMove] );
			}
			m_inline.tryPopBack();
			return true;
		}
		return false;
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_pHash == nullptr ? m_inline.getSize() : m_pHash->getSize();
	}
	bool isHashed( void ) const noexcept
	{
		return m_pHash != nullptr;
	}
};
#endif
