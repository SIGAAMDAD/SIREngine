/* SIREngine - MPL-2.0. Reader-pinned immutable snapshot map. */
#ifndef SIR_READ_MOSTLY_MAP_H
#define SIR_READ_MOSTLY_MAP_H
#include <Core/Containers/SIRFrozenHashIndex.h>
#include <atomic>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#include <new>

template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRReadMostlyMap
{
public:
	using Entry = typename SIRFrozenHashIndex<K, V, Hash, Equal>::Entry;

private:
	const sirContainerAllocator_t *m_pAllocator;
	mutable std::atomic<uint32_t> m_nReaders{ 0 };
	std::atomic<bool> m_bPublishing{ false };
	std::atomic<SIRFrozenHashIndex<K, V, Hash, Equal> *> m_pSnapshot{ nullptr };
	SIRMutex m_writeMutex;
	static void destroy( const sirContainerAllocator_t *pAllocator, SIRFrozenHashIndex<K, V, Hash, Equal> *pSnapshot ) noexcept
	{
		if ( pSnapshot == nullptr ) {
			return;
		}
		pSnapshot->~SIRFrozenHashIndex<K, V, Hash, Equal>();
		SIRRawStorage::deallocate( pAllocator, pSnapshot, sizeof( *pSnapshot ), alignof( SIRFrozenHashIndex<K, V, Hash, Equal> ) );
	}

public:
	class ReadGuard
	{
		const SIRReadMostlyMap *m_pOwner = nullptr;
		const SIRFrozenHashIndex<K, V, Hash, Equal> *m_pSnapshot = nullptr;

	public:
		explicit ReadGuard( const SIRReadMostlyMap *pOwner ) noexcept : m_pOwner( pOwner )
		{
			for ( ;; ) {
				while ( m_pOwner->m_bPublishing.load( std::memory_order_acquire ) ) {
				}
				m_pOwner->m_nReaders.fetch_add( 1, std::memory_order_acq_rel );
				if ( !m_pOwner->m_bPublishing.load( std::memory_order_acquire ) ) {
					m_pSnapshot = m_pOwner->m_pSnapshot.load( std::memory_order_acquire );
					break;
				}
				m_pOwner->m_nReaders.fetch_sub( 1, std::memory_order_release );
			}
		}
		~ReadGuard( void ) noexcept
		{
			m_pOwner->m_nReaders.fetch_sub( 1, std::memory_order_release );
		}
		ReadGuard( const ReadGuard& ) = delete;
		ReadGuard& operator=( const ReadGuard& ) = delete;
		const V *find( const K& key ) const
		{
			return m_pSnapshot == nullptr ? nullptr : m_pSnapshot->find( key );
		}
		uint32_t getSize( void ) const noexcept
		{
			return m_pSnapshot == nullptr ? 0 : m_pSnapshot->getSize();
		}
	};
	explicit SIRReadMostlyMap( const sirContainerAllocator_t *pAllocator ) noexcept : m_pAllocator( pAllocator )
	{
	}
	~SIRReadMostlyMap( void ) noexcept
	{
		destroy( m_pAllocator, m_pSnapshot.load() );
	}
	SIRReadMostlyMap( const SIRReadMostlyMap& ) = delete;
	SIRReadMostlyMap& operator=( const SIRReadMostlyMap& ) = delete;
	ReadGuard read( void ) const noexcept
	{
		return ReadGuard( this );
	}
	SIRContainerResult tryPublish( const Entry *pEntries, uint32_t nCount )
	{
		SIRScopedLock<SIRMutex> guard( m_writeMutex );
		void *pMemory = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, sizeof( SIRFrozenHashIndex<K, V, Hash, Equal> ),
		                                                           alignof( SIRFrozenHashIndex<K, V, Hash, Equal> ), &pMemory );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		auto *pNew = new ( pMemory ) SIRFrozenHashIndex<K, V, Hash, Equal>( m_pAllocator );
		try {
			eResult = pNew->tryBuild( pEntries, nCount );
		} catch ( ... ) {
			destroy( m_pAllocator, pNew );
			throw;
		}
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			destroy( m_pAllocator, pNew );
			return eResult;
		}
		m_bPublishing.store( true, std::memory_order_release );
		while ( m_nReaders.load( std::memory_order_acquire ) != 0 ) {
		}
		auto *pOld = m_pSnapshot.exchange( pNew, std::memory_order_acq_rel );
		m_bPublishing.store( false, std::memory_order_release );
		destroy( m_pAllocator, pOld );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
};
#endif
