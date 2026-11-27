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

#ifndef __SIR_COLONY_H__
#define __SIR_COLONY_H__

#include <Core/SIRPCH.h>
#include <iterator>
#include <type_traits>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRRawStorage.h>
#include <new>
#include <stdint.h>
#include <utility>

template <bool ENABLED, uint32_t BLOCK_SIZE>
struct SIRColonyGenerationStorage
{
};

template <uint32_t BLOCK_SIZE>
struct SIRColonyGenerationStorage<true, BLOCK_SIZE>
{
	uint64_t nGenerations[BLOCK_SIZE] = {};
};

template <typename T, uint32_t BLOCK_SIZE = 64, bool TRACK_GENERATIONS = false>
class SIRColony
{
	static_assert( BLOCK_SIZE > 0 && BLOCK_SIZE <= 64, "Block size must fit a bitmap" );

	struct Block : SIRColonyGenerationStorage<TRACK_GENERATIONS, BLOCK_SIZE>
	{
		Block *pNext;
		Block *pNextAvailable;
		Block *pPreviousAvailable;
		uint64_t nOccupied;
		alignas( T ) unsigned char szSlots[BLOCK_SIZE * sizeof( T )];
	};

	const sirContainerAllocator_t *m_pAllocator;
	Block *m_pFirst = nullptr;
	Block *m_pAvailable = nullptr;
	uint32_t m_nSize = 0;
	uint32_t m_nBlocks = 0;
	uint64_t m_nNextGeneration = 0;

	static T *slot( Block *pBlock, uint32_t iIndex ) noexcept
	{
		return ( T * )( pBlock->szSlots + static_cast<size_t>( iIndex * sizeof( T ) ) );
	}

	void attachAvailable( Block *pBlock ) noexcept
	{
		pBlock->pPreviousAvailable = nullptr;
		pBlock->pNextAvailable = m_pAvailable;
		if ( m_pAvailable != nullptr ) {
			m_pAvailable->pPreviousAvailable = pBlock;
		}
		m_pAvailable = pBlock;
	}

	void detachAvailable( Block *pBlock ) noexcept
	{
		if ( pBlock->pPreviousAvailable != nullptr ) {
			pBlock->pPreviousAvailable->pNextAvailable = pBlock->pNextAvailable;
		} else {
			m_pAvailable = pBlock->pNextAvailable;
		}
		if ( pBlock->pNextAvailable != nullptr ) {
			pBlock->pNextAvailable->pPreviousAvailable = pBlock->pPreviousAvailable;
		}
		pBlock->pNextAvailable = nullptr;
		pBlock->pPreviousAvailable = nullptr;
	}

	static uint32_t firstFreeBit( uint64_t nOccupied ) noexcept
	{
		const uint64_t nVacant = ~nOccupied;
#if defined( __clang__ ) || defined( __GNUC__ )
		return static_cast<uint32_t>( __builtin_ctzll( nVacant ) );
#else
		uint32_t iBit = 0;
		while ( ( nVacant & ( uint64_t( 1 ) << iBit ) ) == 0 ) {
			++iBit;
		}
		return iBit;
#endif
	}

	static uint32_t firstOccupiedBit( uint64_t nOccupied ) noexcept
	{
#if defined( __clang__ ) || defined( __GNUC__ )
		return static_cast<uint32_t>( __builtin_ctzll( nOccupied ) );
#else
		uint32_t iBit = 0;
		while ( ( nOccupied & ( uint64_t( 1 ) << iBit ) ) == 0 ) {
			++iBit;
		}
		return iBit;
#endif
	}

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

	using value_type = T;

	using difference_type = ptrdiff_t;

	template <bool IS_CONST>
	class IteratorBase
	{
		template <bool> friend class IteratorBase;
	public:
		using value_type = T;
		using difference_type = ptrdiff_t;
		using iterator_category = std::forward_iterator_tag;
		using pointer = typename std::conditional<IS_CONST, const T *, T *>::type;
		using reference = typename std::conditional<IS_CONST, const T&, T&>::type;

		IteratorBase( void ) noexcept = default;
		IteratorBase( Block *pBlock, uint32_t iSlot ) noexcept : m_pBlock( pBlock ), m_iSlot( iSlot )
		{
			skipVacant();
		}
		template <bool OTHER_CONST, typename std::enable_if<IS_CONST && !OTHER_CONST, int>::type = 0>
		IteratorBase( const IteratorBase<OTHER_CONST>& rOther ) noexcept : m_pBlock( rOther.m_pBlock ), m_iSlot( rOther.m_iSlot ) {}
		reference operator*( void ) const noexcept
		{
			assert( m_pBlock != nullptr && m_iSlot < BLOCK_SIZE );
			return *slot( m_pBlock, m_iSlot );
		}
		pointer operator->( void ) const noexcept
		{
			return &operator*();
		}
		IteratorBase& operator++( void ) noexcept
		{
			++m_iSlot;
			skipVacant();
			return *this;
		}
		IteratorBase operator++( int ) noexcept
		{
			IteratorBase old( *this );
			++*this;
			return old;
		}
		bool operator==( const IteratorBase& rOther ) const noexcept
		{
			return m_pBlock == rOther.m_pBlock && m_iSlot == rOther.m_iSlot;
		}
		bool operator!=( const IteratorBase& rOther ) const noexcept
		{
			return !( *this == rOther );
		}

	private:
		Block *m_pBlock = nullptr;
		uint32_t m_iSlot = 0;
		void skipVacant( void ) noexcept
		{
			while ( m_pBlock != nullptr ) {
				while ( m_iSlot < BLOCK_SIZE ) {
					if ( ( m_pBlock->nOccupied & ( uint64_t( 1 ) << m_iSlot ) ) != 0 )
					{
						return;
					}
					++m_iSlot;
				}
				m_pBlock = m_pBlock->pNext;
				m_iSlot = 0;
			}
		}
	};

	using iterator = IteratorBase<false>;
	using const_iterator = IteratorBase<true>;
	iterator begin( void ) noexcept
	{
		return { m_pFirst, 0 };
	}
	iterator end( void ) noexcept
	{
		return {};
	}
	const_iterator begin( void ) const noexcept
	{
		return { m_pFirst, 0 };
	}
	const_iterator end( void ) const noexcept
	{
		return {};
	}
	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}
	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	struct Handle
	{
		T *pValue = nullptr;
		uint64_t nGeneration = 0;
	};

	explicit SIRColony( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator )
	{
	}

	~SIRColony() noexcept
	{
		clear();
	}

	SIRColony( const SIRColony& ) = delete;
	SIRColony& operator=( const SIRColony& ) = delete;

	template <typename... Args>
	SIRContainerResult tryEmplace( T **ppValue, Args&&...args )
	{
		if ( ppValue == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if constexpr ( TRACK_GENERATIONS ) {
			if ( m_nNextGeneration == UINT64_MAX ) {
				return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
			}
		}
		Block *pBlock = m_pAvailable;
		constexpr uint64_t MASK = BLOCK_SIZE == 64 ? UINT64_MAX : ( uint64_t( 1 ) << BLOCK_SIZE ) - 1;
		bool bNew = false;
		if ( pBlock == nullptr ) {
			void *pMemory = nullptr;
			SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, sizeof( Block ), alignof( Block ), &pMemory );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			pBlock = new ( pMemory ) Block{};
			bNew = true;
		}
		const uint32_t iFree = firstFreeBit( pBlock->nOccupied );
		T *pValue = nullptr;
		try {
			pValue = new ( slot( pBlock, iFree ) ) T( std::forward<Args>( args )... );
		} catch ( ... ) {
			if ( bNew ) {
				SIRRawStorage::deallocate( m_pAllocator, pBlock, sizeof( Block ), alignof( Block ) );
			}
			throw;
		}
		if ( bNew ) {
			pBlock->pNext = m_pFirst;
			m_pFirst = pBlock;
			++m_nBlocks;
		}
		pBlock->nOccupied |= uint64_t( 1 ) << iFree;
		if constexpr ( TRACK_GENERATIONS ) {
			pBlock->nGenerations[iFree] = ++m_nNextGeneration;
		}
		if ( bNew ) {
			if ( pBlock->nOccupied != MASK ) {
				attachAvailable( pBlock );
			}
		} else if ( pBlock->nOccupied == MASK ) {
			detachAvailable( pBlock );
		}
		++m_nSize;
		*ppValue = pValue;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	// Generation-aware handles are enabled only for tracked colony variants.
	// Untracked colonies retain the original compact block representation.
	Handle tryGetHandle( T *pValue ) const noexcept
	{
		static_assert( TRACK_GENERATIONS, "Generation handles require SIRGenerationColony" );
		if ( pValue == nullptr ) {
			return {};
		}
		const uintptr_t nAddress = reinterpret_cast<uintptr_t>( pValue );
		for ( Block *pBlock = m_pFirst; pBlock != nullptr; pBlock = pBlock->pNext ) {
			const uintptr_t nStart = reinterpret_cast<uintptr_t>( pBlock->szSlots );
			if ( nAddress < nStart || nAddress - nStart >= sizeof( pBlock->szSlots ) || ( nAddress - nStart ) % sizeof( T ) != 0 ) {
				continue;
			}
			const uint32_t iSlot = static_cast<uint32_t>( ( nAddress - nStart ) / sizeof( T ) );
			if ( ( pBlock->nOccupied & ( uint64_t( 1 ) << iSlot ) ) != 0 ) {
				return { pValue, pBlock->nGenerations[iSlot] };
			}
		}
		return {};
	}

	T *tryResolveHandle( Handle rHandle ) const noexcept
	{
		static_assert( TRACK_GENERATIONS, "Generation handles require SIRGenerationColony" );
		if ( rHandle.nGeneration == 0 || rHandle.pValue == nullptr ) {
			return nullptr;
		}
		const Handle current = tryGetHandle( rHandle.pValue );
		return current.nGeneration == rHandle.nGeneration ? current.pValue : nullptr;
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceHandle( Handle *pHandle, Args&&...args )
	{
		static_assert( TRACK_GENERATIONS, "Generation handles require SIRGenerationColony" );
		if ( pHandle == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		T *pValue = nullptr;
		const SIRContainerResult eResult = tryEmplace( &pValue, std::forward<Args>( args )... );
		if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
			*pHandle = tryGetHandle( pValue );
		}
		return eResult;
	}

	bool eraseHandle( Handle rHandle ) noexcept
	{
		static_assert( TRACK_GENERATIONS, "Generation handles require SIRGenerationColony" );
		T *pValue = tryResolveHandle( rHandle );
		return pValue != nullptr && erase( pValue );
	}

	bool erase( T *pValue ) noexcept
	{
		if ( pValue == nullptr ) {
			return false;
		}
		const uintptr_t nAddress = reinterpret_cast<uintptr_t>( pValue );
		for ( Block *pBlock = m_pFirst; pBlock != nullptr; pBlock = pBlock->pNext ) {
			const uintptr_t nStart = reinterpret_cast<uintptr_t>( pBlock->szSlots );
			const uintptr_t nEnd = nStart + sizeof( pBlock->szSlots );
			if ( nAddress < nStart || nAddress >= nEnd ) {
				continue;
			}
			if ( ( nAddress - nStart ) % sizeof( T ) != 0 ) {
				return false;
			}
			const uint32_t iIndex = static_cast<uint32_t>( ( nAddress - nStart ) / sizeof( T ) );
			const uint64_t nMask = uint64_t( 1 ) << iIndex;
			if ( ( pBlock->nOccupied & nMask ) == 0 ) {
				return false;
			}
			pValue->~T();
			const bool bWasFull = pBlock->nOccupied == ( BLOCK_SIZE == 64 ? UINT64_MAX : ( uint64_t( 1 ) << BLOCK_SIZE ) - 1 );
			pBlock->nOccupied &= ~nMask;
			if ( bWasFull ) {
				attachAvailable( pBlock );
			}
			--m_nSize;
			return true;
		}
		return false;
	}

	template <typename Visitor>
	void forEach( Visitor&& visitor )
	{
		for ( T& rValue : *this )
		{
			visitor( rValue );
		}
	}

	template <typename Visitor>
	void forEach( Visitor&& visitor ) const
	{
		for ( const T& rValue : *this )
		{
			visitor( rValue );
		}
	}

	void clear( void ) noexcept
	{
		Block *pBlock = m_pFirst;
		while ( pBlock != nullptr ) {
			Block *pNext = pBlock->pNext;
			uint64_t nLive = pBlock->nOccupied;
			while ( nLive != 0 ) {
				slot( pBlock, firstOccupiedBit( nLive ) )->~T();
				nLive &= nLive - 1;
			}
			SIRRawStorage::deallocate( m_pAllocator, pBlock, sizeof( Block ), alignof( Block ) );
			pBlock = pNext;
		}
		m_pFirst = nullptr;
		m_pAvailable = nullptr;
		m_nBlocks = 0;
		m_nSize = 0;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return {
		    sizeof( *this ), static_cast<uint64_t>( m_nBlocks * sizeof( Block ) ), 0, 0, static_cast<uint64_t>( m_nSize * sizeof( T ) ) };
	}
};

template <typename T, uint32_t BLOCK_SIZE = 64>
using SIRGenerationColony = SIRColony<T, BLOCK_SIZE, true>;

#endif /* __SIR_COLONY_H__ */
