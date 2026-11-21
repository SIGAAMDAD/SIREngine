/* SIREngine - MPL-2.0. Mutable CSR-like row ranges with reserved gaps per row. */
#ifndef SIR_PACKED_CSR_GRAPH_H
#define SIR_PACKED_CSR_GRAPH_H
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

private:
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<std::optional<Edge>> m_storage;
	SIRVector<uint32_t> m_offsets;
	SIRVector<uint32_t> m_sizes;
	SIRVector<uint32_t> m_capacities;
	uint32_t m_nEdges = 0;
	sirContainerResult_t growRow( uint32_t iGrow )
	{
		const uint32_t nOld = m_capacities[iGrow];
		if ( nOld > UINT32_MAX / 2 ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const uint32_t nNew = nOld == 0 ? 4 : nOld * 2;
		if ( m_storage.getSize() > UINT32_MAX - ( nNew - nOld ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		SIRVector<std::optional<Edge>> staged( m_pAllocator );
		sirContainerResult_t eResult = staged.tryResize( m_storage.getSize() + nNew - nOld );
		if ( eResult != SIR_CONTAINER_OK ) {
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
		return SIR_CONTAINER_OK;
	}

public:
	explicit SIRPackedCSRGraph( const sirContainerAllocator_t *pAllocator )
	    : m_pAllocator( pAllocator ), m_storage( pAllocator ), m_offsets( pAllocator ), m_sizes( pAllocator ), m_capacities( pAllocator )
	{
	}
	sirContainerResult_t tryInitialize( uint32_t nVertices, uint32_t nCapacityPerRow = 4 )
	{
		if ( nVertices == 0 || nCapacityPerRow == 0 || nVertices > UINT32_MAX / nCapacityPerRow ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<std::optional<Edge>> storage( m_pAllocator );
		SIRVector<uint32_t> offsets( m_pAllocator );
		SIRVector<uint32_t> sizes( m_pAllocator );
		SIRVector<uint32_t> capacities( m_pAllocator );
		sirContainerResult_t eResult = storage.tryResize( nVertices * nCapacityPerRow );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = offsets.tryResize( nVertices );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = sizes.tryResize( nVertices );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = capacities.tryResize( nVertices );
		if ( eResult != SIR_CONTAINER_OK ) {
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
		return SIR_CONTAINER_OK;
	}
	sirContainerResult_t tryAddEdge( uint32_t iSource, uint32_t iDestination, const T& value )
	{
		if ( m_nEdges == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( iSource >= m_sizes.getSize() || iDestination >= m_sizes.getSize() ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		// Copy potentially aliased input before a row resize can relocate it.
		Edge staged{ iDestination, value };
		if ( m_sizes[iSource] == m_capacities[iSource] ) {
			sirContainerResult_t eResult = growRow( iSource );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		m_storage[m_offsets[iSource] + m_sizes[iSource]].emplace( std::move( staged ) );
		++m_sizes[iSource];
		++m_nEdges;
		return SIR_CONTAINER_OK;
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
};
#endif
