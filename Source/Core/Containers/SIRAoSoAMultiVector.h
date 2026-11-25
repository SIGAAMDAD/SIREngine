/* SIREngine MPLv2 - lazy-lifetime, heterogeneous array-of-structures-of-arrays. */
#ifndef __SIR_AOSOAMULTIVECTOR_H__
#define __SIR_AOSOAMULTIVECTOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <new>
#include <tuple>
#include <type_traits>
#include <stdint.h>

// Stable block addresses mean vector-directory reallocation never relocates
// live column objects. Only used lanes are constructed and destroyed.
template <uint32_t LANES, typename... Fields>
class SIRAoSoAMultiVector
{
	static_assert( LANES > 0 && LANES <= 256, "AoSoA lane width out of range" );
	static_assert( sizeof...( Fields ) > 0, "AoSoA requires at least one column" );

	template <typename T>
	struct Column
	{
		alignas( T ) unsigned char szData[LANES * sizeof( T )];

		T *slot( uint32_t iLane ) noexcept
		{
			return ( T * )( szData + static_cast<size_t>( iLane * sizeof( T ) ) );
		}

		T *at( uint32_t iLane ) noexcept
		{
			return std::launder( slot( iLane ) );
		}

		const T *at( uint32_t iLane ) const noexcept
		{
			return std::launder( ( const T * )( szData + static_cast<size_t>( iLane * sizeof( T ) ) ) );
		}
	};

	struct alignas( 64 ) Block
	{
		std::tuple<Column<Fields>...> Columns;
		uint32_t nUsed = 0;

		template <size_t I = 0>
		void destroyFields( uint32_t iLane, uint32_t nConstructed ) noexcept
		{
			if constexpr ( I < sizeof...( Fields ) ) {
				if ( I < nConstructed ) {
					using Value = typename std::tuple_element<I, std::tuple<Fields...>>::type;
					std::get<I>( Columns ).at( iLane )->~Value();
				}
				destroyFields<I + 1>( iLane, nConstructed );
			}
		}

		template <size_t I = 0>
		void constructFields( uint32_t iLane, const std::tuple<const Fields&...>& rValues, uint32_t *pConstructed )
		{
			if constexpr ( I < sizeof...( Fields ) ) {
				using Value = typename std::tuple_element<I, std::tuple<Fields...>>::type;
				new ( std::get<I>( Columns ).slot( iLane ) ) Value( std::get<I>( rValues ) );
				++*pConstructed;
				constructFields<I + 1>( iLane, rValues, pConstructed );
			}
		}

		void emplace( const Fields&...values )
		{
			const std::tuple<const Fields&...> args( values... );
			uint32_t nConstructed = 0;
			try {
				constructFields( nUsed, args, &nConstructed );
			} catch ( ... ) {
				destroyFields( nUsed, nConstructed );
				throw;
			}
			++nUsed;
		}

		~Block() noexcept
		{
			while ( nUsed != 0 ) {
				--nUsed;
				destroyFields( nUsed, sizeof...( Fields ) );
			}
		}
	};

	SIRVector<Block *> m_Blocks;
	const sirContainerAllocator_t *m_pAllocator;
	uint32_t m_nSize = 0;

public:
	explicit SIRAoSoAMultiVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_Blocks( pAllocator ),
	      m_pAllocator( pAllocator )
	{
	}

	~SIRAoSoAMultiVector() noexcept
	{
		for ( uint32_t i = 0; i < m_Blocks.getSize(); ++i ) {
			Block *pBlock = m_Blocks[i];
			pBlock->~Block();
			SIRRawStorage::deallocate( m_pAllocator, pBlock, sizeof( Block ), alignof( Block ) );
		}
	}

	SIRAoSoAMultiVector( const SIRAoSoAMultiVector& ) = delete;
	SIRAoSoAMultiVector& operator=( const SIRAoSoAMultiVector& ) = delete;

	SIRContainerResult tryEmplaceBack( const Fields&...values )
	{
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( m_nSize % LANES != 0 ) {
			m_Blocks[m_nSize / LANES]->emplace( values... );
			++m_nSize;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		void *pMemory = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, sizeof( Block ), alignof( Block ), &pMemory );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		Block *pBlock = new ( pMemory ) Block;
		try {
			pBlock->emplace( values... );
			eResult = m_Blocks.tryPushBack( pBlock );
		} catch ( ... ) {
			pBlock->~Block();
			SIRRawStorage::deallocate( m_pAllocator, pBlock, sizeof( Block ), alignof( Block ) );
			throw;
		}
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			pBlock->~Block();
			SIRRawStorage::deallocate( m_pAllocator, pBlock, sizeof( Block ), alignof( Block ) );
			return eResult;
		}
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	template <size_t I>
	using FieldType = typename std::tuple_element<I, std::tuple<Fields...>>::type;

	template <size_t I>
	FieldType<I> *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? std::get<I>( m_Blocks[iIndex / LANES]->Columns ).at( iIndex % LANES ) : nullptr;
	}

	template <size_t I>
	const FieldType<I> *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? std::get<I>( m_Blocks[iIndex / LANES]->Columns ).at( iIndex % LANES ) : nullptr;
	}

	template <size_t I>
	SIRReadOnlyVectorView<FieldType<I>> getBlockColumnView( uint32_t iBlock ) const noexcept
	{
		if ( iBlock >= m_Blocks.getSize() ) {
			return {};
		}
		Block *pBlock = m_Blocks[iBlock];
		return { std::get<I>( pBlock->Columns ).at( 0 ), pBlock->nUsed };
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint32_t getBlockCount( void ) const noexcept
	{
		return m_Blocks.getSize();
	}
};

#endif // __SIR_AOSOAMULTIVECTOR_H__
