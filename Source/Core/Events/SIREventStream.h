/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIREVENTSTREAM_H__
#define __SIRENGINE_SIREVENTSTREAM_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <string.h>
#include <type_traits>

// Owner-thread event stream; append to write buffer, drain read buffer.
// Nested drain is rejected; callbacks may append events for the next drain.
template <typename TPayload, uint32_t Capacity = 256>
class SIREventStream
{
public:
	typedef void ( *callback_t )( void *pContext, const TPayload& payload ) noexcept;
	static_assert( Capacity > 0, "Stream capacity must be positive" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Event streams carry POD payloads" );

	SIREventStream( void ) noexcept : m_nWrite( 0 ), m_nCounts{}, m_bDraining( false )
	{
	}
	SIREventStream( const SIREventStream& ) = delete;
	SIREventStream& operator=( const SIREventStream& ) = delete;

	bool append( const TPayload& payload ) noexcept
	{
		const uint32_t nWrite = m_nWrite;
		if ( m_nCounts[nWrite] == Capacity ) {
			return false;
		}
		m_Events[nWrite][m_nCounts[nWrite]++] = payload;
		return true;
	}

	// Accepts the prefix that fits the current write buffer. This is an
	// owner-thread bulk copy, with no per-item dispatch or synchronization.
	uint32_t appendBatch( const TPayload *pPayloads, uint32_t nCount ) noexcept
	{
		if ( pPayloads == nullptr || nCount == 0 ) {
			return 0;
		}
		const uint32_t nRoom = Capacity - m_nCounts[m_nWrite];
		const uint32_t nWritten = nCount < nRoom ? nCount : nRoom;
		memcpy( &m_Events[m_nWrite][m_nCounts[m_nWrite]], pPayloads, nWritten * sizeof( TPayload ) );
		m_nCounts[m_nWrite] += nWritten;
		return nWritten;
	}

	uint32_t drain( void *pContext, callback_t pCallback ) noexcept
	{
		if ( m_bDraining || pCallback == nullptr ) {
			return 0;
		}
		const uint32_t nRead = m_nWrite;
		m_nWrite ^= 1;
		m_bDraining = true;
		const uint32_t nCount = m_nCounts[nRead];
		for ( uint32_t i = 0; i < nCount; ++i ) {
			pCallback( pContext, m_Events[nRead][i] );
		}
		m_nCounts[nRead] = 0;
		m_bDraining = false;
		return nCount;
	}

	uint32_t pendingCount( void ) const noexcept
	{
		return m_nCounts[m_nWrite];
	}
	void clear( void ) noexcept
	{
		if ( !m_bDraining ) {
			m_nCounts[0] = 0;
			m_nCounts[1] = 0;
		}
	}

private:
	TPayload m_Events[2][Capacity];
	uint32_t m_nWrite;
	uint32_t m_nCounts[2];
	bool m_bDraining;
};

#endif // __SIRENGINE_SIREVENTSTREAM_H__
