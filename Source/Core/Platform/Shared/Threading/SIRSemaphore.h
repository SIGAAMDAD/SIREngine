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

#ifndef __SIR_SEMAPHORE_H__
#define __SIR_SEMAPHORE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

#include <Core/Platform/Shared/Storage/SIRPlatformNativeStorage.h>

enum class sirSemaphoreResult : uint8_t {
	SIR_SEMAPHORE_OK = 0,
	SIR_SEMAPHORE_WOULD_BLOCK = 1,
	SIR_SEMAPHORE_TIMED_OUT = 2,
	SIR_SEMAPHORE_CLOSED = 3,
	SIR_SEMAPHORE_LIMIT_REACHED = 4,
	SIR_SEMAPHORE_INVALID_ARGUMENT = 5,
	SIR_SEMAPHORE_OS_ERROR = 6
};

// Thread-shared, process-local counting semaphore.
// create()/close() are lifecycle operations and must not race with any wait
// or post call. All waiters must be finished before close() or destruction.
// A successful post() publishes one permit. A successful wait consumes one.
// No engine heap allocations; kernel state is owned by the OS backend.
class SIRSemaphore final
{
public:
	explicit SIRSemaphore( void ) noexcept;
	explicit SIRSemaphore( const SIRSemaphore& ) = delete;
	explicit SIRSemaphore( SIRSemaphore&& ) = delete;
	~SIRSemaphore() noexcept;

	SIRSemaphore& operator=( const SIRSemaphore& ) = delete;
	SIRSemaphore& operator=( SIRSemaphore&& ) = delete;


	[[nodiscard]] sirSemaphoreResult create( uint32_t nInitialCount = 0 ) noexcept;
	[[nodiscard]] sirSemaphoreResult post( void ) noexcept;
	[[nodiscard]] sirSemaphoreResult wait( void ) noexcept;
	[[nodiscard]] sirSemaphoreResult tryWait( void ) noexcept;
	[[nodiscard]] sirSemaphoreResult waitFor( uint32_t nMilliseconds ) noexcept;
	[[nodiscard]] sirSemaphoreResult close( void ) noexcept;
	bool isOpen( void ) const noexcept;

private:
#if defined( _WIN32 )
	void *m_pHandle;
#else
	sirPlatformNativeStorage_t m_Semaphore;
	bool m_bOpen;
#endif
};

#endif // __SIR_SEMAPHORE_H__
