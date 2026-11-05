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

#include <Core/Platform/Input/SIRInputHistory.h>
#include <limits.h>

SIRInputHistory::SIRInputHistory( void ) noexcept : m_pStorage( nullptr ), m_nCapacity( 0 ), m_nCount( 0 ), m_nHead( 0 )
{
}

uint32_t SIRInputHistory::count( void ) const noexcept
{
	return m_nCount;
}

uint32_t SIRInputHistory::capacity( void ) const noexcept
{
	return m_nCapacity;
}

uint64_t SIRInputHistory::oldestTick( void ) const noexcept
{
	return m_nCount == 0 ? 0 : m_pStorage[m_nHead].nTick;
}

uint64_t SIRInputHistory::newestTick( void ) const noexcept
{
	return m_nCount == 0 ? 0 : m_pStorage[( uint32_t )( ( ( uint64_t )m_nHead + m_nCount - 1 ) % m_nCapacity )].nTick;
}

bool SIRInputHistory::initialize( sirInputReplayFrame_t *pStorage, uint32_t nCapacity ) noexcept
{
	if ( pStorage == nullptr || nCapacity == 0 ) {
		return false;
	}
	m_pStorage = pStorage;
	m_nCapacity = nCapacity;
	m_nCount = 0;
	m_nHead = 0;
	return true;
}

bool SIRInputHistory::append( const sirInputReplayFrame_t& frame ) noexcept
{
	if ( m_pStorage == nullptr || !SIRInputReplayTape::validateFrame( frame ) ) {
		return false;
	}
	if ( m_nCount != 0 ) {
		const uint64_t nNewest = newestTick();
		if ( nNewest == UINT64_MAX || frame.nTick != nNewest + 1 ) {
			return false;
		}
	}
	const uint32_t iSlot = ( uint32_t )( ( ( uint64_t )m_nHead + m_nCount ) % m_nCapacity );
	m_pStorage[iSlot] = frame;
	if ( m_nCount == m_nCapacity ) {
		m_nHead = ( m_nHead + 1 ) % m_nCapacity;
	} else {
		++m_nCount;
	}
	return true;
}

bool SIRInputHistory::read( uint64_t nTick, sirInputReplayFrame_t *pOut ) const noexcept
{
	if ( pOut == nullptr || m_nCount == 0 || nTick < oldestTick() || nTick > newestTick() ) {
		return false;
	}
	const uint32_t iSlot = ( uint32_t )( ( ( uint64_t )m_nHead + ( nTick - oldestTick() ) ) % m_nCapacity );
	*pOut = m_pStorage[iSlot];
	return true;
}

bool SIRInputHistory::rewindFrom( uint64_t nTick ) noexcept
{
	if ( m_nCount == 0 || nTick < oldestTick() || ( newestTick() != UINT64_MAX && nTick > newestTick() + 1 ) ) {
		return false;
	}
	m_nCount = ( uint32_t )( nTick - oldestTick() );
	if ( m_nCount == 0 ) {
		m_nHead = 0;
	}
	return true;
}

bool SIRInputHistory::replaceFrom( const sirInputReplayFrame_t& frame ) noexcept
{
	if ( !SIRInputReplayTape::validateFrame( frame ) || m_nCount == 0 || frame.nTick < oldestTick() || frame.nTick > newestTick() ) {
		return false;
	}
	// The validated replacement tick is inside the contiguous ring, so rewinding cannot fail.
	( void )rewindFrom( frame.nTick );
	return append( frame );
}

static void sirHistoryHashInteger( uint64_t *pHash, uint64_t nValue, uint32_t nBytes ) noexcept
{
	for ( uint32_t i = 0; i < nBytes; ++i ) {
		*pHash = ( *pHash ^ ( uint8_t )( nValue >> ( i * 8 ) ) ) * UINT64_C( 1099511628211 );
	}
}

bool SIRInputHistory::hashFrame( const sirInputReplayFrame_t& frame, uint64_t *pOut ) noexcept
{
	if ( pOut == nullptr || !SIRInputReplayTape::validateFrame( frame ) ) {
		return false;
	}
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	sirHistoryHashInteger( &nHash, frame.nTick, 8 );
	sirHistoryHashInteger( &nHash, frame.nCount, 4 );
	for ( uint32_t i = 0; i < frame.nCount; ++i ) {
		const sirInputActionState_t& state = frame.actions[i];
		sirHistoryHashInteger( &nHash, state.nActionId, 4 );
		sirHistoryHashInteger( &nHash, state.nUser, 4 );
		sirHistoryHashInteger( &nHash, ( uint32_t )state.nValue, 4 );
		sirHistoryHashInteger( &nHash, state.nTransitions, 4 );
		const uint8_t nFlags = ( state.bHeld ? 1 : 0 ) | ( state.bPressed ? 2 : 0 ) | ( state.bReleased ? 4 : 0 );
		sirHistoryHashInteger( &nHash, nFlags, 1 );
	}
	*pOut = nHash;
	return true;
}

bool SIRInputHistory::digest( uint64_t nTick, uint64_t *pOut ) const noexcept
{
	if ( pOut == nullptr || m_nCount == 0 || nTick < oldestTick() || nTick > newestTick() ) {
		return false;
	}
	const uint32_t iSlot = ( uint32_t )( ( ( uint64_t )m_nHead + ( nTick - oldestTick() ) ) % m_nCapacity );
	return hashFrame( m_pStorage[iSlot], pOut );
}

void SIRInputHistory::clear( void ) noexcept
{
	m_nCount = 0;
	m_nHead = 0;
}
