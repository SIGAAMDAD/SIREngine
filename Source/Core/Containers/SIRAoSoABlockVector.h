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

#ifndef __SIR_AOSOABLOCKVECTOR_H__
#define __SIR_AOSOABLOCKVECTOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <type_traits>

template <typename A, typename B, uint32_t LANES = 16>
class SIRAoSoABlockVector
{
	static_assert( LANES > 0 && LANES <= 256, "AoSoA lane count out of range" );
	static_assert( std::is_default_constructible<A>::value && std::is_default_constructible<B>::value,
	               "Blocked AoSoA columns require default-constructible entries" );
	static_assert( std::is_nothrow_copy_assignable<A>::value && std::is_nothrow_copy_assignable<B>::value,
	               "AoSoA append requires nonthrowing column assignment for atomic element publication" );

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

	explicit SIRAoSoABlockVector( const sirContainerAllocator_t *pAllocator )
	    : m_blocks( pAllocator )
	{
	}

	SIRContainerResult tryEmplaceBack( const A& first, const B& second )
	{
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		// Stage before growing m_blocks: inputs may alias an existing column.
		A stagedFirst( first );
		B stagedSecond( second );
		if ( m_nSize % LANES == 0 ) {
			const SIRContainerResult eResult = m_blocks.tryEmplaceBack();
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		Block& block = m_blocks[m_nSize / LANES];
		block.szFirst[m_nSize % LANES] = stagedFirst;
		block.szSecond[m_nSize % LANES] = stagedSecond;
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	A *tryGetFirst( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? &m_blocks[iIndex / LANES].szFirst[iIndex % LANES] : nullptr;
	}

	B *tryGetSecond( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? &m_blocks[iIndex / LANES].szSecond[iIndex % LANES] : nullptr;
	}

	const A *tryGetFirst( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? &m_blocks[iIndex / LANES].szFirst[iIndex % LANES] : nullptr;
	}

	const B *tryGetSecond( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? &m_blocks[iIndex / LANES].szSecond[iIndex % LANES] : nullptr;
	}

	uint32_t capacity( void ) const noexcept
	{
		const uint64_t nCapacity = static_cast<uint64_t>( m_blocks.capacity() ) * LANES;
		return nCapacity > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>( nCapacity );
	}

	SIRContainerResult tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 )
		{
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		--m_nSize;
		if ( m_nSize % LANES == 0 )
		{
			return m_blocks.tryPopBack();
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		m_blocks.clear();
		m_nSize = 0;
	}

	template <typename Visitor>
	void forEach( Visitor&& visitor )
	{
		for ( uint32_t iIndex = 0; iIndex < m_nSize; ++iIndex ) {
			visitor( *tryGetFirst( iIndex ), *tryGetSecond( iIndex ) );
		}
	}

	template <typename Visitor>
	void forEach( Visitor&& visitor ) const
	{
		for ( uint32_t iIndex = 0; iIndex < m_nSize; ++iIndex ) {
			visitor( *tryGetFirst( iIndex ), *tryGetSecond( iIndex ) );
		}
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint32_t getBlockCount( void ) const noexcept
	{
		return m_blocks.getSize();
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return m_blocks.getFootprint();
	}

private:
	struct alignas( 64 ) Block
	{
		A szFirst[LANES]{};
		B szSecond[LANES]{};
	};

	SIRVector<Block> m_blocks;
	uint32_t m_nSize = 0;
};

#endif /* __SIR_AOSOABLOCKVECTOR_H__ */
