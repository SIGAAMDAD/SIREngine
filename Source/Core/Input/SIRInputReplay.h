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

#ifndef __SIRENGINE_SIRINPUTREPLAY_H__
#define __SIRENGINE_SIRINPUTREPLAY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Input/SIRInputActions.h>

constexpr uint32_t SIR_INPUT_REPLAY_MAGIC = UINT32_C( 0x50524953 ); // SIRP in little-endian.
constexpr uint32_t SIR_INPUT_REPLAY_HEADER_BYTES = 32;
constexpr uint32_t SIR_INPUT_REPLAY_RECORD_BYTES = 20;
constexpr uint32_t SIR_INPUT_REPLAY_MAX_FRAME_BYTES =
    SIR_INPUT_REPLAY_HEADER_BYTES + SIR_INPUT_ACTION_CAPACITY * SIR_INPUT_REPLAY_RECORD_BYTES;

typedef struct sirInputReplayKey_s
{
	uint32_t nActionId;
	uint32_t nUser;
} sirInputReplayKey_t;

typedef struct sirInputReplayFrame_s
{
	uint64_t nTick;
	uint32_t nCount;
	sirInputActionState_t actions[SIR_INPUT_ACTION_CAPACITY];
} sirInputReplayFrame_t;

typedef enum sirInputReplayMode_e : uint8_t
{
	SIR_INPUT_REPLAY_IDLE = 0,
	SIR_INPUT_REPLAY_RECORD = 1,
	SIR_INPUT_REPLAY_READ = 2
} sirInputReplayMode_t;

// Engine-owned action-tick tape. The caller owns the byte buffer and keeps
// it alive while recording or reading. No allocations or native handles.
// Text and IME composition are never serialized into this format.
class SIRInputReplayTape
{
public:
	SIRInputReplayTape( void ) noexcept;
	bool beginRecord( uint8_t *pBuffer, uint32_t nCapacity ) noexcept;
	bool append( uint64_t nTick, const SIRInputActions& actions, const sirInputReplayKey_t *pKeys, uint32_t nCount ) noexcept;
	bool beginRead( const uint8_t *pBuffer, uint32_t nBytes ) noexcept;
	bool next( sirInputReplayFrame_t *pFrame ) noexcept;
	bool seek( uint64_t nTick, sirInputReplayFrame_t *pFrame ) noexcept;
	void rewind( void ) noexcept;
	void reset( void ) noexcept;
	bool finished( void ) const noexcept;
	uint32_t bytesUsed( void ) const noexcept;
	sirInputReplayMode_t mode( void ) const noexcept;
	static bool validateFrame( const sirInputReplayFrame_t& frame ) noexcept;
	static bool query( const sirInputReplayFrame_t& frame, uint32_t nActionId, uint32_t nUser, sirInputActionState_t *pOut ) noexcept;

private:
	static void write16( uint8_t *pBytes, uint16_t nValue ) noexcept;
	static void write32( uint8_t *pBytes, uint32_t nValue ) noexcept;
	static void write64( uint8_t *pBytes, uint64_t nValue ) noexcept;
	static uint16_t read16( const uint8_t *pBytes ) noexcept;
	static uint32_t read32( const uint8_t *pBytes ) noexcept;
	static uint64_t read64( const uint8_t *pBytes ) noexcept;
	static uint32_t checksum( const uint8_t *pBytes, uint32_t nLength ) noexcept;
	static bool valid( const sirInputActionState_t& state ) noexcept;

	uint8_t *m_pWrite;
	const uint8_t *m_pRead;
	uint32_t m_nCapacity;
	uint32_t m_nOffset;
	uint64_t m_nLastTick;
	sirInputReplayMode_t m_eMode;
};

#endif // __SIRENGINE_SIRINPUTREPLAY_H__
