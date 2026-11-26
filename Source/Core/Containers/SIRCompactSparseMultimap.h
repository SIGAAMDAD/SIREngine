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

#ifndef __SIR_COMPACTSPARSEMULTIMAP_H__
#define __SIR_COMPACTSPARSEMULTIMAP_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <algorithm>
#include <functional>

template <typename K, typename V, typename Less = std::less<K>>
class SIRCompactSparseMultimap
{
public:
	struct Entry
	{
		K key;
		V value;
	};

	explicit SIRCompactSparseMultimap( const sirContainerAllocator_t *pAllocator, Less less = Less() )
	    : m_pAllocator( pAllocator ),
	      m_keys( pAllocator ),
	      m_offsets( pAllocator ),
	      m_values( pAllocator ),
	      m_less( std::move( less ) )
	{
	}

	SIRContainerResult tryBuild( const Entry *pEntries, uint32_t nCount )
	{
		if ( nCount == UINT32_MAX || ( nCount && pEntries == nullptr ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<Entry> sorted( m_pAllocator );
		SIRContainerResult eResult = sorted.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			sorted.tryEmplaceBack( pEntries[iIndex] );
		}
		if ( sorted.getSize() > 1 ) {
			std::sort( sorted.begin(), sorted.end(), [&]( const Entry& a, const Entry& b ) { return m_less( a.key, b.key ); } );
		}
		SIRVector<K> keys( m_pAllocator );
		SIRVector<uint32_t> offsets( m_pAllocator );
		SIRVector<V> values( m_pAllocator );
		eResult = keys.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = offsets.tryReserve( nCount + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = values.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			if ( keys.isEmpty() || m_less( keys[keys.getSize() - 1], sorted[iIndex].key ) ) {
				keys.tryEmplaceBack( sorted[iIndex].key );
				offsets.tryEmplaceBack( iIndex );
			}
			values.tryEmplaceBack( sorted[iIndex].value );
		}
		offsets.tryEmplaceBack( nCount );
		m_keys = std::move( keys );
		m_offsets = std::move( offsets );
		m_values = std::move( values );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool find( const K& key, const V **ppValues, uint32_t *pCount ) const
	{
		if ( ppValues == nullptr || pCount == nullptr ) {
			return false;
		}
		uint32_t nLo = 0;
		uint32_t nHi = m_keys.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			if ( m_less( m_keys[iMid], key ) ) {
				nLo = iMid + 1;
			} else {
				nHi = iMid;
			}
		}
		if ( nLo == m_keys.getSize() || m_less( key, m_keys[nLo] ) ) {
			return false;
		}
		*ppValues = m_values.data() + m_offsets[nLo];
		*pCount = m_offsets[nLo + 1] - m_offsets[nLo];
		return true;
	}

	uint32_t getKeyCount( void ) const noexcept
	{
		return m_keys.getSize();
	}

	uint32_t getValueCount( void ) const noexcept
	{
		return m_values.getSize();
	}

	publicprivate :

	    const sirContainerAllocator_t *m_pAllocator;
	SIRVector<K> m_keys;
	SIRVector<uint32_t> m_offsets;
	SIRVector<V> m_values;
	Less m_less;
};
#endif /* __SIR_COMPACTSPARSEMULTIMAP_H__ */
