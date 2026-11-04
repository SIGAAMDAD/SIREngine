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

#ifndef __SIRENGINE_SIRSEMAPHORE_H__
#define __SIRENGINE_SIRSEMAPHORE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

#if defined( __linux__ )
#include <semaphore.h>
#elif !defined( _WIN32 )
#error "SIRSemaphore requires a Windows or Linux backend"
#endif

typedef enum sirSemaphoreResult_e : uint8_t
{
	SIR_SEMAPHORE_OK = 0,
	SIR_SEMAPHORE_WOULD_BLOCK = 1,
	SIR_SEMAPHORE_TIMED_OUT = 2,
	SIR_SEMAPHORE_CLOSED = 3,
	SIR_SEMAPHORE_LIMIT_REACHED = 4,
	SIR_SEMAPHORE_INVALID_ARGUMENT = 5,
	SIR_SEMAPHORE_OS_ERROR = 6
} sirSemaphoreResult_t;

// Thread-shared, process-local counting semaphore.
// create()/close() are lifecycle operations and must not race with any wait
// or post call. All waiters must be finished before close() or destruction.
// A successful post() publishes one permit. A successful wait consumes one.
// No engine heap allocations; kernel state is owned by the OS backend.
class SIRSemaphore final
{
private:
#if defined( _WIN32 )
	void *m_pHandle;
#else
	sem_t m_Semaphore;
	bool m_bOpen;
#endif

public:
	SIRSemaphore( void ) noexcept;
	~SIRSemaphore( void ) noexcept;
	SIRSemaphore( const SIRSemaphore & ) = delete;
	SIRSemaphore &operator=( const SIRSemaphore & ) = delete;
	SIRSemaphore( SIRSemaphore && ) = delete;
	SIRSemaphore &operator=( SIRSemaphore && ) = delete;

	sirSemaphoreResult_t create( uint32_t nInitialCount = 0 ) noexcept;
	sirSemaphoreResult_t post( void ) noexcept;
	sirSemaphoreResult_t wait( void ) noexcept;
	sirSemaphoreResult_t tryWait( void ) noexcept;
	sirSemaphoreResult_t waitFor( uint32_t nMilliseconds ) noexcept;
	sirSemaphoreResult_t close( void ) noexcept;
	bool isOpen( void ) const noexcept;
};

#endif // __SIRENGINE_SIRSEMAPHORE_H__
