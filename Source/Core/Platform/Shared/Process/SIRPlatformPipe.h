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

#ifndef __SIR_PLATFORM_PIPE_H__
#define __SIR_PLATFORM_PIPE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class sirPipeResult : uint8_t
{
	SIR_PIPE_OK = 0,
	SIR_PIPE_END_OF_STREAM = 1,
	SIR_PIPE_CLOSED = 2,
	SIR_PIPE_BROKEN = 3,
	SIR_PIPE_INVALID_ARGUMENT = 4,
	SIR_PIPE_IO_ERROR = 5
};

// Blocking, unidirectional anonymous byte stream (one read end, one write end).
// No shared cursor, allocation, framing, or implicit process-wide signal policy.
// Reads may return fewer bytes than requested, writes loop until completion or error.
// The two ends may be used concurrently from different threads. Close is not safe
// while another thread is using the same endpoint. Both ends are non-inheritable.
class SIRPlatformPipe final
{
public:
	explicit SIRPlatformPipe( void ) noexcept;
	explicit SIRPlatformPipe( const SIRPlatformPipe& ) = delete;
	explicit SIRPlatformPipe( SIRPlatformPipe&& other ) noexcept;
	~SIRPlatformPipe() noexcept;

	SIRPlatformPipe& operator=( SIRPlatformPipe&& ) = delete;
	SIRPlatformPipe& operator=( const SIRPlatformPipe& ) = delete;


	[[nodiscard]] sirPipeResult create( void ) noexcept;
	[[nodiscard]] sirPipeResult read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	[[nodiscard]] sirPipeResult write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	[[nodiscard]] sirPipeResult closeRead( void ) noexcept;
	[[nodiscard]] sirPipeResult closeWrite( void ) noexcept;
	[[nodiscard]] sirPipeResult close( void ) noexcept;
	bool isReadOpen( void ) const noexcept;
	bool isWriteOpen( void ) const noexcept;

private:
	uintptr_t m_nReadHandle;
	uintptr_t m_nWriteHandle;
};

#endif // __SIR_PLATFORM_PIPE_H__
