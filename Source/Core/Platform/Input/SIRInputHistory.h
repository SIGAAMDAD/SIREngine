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

#ifndef __SIRENGINE_SIRINPUTHISTORY_H__
#define __SIRENGINE_SIRINPUTHISTORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputReplay.h>

// Owner-thread rollback window; storage belongs to the caller/allocator.
// Tick numbers must be contiguous, so missing input cannot be hidden.
class SIRInputHistory
{
public:
	SIRInputHistory( void ) noexcept;
	SIRInputHistory( const SIRInputHistory& ) = delete;

	SIRInputHistory& operator=( const SIRInputHistory& ) = delete;

	uint32_t count( void ) const noexcept;
	uint32_t capacity( void ) const noexcept;
	uint64_t oldestTick( void ) const noexcept;
	uint64_t newestTick( void ) const noexcept;

	bool initialize( sirInputReplayFrame_t *pStorage, uint32_t nCapacity ) noexcept;
	bool append( const sirInputReplayFrame_t& frame ) noexcept;
	bool read( uint64_t nTick, sirInputReplayFrame_t *pOut ) const noexcept;
	bool rewindFrom( uint64_t nTick ) noexcept;
	bool replaceFrom( const sirInputReplayFrame_t& frame ) noexcept;
	bool digest( uint64_t nTick, uint64_t *pOut ) const noexcept;
	void clear( void ) noexcept;
	static bool hashFrame( const sirInputReplayFrame_t& frame, uint64_t *pOut ) noexcept;

protected:

private:
	sirInputReplayFrame_t *m_pStorage;
	uint32_t m_nCapacity;
	uint32_t m_nCount;
	uint32_t m_nHead;
};

#endif // __SIRENGINE_SIRINPUTHISTORY_H__
