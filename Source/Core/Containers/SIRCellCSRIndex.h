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

#ifndef __SIR_CELLCSRINDEX_H__
#define __SIR_CELLCSRINDEX_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <algorithm>
#include <functional>

template <typename CellKey, typename Handle, typename Less = std::less<CellKey>>
class SIRCellCSRIndex
{
public:
	struct Entry
	{
		CellKey cell;
		Handle handle;
	};

	explicit SIRCellCSRIndex( const sirContainerAllocator_t *pAllocator, Less less = Less() )
	    : m_pAllocator( pAllocator ),
	      m_cells( pAllocator ),
	      m_offsets( pAllocator ),
	      m_handles( pAllocator ),
	      m_less( std::move( less ) )
	{
	}

	SIRContainerResult tryBuild( const Entry *pEntries, uint32_t nCount )
	{
		if ( nCount == UINT32_MAX || ( nCount > 0 && pEntries == nullptr ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		// Most pre-cooked cell directories arrive grouped by cell already.
		// An early-out sortedness probe avoids copying and sorting that input;
		// for shuffled inputs the first inversion normally ends the probe.
		bool bOrdered = true;
		for ( uint32_t iEntry = 1; iEntry < nCount; ++iEntry ) {
			if ( m_less( pEntries[iEntry].cell, pEntries[iEntry - 1].cell ) ) {
				bOrdered = false;
				break;
			}
		}
		SIRVector<Entry> sorted( m_pAllocator );
		const Entry *pOrdered = pEntries;
		SIRContainerResult eResult = SIRContainerResult::SIR_CONTAINER_OK;
		if ( !bOrdered ) {
			eResult = sorted.tryReserve( nCount );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			for ( uint32_t iEntry = 0; iEntry < nCount; ++iEntry ) {
				eResult = sorted.tryEmplaceBack( pEntries[iEntry] );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
			}
			if ( sorted.getSize() > 1 ) {
				std::sort( sorted.begin(), sorted.end(), [&]( const Entry& a, const Entry& b ) { return m_less( a.cell, b.cell ); } );
			}
			pOrdered = sorted.data();
		}
		SIRVector<CellKey> cells( m_pAllocator );
		SIRVector<uint32_t> offsets( m_pAllocator );
		SIRVector<Handle> handles( m_pAllocator );
		eResult = cells.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = offsets.tryReserve( nCount + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = handles.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			if ( cells.isEmpty() || m_less( cells[cells.getSize() - 1], pOrdered[iIndex].cell ) ) {
				cells.tryEmplaceBack( pOrdered[iIndex].cell );
				offsets.tryEmplaceBack( iIndex );
			}
			handles.tryEmplaceBack( pOrdered[iIndex].handle );
		}
		offsets.tryEmplaceBack( nCount );
		m_cells = std::move( cells );
		m_offsets = std::move( offsets );
		m_handles = std::move( handles );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool find( const CellKey& cell, const Handle **ppData, uint32_t *pCount ) const
	{
		if ( ppData == nullptr || pCount == nullptr ) {
			return false;
		}
		uint32_t nLo = 0;
		uint32_t nHi = m_cells.getSize();
		while ( nLo < nHi ) {
			uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( m_less( m_cells[iMid], cell ) ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		if ( nLo == m_cells.getSize() || m_less( cell, m_cells[nLo] ) ) {
			return false;
		}
		*ppData = m_handles.data() + m_offsets[nLo];
		*pCount = m_offsets[nLo + 1] - m_offsets[nLo];
		return true;
	}

	uint32_t getCellCount( void ) const noexcept
	{
		return m_cells.getSize();
	}

	publicprivate :

	    const sirContainerAllocator_t *m_pAllocator;
	SIRVector<CellKey> m_cells;
	SIRVector<uint32_t> m_offsets;
	SIRVector<Handle> m_handles;
	Less m_less;
};
#endif /* __SIR_CELLCSRINDEX_H__ */
