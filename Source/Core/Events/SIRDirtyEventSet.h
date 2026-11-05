/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRDIRTYEVENTSET_H__
#define __SIRENGINE_SIRDIRTYEVENTSET_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <stdint.h>
#include <string.h>
#if defined( _MSC_VER )
#include <intrin.h>
#endif

// Owner-thread bounded coalescing: the most recent generation wins for a slot.
// IDs are 1-based; the owner validates generation on delivery. A drain
// snapshots membership before callbacks so reentrant marks wait for next drain.
// The bit-only variant avoids storing one generation per potential resource.
template <uint32_t Capacity, bool TrackGeneration = true>
class SIRDirtyEventSet
{
public:
	typedef bool ( *validator_t )( void *, sirCompactEventRef_t ) noexcept;
	typedef void ( *callback_t )( void *, sirCompactEventRef_t ) noexcept;
	static_assert( Capacity > 0, "Dirty set must have valid slots" );

	SIRDirtyEventSet( void ) noexcept
	    : m_nPending( 0 ), m_nCoalesced( 0 ), m_nRejected( 0 ), m_bDraining( false ), m_bDiscardSnapshot( false )
	{
	}
	SIRDirtyEventSet( const SIRDirtyEventSet& ) = delete;
	SIRDirtyEventSet& operator=( const SIRDirtyEventSet& ) = delete;

	bool mark( uint32_t nSlot, uint32_t nGeneration = 1 ) noexcept
	{
		if ( nSlot == 0 || nSlot > Capacity || nGeneration == 0 ) {
			return false;
		}
		const uint32_t nIndex = nSlot - 1;
		const uint32_t nWord = nIndex >> 6;
		const uint64_t nBit = UINT64_C( 1 ) << ( nIndex & 63 );
		if constexpr ( TrackGeneration ) {
			m_nGenerations[nIndex] = nGeneration;
		}
		if ( ( m_nWords[nWord] & nBit ) != 0 ) {
			++m_nCoalesced;
		} else {
			m_nWords[nWord] |= nBit;
			++m_nPending;
		}
		return true;
	}

	uint32_t drain( void *pContext, callback_t pCallback, validator_t pValidator = nullptr, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( pCallback == nullptr || m_bDraining || nBudget == 0 || m_nPending == 0 ) {
			return 0;
		}
		m_bDraining = true;
		m_bDiscardSnapshot = false;
		uint64_t szSnapshot[( Capacity + 63 ) / 64];
		memcpy( szSnapshot, m_nWords, sizeof( szSnapshot ) );
		uint32_t nExamined = 0;
		uint32_t nDelivered = 0;
		for ( uint32_t iWord = 0; iWord < ( Capacity + 63 ) / 64 && nExamined < nBudget && !m_bDiscardSnapshot; ++iWord ) {
			uint64_t nBits = szSnapshot[iWord];
			while ( nBits != 0 && nExamined < nBudget && !m_bDiscardSnapshot ) {
#if defined( _MSC_VER )
				unsigned long nOffset = 0;
				_BitScanForward64( &nOffset, nBits );
#else
				const uint32_t nOffset = ( uint32_t )__builtin_ctzll( nBits );
#endif
				const uint64_t nBit = UINT64_C( 1 ) << nOffset;
				const uint32_t nIndex = iWord * 64 + ( uint32_t )nOffset;
				nBits &= ~nBit;
				m_nWords[iWord] &= ~nBit;
				--m_nPending;
				++nExamined;
				const sirCompactEventRef_t ref{ nIndex + 1, TrackGeneration ? m_nGenerations[nIndex] : 1 };
				if ( pValidator == nullptr || pValidator( pContext, ref ) ) {
					pCallback( pContext, ref );
					++nDelivered;
				} else {
					++m_nRejected;
				}
			}
		}
		m_bDraining = false;
		return nDelivered;
	}

	uint32_t pendingCount( void ) const noexcept
	{
		return m_nPending;
	}
	uint64_t coalescedCount( void ) const noexcept
	{
		return m_nCoalesced;
	}
	uint64_t staleCount( void ) const noexcept
	{
		return m_nRejected;
	}
	void clear( void ) noexcept
	{
		if ( m_bDraining ) {
			m_bDiscardSnapshot = true;
		}
		for ( uint32_t i = 0; i < ( Capacity + 63 ) / 64; ++i ) {
			m_nWords[i] = 0;
		}
		m_nPending = 0;
	}

private:
	uint64_t m_nWords[( Capacity + 63 ) / 64]{};
	uint32_t m_nGenerations[TrackGeneration ? Capacity : 1]{};
	uint32_t m_nPending;
	uint64_t m_nCoalesced;
	uint64_t m_nRejected;
	bool m_bDraining;
	bool m_bDiscardSnapshot;
};

#endif // __SIRENGINE_SIRDIRTYEVENTSET_H__
