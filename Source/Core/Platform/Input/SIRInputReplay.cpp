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

#include <Core/Platform/Input/SIRInputReplay.h>
#include <string.h>

SIRInputReplayTape::SIRInputReplayTape( void ) noexcept
    : m_pWrite( nullptr ), m_pRead( nullptr ), m_nCapacity( 0 ), m_nOffset( 0 ), m_nLastTick( 0 ), m_eMode( SIR_INPUT_REPLAY_IDLE )
{
}

void SIRInputReplayTape::write16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

void SIRInputReplayTape::write32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pBytes[i] = ( uint8_t )( nValue >> ( 8 * i ) );
	}
}

void SIRInputReplayTape::write64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		pBytes[i] = ( uint8_t )( nValue >> ( 8 * i ) );
	}
}

uint16_t SIRInputReplayTape::read16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )pBytes[0] | ( ( uint16_t )pBytes[1] << 8 );
}

uint32_t SIRInputReplayTape::read32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

uint64_t SIRInputReplayTape::read64( const uint8_t *pBytes ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t i = 0; i < 8; ++i ) {
		nValue |= ( uint64_t )pBytes[i] << ( 8 * i );
	}
	return nValue;
}

uint32_t SIRInputReplayTape::checksum( const uint8_t *pBytes, uint32_t nLength ) noexcept
{
	uint32_t nHash = UINT32_C( 2166136261 );
	for ( uint32_t i = 0; i < nLength; ++i ) {
		nHash = ( nHash ^ pBytes[i] ) * UINT32_C( 16777619 );
	}
	return nHash;
}

bool SIRInputReplayTape::valid( const sirInputActionState_t& state ) noexcept
{
	if ( state.nActionId == 0 || state.nUser >= SIR_INPUT_USER_CAPACITY ) {
		return false;
	}
	if ( state.nValue < -32767 || state.nValue > 32767 ) {
		return false;
	}
	return state.bHeld == ( state.nValue != 0 );
}

bool SIRInputReplayTape::beginRecord( uint8_t *pBuffer, uint32_t nCapacity ) noexcept
{
	if ( pBuffer == nullptr || nCapacity < SIR_INPUT_REPLAY_HEADER_BYTES ) {
		return false;
	}
	reset();
	m_pWrite = pBuffer;
	m_nCapacity = nCapacity;
	m_eMode = SIR_INPUT_REPLAY_RECORD;
	return true;
}

bool SIRInputReplayTape::append( uint64_t nTick, const SIRInputActions& actions, const sirInputReplayKey_t *pKeys,
                                 uint32_t nCount ) noexcept
{
	if ( m_eMode != SIR_INPUT_REPLAY_RECORD || nTick == 0 || nTick <= m_nLastTick || nCount > SIR_INPUT_ACTION_CAPACITY ||
	     ( nCount != 0 && pKeys == nullptr ) ) {
		return false;
	}
	const uint32_t nBytes = SIR_INPUT_REPLAY_HEADER_BYTES + nCount * SIR_INPUT_REPLAY_RECORD_BYTES;
	if ( nBytes > m_nCapacity - m_nOffset ) {
		return false;
	}

	// Preflight every key and its state before touching the caller's buffer.
	for ( uint32_t i = 0; i < nCount; ++i ) {
		sirInputActionState_t state = {};
		if ( !actions.query( pKeys[i].nActionId, pKeys[i].nUser, &state ) || !valid( state ) ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pKeys[i].nActionId == pKeys[j].nActionId && pKeys[i].nUser == pKeys[j].nUser ) {
				return false;
			}
		}
	}

	uint8_t *pBytes = m_pWrite + m_nOffset;
	write32( pBytes, SIR_INPUT_REPLAY_MAGIC );
	write16( pBytes + 4, 1 );
	write16( pBytes + 6, SIR_INPUT_REPLAY_HEADER_BYTES );
	write64( pBytes + 8, nTick );
	write32( pBytes + 16, nCount );
	write32( pBytes + 20, nBytes );
	write32( pBytes + 28, 0 );
	for ( uint32_t i = 0; i < nCount; ++i ) {
		sirInputActionState_t state = {};
		( void )actions.query( pKeys[i].nActionId, pKeys[i].nUser, &state );
		uint8_t *pRecord = pBytes + SIR_INPUT_REPLAY_HEADER_BYTES + i * SIR_INPUT_REPLAY_RECORD_BYTES;
		write32( pRecord, state.nActionId );
		write32( pRecord + 4, state.nUser );
		write32( pRecord + 8, ( uint32_t )state.nValue );
		write32( pRecord + 12, state.nTransitions );
		const uint32_t nFlags = ( state.bHeld ? 1U : 0U ) | ( state.bPressed ? 2U : 0U ) | ( state.bReleased ? 4U : 0U );
		write32( pRecord + 16, nFlags );
	}
	write32( pBytes + 24, checksum( pBytes + SIR_INPUT_REPLAY_HEADER_BYTES, nBytes - SIR_INPUT_REPLAY_HEADER_BYTES ) );
	m_nOffset += nBytes;
	m_nLastTick = nTick;
	return true;
}

bool SIRInputReplayTape::beginRead( const uint8_t *pBuffer, uint32_t nBytes ) noexcept
{
	if ( pBuffer == nullptr ) {
		return false;
	}
	reset();
	m_pRead = pBuffer;
	m_nCapacity = nBytes;
	m_eMode = SIR_INPUT_REPLAY_READ;
	return true;
}

bool SIRInputReplayTape::next( sirInputReplayFrame_t *pFrame ) noexcept
{
	if ( m_eMode != SIR_INPUT_REPLAY_READ || pFrame == nullptr || m_nCapacity - m_nOffset < SIR_INPUT_REPLAY_HEADER_BYTES ) {
		return false;
	}
	const uint8_t *pBytes = m_pRead + m_nOffset;
	const uint32_t nCount = read32( pBytes + 16 );
	const uint32_t nBytes = read32( pBytes + 20 );
	const uint64_t nTick = read64( pBytes + 8 );
	if ( read32( pBytes ) != SIR_INPUT_REPLAY_MAGIC || read16( pBytes + 4 ) != 1 || read16( pBytes + 6 ) != SIR_INPUT_REPLAY_HEADER_BYTES ||
	     read32( pBytes + 28 ) != 0 || nTick == 0 || nTick <= m_nLastTick || nCount > SIR_INPUT_ACTION_CAPACITY ) {
		return false;
	}
	const uint32_t nRequired = SIR_INPUT_REPLAY_HEADER_BYTES + nCount * SIR_INPUT_REPLAY_RECORD_BYTES;
	if ( nBytes != nRequired || nBytes > m_nCapacity - m_nOffset ||
	     read32( pBytes + 24 ) != checksum( pBytes + SIR_INPUT_REPLAY_HEADER_BYTES, nBytes - SIR_INPUT_REPLAY_HEADER_BYTES ) ) {
		return false;
	}

	sirInputReplayFrame_t candidate = {};
	candidate.nTick = nTick;
	candidate.nCount = nCount;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const uint8_t *pRecord = pBytes + SIR_INPUT_REPLAY_HEADER_BYTES + i * SIR_INPUT_REPLAY_RECORD_BYTES;
		sirInputActionState_t& state = candidate.actions[i];
		state.nActionId = read32( pRecord );
		state.nUser = read32( pRecord + 4 );
		state.nValue = ( int32_t )read32( pRecord + 8 );
		state.nTransitions = read32( pRecord + 12 );
		const uint32_t nFlags = read32( pRecord + 16 );
		if ( ( nFlags & ~7U ) != 0 ) {
			return false;
		}
		state.bHeld = ( nFlags & 1 ) != 0;
		state.bPressed = ( nFlags & 2 ) != 0;
		state.bReleased = ( nFlags & 4 ) != 0;
	}
	if ( !validateFrame( candidate ) ) {
		return false;
	}
	*pFrame = candidate;
	m_nOffset += nBytes;
	m_nLastTick = nTick;
	return true;
}

bool SIRInputReplayTape::seek( uint64_t nTick, sirInputReplayFrame_t *pFrame ) noexcept
{
	if ( m_eMode != SIR_INPUT_REPLAY_READ || pFrame == nullptr ) {
		return false;
	}
	rewind();
	sirInputReplayFrame_t candidate = {};
	while ( next( &candidate ) ) {
		if ( candidate.nTick == nTick ) {
			*pFrame = candidate;
			return true;
		}
		if ( candidate.nTick > nTick ) {
			break;
		}
	}
	return false;
}

void SIRInputReplayTape::rewind( void ) noexcept
{
	if ( m_eMode == SIR_INPUT_REPLAY_READ ) {
		m_nOffset = 0;
		m_nLastTick = 0;
	}
}

void SIRInputReplayTape::reset( void ) noexcept
{
	m_pWrite = nullptr;
	m_pRead = nullptr;
	m_nCapacity = 0;
	m_nOffset = 0;
	m_nLastTick = 0;
	m_eMode = SIR_INPUT_REPLAY_IDLE;
}

bool SIRInputReplayTape::finished( void ) const noexcept
{
	return m_eMode == SIR_INPUT_REPLAY_READ && m_nOffset == m_nCapacity;
}

uint32_t SIRInputReplayTape::bytesUsed( void ) const noexcept
{
	return m_nOffset;
}

sirInputReplayMode_t SIRInputReplayTape::mode( void ) const noexcept
{
	return m_eMode;
}

bool SIRInputReplayTape::validateFrame( const sirInputReplayFrame_t& frame ) noexcept
{
	if ( frame.nTick == 0 || frame.nCount > SIR_INPUT_ACTION_CAPACITY ) {
		return false;
	}
	for ( uint32_t i = 0; i < frame.nCount; ++i ) {
		const sirInputActionState_t& state = frame.actions[i];
		if ( !valid( state ) ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( state.nActionId == frame.actions[j].nActionId && state.nUser == frame.actions[j].nUser ) {
				return false;
			}
		}
	}
	return true;
}

bool SIRInputReplayTape::query( const sirInputReplayFrame_t& frame, uint32_t nActionId, uint32_t nUser,
                                sirInputActionState_t *pOut ) noexcept
{
	if ( pOut == nullptr || frame.nCount > SIR_INPUT_ACTION_CAPACITY ) {
		return false;
	}
	for ( uint32_t i = 0; i < frame.nCount; ++i ) {
		if ( frame.actions[i].nActionId == nActionId && frame.actions[i].nUser == nUser ) {
			*pOut = frame.actions[i];
			return true;
		}
	}
	return false;
}
