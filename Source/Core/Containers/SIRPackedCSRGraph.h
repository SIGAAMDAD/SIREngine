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

#ifndef __SIR_PACKEDCSRGRAPH_H__
#define __SIR_PACKEDCSRGRAPH_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <optional>

template <typename T>
class SIRPackedCSRGraph
{
public:
	struct Edge
	{
		uint32_t nDestination;
		T value;
	};

	explicit SIRPackedCSRGraph( const sirContainerAllocator_t *pAllocator )
	    : m_pAllocator( pAllocator ),
	      m_storage( pAllocator ),
	      m_offsets( pAllocator ),
	      m_sizes( pAllocator ),
	      m_capacities( pAllocator )
	{
	}

	SIRContainerResult tryInitialize( uint32_t nVertices, uint32_t nCapacityPerRow = 4 )
	{
		if ( nVertices == 0 || nCapacityPerRow == 0 || nVertices > UINT32_MAX / nCapacityPerRow ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<std::optional<Edge>> storage( m_pAllocator );
		SIRVector<uint32_t> offsets( m_pAllocator );
		SIRVector<uint32_t> sizes( m_pAllocator );
		SIRVector<uint32_t> capacities( m_pAllocator );
		SIRContainerResult eResult = storage.tryResize( nVertices * nCapacityPerRow );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = offsets.tryResize( nVertices );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = sizes.tryResize( nVertices );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = capacities.tryResize( nVertices );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iVertex = 0; iVertex < nVertices; ++iVertex ) {
			offsets[iVertex] = iVertex * nCapacityPerRow;
			capacities[iVertex] = nCapacityPerRow;
		}
		m_storage = std::move( storage );
		m_offsets = std::move( offsets );
		m_sizes = std::move( sizes );
		m_capacities = std::move( capacities );
		m_nEdges = 0;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryAddEdge( uint32_t iSource, uint32_t iDestination, const T& value )
	{
		if ( m_nEdges == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( iSource >= m_sizes.getSize() || iDestination >= m_sizes.getSize() ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		// Copy potentially aliased input before a row resize can relocate it.
		Edge staged{ iDestination, value };
		if ( m_sizes[iSource] == m_capacities[iSource] ) {
			SIRContainerResult eResult = growRow( iSource );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		m_storage[m_offsets[iSource] + m_sizes[iSource]].emplace( std::move( staged ) );
		++m_sizes[iSource];
		++m_nEdges;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool eraseEdge( uint32_t iSource, uint32_t iDestination )
	{
		if ( iSource >= m_sizes.getSize() ) {
			return false;
		}
		static_assert( std::is_nothrow_move_assignable<Edge>::value, "Erase requires noexcept edge assignment" );
		for ( uint32_t iIndex = 0; iIndex < m_sizes[iSource]; ++iIndex ) {
			const uint32_t iTarget = m_offsets[iSource] + iIndex;
			if ( m_storage[iTarget]->nDestination != iDestination ) {
				continue;
			}
			const uint32_t iLast = m_offsets[iSource] + m_sizes[iSource] - 1;
			if ( iTarget != iLast ) {
				*m_storage[iTarget] = std::move( *m_storage[iLast] );
			}
			m_storage[iLast].reset();
			--m_sizes[iSource];
			--m_nEdges;
			return true;
		}
		return false;
	}

	template <typename Visitor>
	void forEachEdge( uint32_t iSource, Visitor&& visitor ) const
	{
		if ( iSource >= m_sizes.getSize() ) {
			return;
		}
		for ( uint32_t iIndex = 0; iIndex < m_sizes[iSource]; ++iIndex ) {
			const Edge& edge = *m_storage[m_offsets[iSource] + iIndex];
			visitor( edge.nDestination, edge.value );
		}
	}

	uint32_t getEdgeCount( void ) const noexcept
	{
		return m_nEdges;
	}

	uint32_t getVertexCount( void ) const noexcept
	{
		return m_sizes.getSize();
	}

	publicprivate :

	    const sirContainerAllocator_t *m_pAllocator;
	SIRVector<std::optional<Edge>> m_storage;
	SIRVector<uint32_t> m_offsets;
	SIRVector<uint32_t> m_sizes;
	SIRVector<uint32_t> m_capacities;
	uint32_t m_nEdges = 0;

	SIRContainerResult growRow( uint32_t iGrow )
	{
		const uint32_t nOld = m_capacities[iGrow];
		if ( nOld > UINT32_MAX / 2 ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const uint32_t nNew = nOld == 0 ? 4 : nOld * 2;
		if ( m_storage.getSize() > UINT32_MAX - ( nNew - nOld ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		SIRVector<std::optional<Edge>> staged( m_pAllocator );
		SIRContainerResult eResult = staged.tryResize( m_storage.getSize() + nNew - nOld );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint32_t iWrite = 0;
		for ( uint32_t iVertex = 0; iVertex < m_sizes.getSize(); ++iVertex ) {
			for ( uint32_t iEdge = 0; iEdge < m_sizes[iVertex]; ++iEdge ) {
				staged[iWrite + iEdge].emplace( *m_storage[m_offsets[iVertex] + iEdge] );
			}
			iWrite += iVertex == iGrow ? nNew : m_capacities[iVertex];
		}
		m_storage = std::move( staged );
		m_capacities[iGrow] = nNew;
		uint32_t iOffset = 0;
		for ( uint32_t iVertex = 0; iVertex < m_sizes.getSize(); ++iVertex ) {
			m_offsets[iVertex] = iOffset;
			iOffset += m_capacities[iVertex];
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
};
#endif /* __SIR_PACKEDCSRGRAPH_H__ */
