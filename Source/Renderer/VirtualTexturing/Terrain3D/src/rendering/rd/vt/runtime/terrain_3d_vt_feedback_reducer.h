// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_REDUCER_H
#define TERRAIN3D_VT_FEEDBACK_REDUCER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"

#define T3D_VT_FEEDBACK_MAX_PARENT_DEPTH 32u

typedef bool ( *t3dVTFeedbackPageLookup_t )( void *, uint32_t, uint32_t&, uint16_t& );

struct t3dVTFeedbackCandidate_t
{
	uint32_t m_nPage;
	uint32_t m_nFrequency;
	uint32_t m_nScore;
	uint16_t m_nMip;
	uint16_t m_nReserved;
};

struct t3dVTFeedbackPolicy_t
{
	uint32_t m_nMinimumSamples = 2u;
	uint32_t m_nMaxCandidates = 85u;
	uint32_t m_nParentBoost = 512u;
	uint32_t m_nRootBoost = 1536u;
	uint32_t m_nMotionLevel = 0u;
};

struct t3dVTFeedbackReductionStats_t
{
	uint32_t m_nSamples = 0u;
	uint32_t m_nValidSamples = 0u;
	uint32_t m_nUniquePages = 0u;
	// Directly observed pages before ancestry promotion. This measures grid
	// diversity without mixing in the CPU reducer's promoted parents.
	uint32_t m_nDirectUniquePages = 0u;
	uint32_t m_nParentPromotions = 0u;
	uint32_t m_nRejectedNoise = 0u;
	uint32_t m_nInvalidParents = 0u;
	uint32_t m_nCandidates = 0u;
};

// Only cold-owned buffers. Each capture visits touched pages, not the entire DB.
class Terrain3DVTFeedbackReducer
{
public:
	void bindCold( uint32_t nCapacity, uint32_t nCandidateCapacity, uint32_t *pEpoch, uint32_t *pDirect, uint32_t *pAggregate,
	               uint32_t *pTouched, t3dVTFeedbackCandidate_t *pCandidates )
	{
		m_nCapacity = nCapacity;
		m_nCandidateCapacity = nCandidateCapacity;
		m_pEpoch = pEpoch;
		m_pDirect = pDirect;
		m_pAggregate = pAggregate;
		m_pTouched = pTouched;
		m_pCandidates = pCandidates;
		m_nEpoch = m_nTouched = m_nSelected = 0u;
		if ( pEpoch )
			memset( pEpoch, 0, ( size_t )nCapacity * sizeof( uint32_t ) );
	}
	void resetCold()
	{
		bindCold( 0u, 0u, NULL, NULL, NULL, NULL, NULL );
	}
	// Explicit invalidation for rejected versioned readbacks; never publish an
	// earlier capture's selected candidates after a failed preflight.
	void discardCapture()
	{
		m_nTouched = m_nSelected = 0u;
	}
	uint32_t getTouchedCount() const
	{
		return m_nTouched;
	}
	uint32_t getTouchedPage( uint32_t i ) const
	{
		return i < m_nTouched ? m_pTouched[i] : UINT32_MAX;
	}
	uint32_t getSelectedCount() const
	{
		return m_nSelected;
	}
	bool hasDirectPage( const uint32_t nPage ) const
	{
		return m_nTouched != 0u && m_nEpoch != 0u && m_pEpoch != NULL && m_pDirect != NULL && nPage < m_nCapacity &&
		       m_pEpoch[nPage] == m_nEpoch && m_pDirect[nPage] != 0u;
	}
	const t3dVTFeedbackCandidate_t& getSelected( uint32_t i ) const
	{
		return m_pCandidates[i];
	}

	bool reduce( const uint8_t *pWords, uint32_t nWords, uint32_t nPageCount, t3dVTFeedbackPageLookup_t pLookup, void *pUser,
	             const t3dVTFeedbackPolicy_t& policy, t3dVTFeedbackReductionStats_t& stats )
	{
		return reduceInternal( pWords, nWords, nPageCount, pLookup, pUser, policy, stats, false );
	}

	// P31b: consume a complete version-3 multi-request capture. A cell that
	// overflows its four unique page slots invalidates the ENTIRE capture.
	// No partial demand is admitted in that case; the caller keeps v2 feedback.
	bool reduceMulti( const uint8_t *pCells, uint32_t nCells, uint32_t nPageCount, t3dVTFeedbackPageLookup_t pLookup, void *pUser,
	                  const t3dVTFeedbackPolicy_t& policy, t3dVTFeedbackReductionStats_t& stats )
	{
		stats = t3dVTFeedbackReductionStats_t{};
		discardCapture();
		if ( !pCells || nCells > 240u * 136u )
			return false;
		for ( uint32_t i = 0u; i < nCells; ++i ) {
			const uint8_t *const cell = pCells + ( size_t )i * sizeof( t3dVTFeedbackMultiRecord_t );
			uint32_t overflow = 0u;
			memcpy( &overflow, cell + 8u * sizeof( uint32_t ), sizeof( overflow ) );
			if ( overflow != 0u )
				return false;
			uint32_t countSum = 0u;
			for ( uint32_t j = 0u; j < T3D_VT_FEEDBACK_MULTI_PAGES; ++j ) {
				uint32_t page = 0u, weight = 0u;
				memcpy( &page, cell + j * sizeof( uint32_t ), sizeof( page ) );
				memcpy( &weight, cell + ( T3D_VT_FEEDBACK_MULTI_PAGES + j ) * sizeof( uint32_t ), sizeof( weight ) );
				if ( page == T3D_VT_FEEDBACK_GRID_INVALID_PAGE ) {
					if ( weight != 0u )
						return false;
					continue;
				}
				if ( page >= nPageCount || weight == 0u ||
				     weight > T3D_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL * T3D_VT_FEEDBACK_MULTI_MAX_LAYERS )
					return false;
				if ( UINT32_MAX - countSum < weight )
					return false;
				countSum += weight;
				for ( uint32_t k = 0u; k < j; ++k ) {
					uint32_t other = 0u;
					memcpy( &other, cell + k * sizeof( uint32_t ), sizeof( other ) );
					if ( other == page )
						return false;
				}
			}
			if ( countSum > T3D_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL * T3D_VT_FEEDBACK_MULTI_MAX_LAYERS )
				return false;
		}
		return reduceInternal( pCells, nCells, nPageCount, pLookup, pUser, policy, stats, true );
	}

private:
	bool reduceInternal( const uint8_t *pWords, uint32_t nWords, uint32_t nPageCount, t3dVTFeedbackPageLookup_t pLookup, void *pUser,
	                     const t3dVTFeedbackPolicy_t& policy, t3dVTFeedbackReductionStats_t& stats, const bool bMulti )
	{
		stats = t3dVTFeedbackReductionStats_t{};
		m_nTouched = m_nSelected = 0u;
		if ( !pWords || !pLookup || !m_pEpoch || !m_pDirect || !m_pAggregate || !m_pTouched || !m_pCandidates || nPageCount > m_nCapacity ||
		     m_nCandidateCapacity == 0u )
			return false;
		if ( ++m_nEpoch == 0u ) {
			memset( m_pEpoch, 0, ( size_t )m_nCapacity * sizeof( uint32_t ) );
			m_nEpoch = 1u;
		}
		stats.m_nSamples = bMulti ? 0u : nWords;
		for ( uint32_t i = 0u; i < nWords; ++i ) {
			const uint32_t samples = bMulti ? T3D_VT_FEEDBACK_MULTI_PAGES : 1u;
			for ( uint32_t j = 0u; j < samples; ++j ) {
				uint32_t page = 0u, weight = 1u;
				const uint8_t *const word = pWords + ( size_t )i * ( bMulti ? sizeof( t3dVTFeedbackMultiRecord_t ) : sizeof( uint32_t ) );
				memcpy( &page, word + ( size_t )j * sizeof( uint32_t ), sizeof( page ) );
				if ( bMulti )
					memcpy( &weight, word + ( size_t )( T3D_VT_FEEDBACK_MULTI_PAGES + j ) * sizeof( uint32_t ), sizeof( weight ) );
				if ( bMulti )
					stats.m_nSamples = add( stats.m_nSamples, weight );
				if ( page >= nPageCount || weight == 0u )
					continue;
				stats.m_nValidSamples = add( stats.m_nValidSamples, weight );
				visit( page );
				m_pDirect[page] = add( m_pDirect[page], weight );
				m_pAggregate[page] = add( m_pAggregate[page], weight );
			}
		}
		const uint32_t leafCount = m_nTouched;
		stats.m_nDirectUniquePages = leafCount;
		for ( uint32_t i = 0u; i < leafCount; ++i ) {
			const uint32_t leaf = m_pTouched[i], count = m_pDirect[leaf];
			uint32_t page = leaf;
			uint32_t ancestors[T3D_VT_FEEDBACK_MAX_PARENT_DEPTH + 1u];
			ancestors[0] = leaf;
			for ( uint32_t depth = 0u; depth < T3D_VT_FEEDBACK_MAX_PARENT_DEPTH; ++depth ) {
				uint32_t parent = UINT32_MAX;
				uint16_t mip = 0u;
				if ( !pLookup( pUser, page, parent, mip ) ) {
					++stats.m_nInvalidParents;
					break;
				}
				if ( parent == UINT32_MAX )
					break;
				if ( parent >= nPageCount || parent == page ) {
					++stats.m_nInvalidParents;
					break;
				}
				bool cycle = false;
				for ( uint32_t j = 0u; j <= depth; ++j ) {
					if ( ancestors[j] == parent ) {
						cycle = true;
						break;
					}
				}
				if ( cycle || depth + 1u == T3D_VT_FEEDBACK_MAX_PARENT_DEPTH ) {
					++stats.m_nInvalidParents;
					break;
				}
				ancestors[depth + 1u] = parent;
				visit( parent );
				m_pAggregate[parent] = add( m_pAggregate[parent], count );
				++stats.m_nParentPromotions;
				page = parent;
			}
		}
		stats.m_nUniquePages = m_nTouched;
		const uint32_t limit = min( policy.m_nMaxCandidates, m_nCandidateCapacity );
		for ( uint32_t i = 0u; i < m_nTouched; ++i ) {
			const uint32_t page = m_pTouched[i];
			uint32_t parent = UINT32_MAX;
			uint16_t mip = 0u;
			if ( !pLookup( pUser, page, parent, mip ) )
				continue;
			const uint32_t count = m_pAggregate[page];
			const bool root = parent == UINT32_MAX;
			if ( count < policy.m_nMinimumSamples && !root ) {
				++stats.m_nRejectedNoise;
				continue;
			}
			const bool promoted = count > m_pDirect[page];
			uint32_t score = add( count > UINT32_MAX / 64u ? UINT32_MAX : count * 64u, root       ? policy.m_nRootBoost
			                                                                           : promoted ? policy.m_nParentBoost
			                                                                                      : 0u );
			if ( policy.m_nMotionLevel ) {
				const uint32_t level = min( policy.m_nMotionLevel, 2u );
				score = add( score, ( root ? 4096u : promoted ? 768u : 0u ) * level );
			}
			const t3dVTFeedbackCandidate_t candidate{ page, count, score, mip, 0u };
			if ( m_nSelected < limit ) {
				m_pCandidates[m_nSelected] = candidate;
				siftUp( m_nSelected++ );
			} else if ( limit && worse( m_pCandidates[0], candidate ) ) {
				m_pCandidates[0] = candidate;
				siftDown( 0u, m_nSelected );
			}
		}
		for ( uint32_t n = m_nSelected; n > 1u; --n ) {
			swap( 0u, n - 1u );
			siftDown( 0u, n - 1u );
		}
		stats.m_nCandidates = m_nSelected;
		return true;
	}
	static uint32_t min( uint32_t a, uint32_t b )
	{
		return a < b ? a : b;
	}
	static uint32_t add( uint32_t a, uint32_t b )
	{
		return UINT32_MAX - a < b ? UINT32_MAX : a + b;
	}
	static bool worse( const t3dVTFeedbackCandidate_t& a, const t3dVTFeedbackCandidate_t& b )
	{
		return a.m_nScore < b.m_nScore || ( a.m_nScore == b.m_nScore && a.m_nPage > b.m_nPage );
	}
	void visit( uint32_t page )
	{
		if ( m_pEpoch[page] == m_nEpoch )
			return;
		m_pEpoch[page] = m_nEpoch;
		m_pDirect[page] = m_pAggregate[page] = 0u;
		m_pTouched[m_nTouched++] = page;
	}
	void swap( uint32_t a, uint32_t b )
	{
		const t3dVTFeedbackCandidate_t tmp = m_pCandidates[a];
		m_pCandidates[a] = m_pCandidates[b];
		m_pCandidates[b] = tmp;
	}
	void siftUp( uint32_t i )
	{
		while ( i ) {
			uint32_t parent = ( i - 1u ) / 2u;
			if ( !worse( m_pCandidates[i], m_pCandidates[parent] ) )
				break;
			swap( i, parent );
			i = parent;
		}
	}
	void siftDown( uint32_t i, uint32_t n )
	{
		for ( ;; ) {
			uint32_t child = 2u * i + 1u;
			if ( child >= n )
				break;
			if ( child + 1u < n && worse( m_pCandidates[child + 1u], m_pCandidates[child] ) )
				++child;
			if ( !worse( m_pCandidates[child], m_pCandidates[i] ) )
				break;
			swap( i, child );
			i = child;
		}
	}
	uint32_t m_nCapacity = 0u, m_nCandidateCapacity = 0u, m_nEpoch = 0u, m_nTouched = 0u, m_nSelected = 0u;
	uint32_t *m_pEpoch = NULL, *m_pDirect = NULL, *m_pAggregate = NULL, *m_pTouched = NULL;
	t3dVTFeedbackCandidate_t *m_pCandidates = NULL;
};
#endif // TERRAIN3D_VT_FEEDBACK_REDUCER_H
