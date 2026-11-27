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

#ifndef __SIR_CSRBUILDER_H__
#define __SIR_CSRBUILDER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <algorithm>
#include <utility>

template <typename T>
class SIRCSRGraph
{
	template <typename U>
	friend class SIRCSRBuilder;
	SIRVector<uint32_t> m_offsets;
	SIRVector<uint32_t> m_destinations;
	SIRVector<T> m_values;
	uint32_t m_nVertices = 0;

public:
	using size_type = uint32_t;

	uint32_t size( void ) const noexcept
	{
		return getEdgeCount();
	}

	bool empty( void ) const noexcept
	{
		return getEdgeCount() == 0;
	}

	bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	explicit SIRCSRGraph( const sirContainerAllocator_t *pAllocator )
	    : m_offsets( pAllocator ),
	      m_destinations( pAllocator ),
	      m_values( pAllocator )
	{
	}

	using value_type = T;

	uint32_t degree( uint32_t iVertex ) const noexcept
	{
		return iVertex < m_nVertices ? m_offsets[iVertex + 1] - m_offsets[iVertex] : 0;
	}

	SIRReadOnlyVectorView<uint32_t> destinations( uint32_t iVertex ) const noexcept
	{
		const uint32_t nDegree = degree( iVertex );
		return nDegree == 0 ? SIRReadOnlyVectorView<uint32_t>()
		                    : SIRReadOnlyVectorView<uint32_t>( m_destinations.data() + m_offsets[iVertex], nDegree );
	}

	SIRReadOnlyVectorView<T> values( uint32_t iVertex ) const noexcept
	{
		const uint32_t nDegree = degree( iVertex );
		return nDegree == 0 ? SIRReadOnlyVectorView<T>()
		                    : SIRReadOnlyVectorView<T>( m_values.data() + m_offsets[iVertex], nDegree );
	}

	uint32_t getVertexCount( void ) const noexcept
	{
		return m_nVertices;
	}

	uint32_t getEdgeCount( void ) const noexcept
	{
		return m_values.getSize();
	}

	uint32_t getEdgeBegin( uint32_t iVertex ) const noexcept
	{
		return iVertex < m_nVertices ? m_offsets[iVertex] : 0;
	}

	uint32_t getEdgeEnd( uint32_t iVertex ) const noexcept
	{
		return iVertex < m_nVertices ? m_offsets[iVertex + 1] : 0;
	}

	uint32_t getDestination( uint32_t iEdge ) const noexcept
	{
		return m_destinations[iEdge];
	}

	const T *tryGetValue( uint32_t iEdge ) const noexcept
	{
		return m_values.tryGet( iEdge );
	}
};

template <typename T>
class SIRCSRBuilder
{
public:
	using size_type = uint32_t;

	uint32_t size( void ) const noexcept
	{
		return getPendingEdges();
	}

	bool empty( void ) const noexcept
	{
		return getPendingEdges() == 0;
	}

	bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	struct Edge
	{
		uint32_t nSource;
		uint32_t nDestination;
		T value;
	};

	explicit SIRCSRBuilder( const sirContainerAllocator_t *pAllocator, uint32_t nVertices )
	    : m_pAllocator( pAllocator ),
	      m_edges( pAllocator ),
	      m_nVertices( nVertices )
	{
	}

	SIRContainerResult tryAddEdge( uint32_t iSource, uint32_t iDestination, const T& value )
	{
		if ( iSource >= m_nVertices || iDestination >= m_nVertices ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		return m_edges.tryEmplaceBack( Edge{ iSource, iDestination, value } );
	}

	SIRContainerResult tryFreeze( SIRCSRGraph<T>& output ) const
	{
		if ( m_nVertices == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		SIRVector<uint32_t> offsets( m_pAllocator );
		SIRVector<uint32_t> cursors( m_pAllocator );
		SIRVector<uint32_t> order( m_pAllocator );
		SIRVector<uint32_t> destinations( m_pAllocator );
		SIRVector<T> values( m_pAllocator );
		SIRContainerResult eResult = offsets.tryResize( m_nVertices + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = cursors.tryResize( m_nVertices );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = order.tryResize( m_edges.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		// Count each source row, then prefix-sum to exact CSR row offsets.
		for ( uint32_t iEdge = 0; iEdge < m_edges.getSize(); ++iEdge ) {
			++offsets[m_edges[iEdge].nSource + 1];
		}
		for ( uint32_t iVertex = 0; iVertex < m_nVertices; ++iVertex ) {
			offsets[iVertex + 1] += offsets[iVertex];
			cursors[iVertex] = offsets[iVertex];
		}
		// Arrange *indices*, not potentially large payload-bearing edges.
		for ( uint32_t iEdge = 0; iEdge < m_edges.getSize(); ++iEdge ) {
			const uint32_t iSource = m_edges[iEdge].nSource;
			order[cursors[iSource]++] = iEdge;
		}
		for ( uint32_t iVertex = 0; iVertex < m_nVertices; ++iVertex ) {
			const uint32_t iBegin = offsets[iVertex];
			const uint32_t iEnd = offsets[iVertex + 1];
			if ( iEnd - iBegin > 1 ) {
				std::sort( order.begin() + iBegin, order.begin() + iEnd, [&]( uint32_t iLeft, uint32_t iRight ) {
					return m_edges[iLeft].nDestination < m_edges[iRight].nDestination;
				} );
			}
		}
		eResult = destinations.tryReserve( m_edges.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = values.tryReserve( m_edges.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iOrder = 0; iOrder < order.getSize(); ++iOrder ) {
			const Edge& edge = m_edges[order[iOrder]];
			destinations.tryEmplaceBack( edge.nDestination );
			values.tryEmplaceBack( edge.value );
		}
		output.m_offsets = std::move( offsets );
		output.m_destinations = std::move( destinations );
		output.m_values = std::move( values );
		output.m_nVertices = m_nVertices;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	using value_type = Edge;

	void clear( void ) noexcept
	{
		m_edges.clear();
	}

	uint32_t getPendingEdges( void ) const noexcept
	{
		return m_edges.getSize();
	}

private:
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<Edge> m_edges;
	uint32_t m_nVertices;
};
#endif /* __SIR_CSRBUILDER_H__ */
