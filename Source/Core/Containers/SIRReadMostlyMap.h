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

#ifndef __SIR_READMOSTLYMAP_H__
#define __SIR_READMOSTLYMAP_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
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

	class ReadGuard
	{
		const SIRReadMostlyMap *m_pOwner = nullptr;
		const SIRFrozenHashIndex<K, V, Hash, Equal> *m_pSnapshot = nullptr;

	public:
		using size_type = uint32_t;

		uint32_t size( void ) const noexcept
		{
			return getSize();
		}

		bool empty( void ) const noexcept
		{
			return getSize() == 0;
		}

		bool isEmpty( void ) const noexcept
		{
			return empty();
		}

		using key_type = K;
		using mapped_type = V;
		using value_type = Entry;
		using const_iterator = typename SIRFrozenHashIndex<K, V, Hash, Equal>::const_iterator;
		using iterator = const_iterator;

		iterator begin( void ) const noexcept
		{
			return m_pSnapshot == nullptr ? iterator() : m_pSnapshot->begin();
		}

		iterator end( void ) const noexcept
		{
			return m_pSnapshot == nullptr ? iterator() : m_pSnapshot->end();
		}

		const_iterator cbegin( void ) const noexcept
		{
			return begin();
		}

		const_iterator cend( void ) const noexcept
		{
			return end();
		}


		bool contains( const K& rKey ) const
		{
			return find( rKey ) != nullptr;
		}

		explicit ReadGuard( const SIRReadMostlyMap *pOwner ) noexcept
		    : m_pOwner( pOwner )
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

		~ReadGuard() noexcept
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

	explicit SIRReadMostlyMap( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator )
	{
	}

	~SIRReadMostlyMap() noexcept
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
		SIRContainerResult eResult = SIRRawStorage::tryAllocate(
		    m_pAllocator, sizeof( SIRFrozenHashIndex<K, V, Hash, Equal> ), alignof( SIRFrozenHashIndex<K, V, Hash, Equal> ), &pMemory );
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
};
#endif /* __SIR_READMOSTLYMAP_H__ */
