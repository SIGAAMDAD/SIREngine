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

#ifndef __SIRENGINE_SIRPLATFORMPIPE_H__
#define __SIRENGINE_SIRPLATFORMPIPE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirPipeResult_e : uint8_t
{
	SIR_PIPE_OK = 0,
	SIR_PIPE_END_OF_STREAM = 1,
	SIR_PIPE_CLOSED = 2,
	SIR_PIPE_BROKEN = 3,
	SIR_PIPE_INVALID_ARGUMENT = 4,
	SIR_PIPE_IO_ERROR = 5
} sirPipeResult_t;

// Blocking, unidirectional anonymous byte stream (one read end, one write end).
// No shared cursor, allocation, framing, or implicit process-wide signal policy.
// Reads may return fewer bytes than requested, writes loop until completion or error.
// The two ends may be used concurrently from different threads. Close is not safe
// while another thread is using the same endpoint. Both ends are non-inheritable.
class SIRPlatformPipe final
{
private:
	uintptr_t m_nReadHandle;
	uintptr_t m_nWriteHandle;

public:
	SIRPlatformPipe( void ) noexcept;
	~SIRPlatformPipe( void ) noexcept;
	SIRPlatformPipe( const SIRPlatformPipe & ) = delete;
	SIRPlatformPipe &operator=( const SIRPlatformPipe & ) = delete;
	SIRPlatformPipe( SIRPlatformPipe && ) = delete;
	SIRPlatformPipe &operator=( SIRPlatformPipe && ) = delete;

	sirPipeResult_t create( void ) noexcept;
	sirPipeResult_t read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	sirPipeResult_t write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	sirPipeResult_t closeRead( void ) noexcept;
	sirPipeResult_t closeWrite( void ) noexcept;
	sirPipeResult_t close( void ) noexcept;
	bool isReadOpen( void ) const noexcept;
	bool isWriteOpen( void ) const noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMPIPE_H__
