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

#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <atomic>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define SIR_THREAD_CHECK( condition )                                                                                                      \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static std::atomic<uint32_t> s_nCreateCalls{ 0 };
static std::atomic<uint32_t> s_nFailOrdinal{ 0 };

extern "C" int __real_pthread_create( pthread_t *pThread, const pthread_attr_t *pAttrs, void *( *pfnStart )( void * ), void *pContext );

extern "C" int __wrap_pthread_create( pthread_t *pThread, const pthread_attr_t *pAttrs, void *( *pfnStart )( void * ), void *pContext )
{
	const uint32_t nCall = s_nCreateCalls.fetch_add( 1, std::memory_order_relaxed ) + 1;
	if ( nCall == s_nFailOrdinal.load( std::memory_order_relaxed ) ) {
		return EAGAIN;
	}
	return __real_pthread_create( pThread, pAttrs, pfnStart, pContext );
}

static sirTaskResult_t runCounter( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1, std::memory_order_relaxed );
	return SIR_TASK_RESULT_SUCCESS;
}

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIR_THREAD_CHECK( nPage != 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t memoryConfig = { nPage * 128, nPage * 32, nPage * 8, 8 };
	SIR_THREAD_CHECK( memory.init( memoryConfig ) );
	const sirTaskSchedulerConfig_t taskConfig = { 8, 2, 16 };
	for ( uint32_t nFail = 1; nFail <= 2; ++nFail ) {
		SIRTaskScheduler scheduler;
		s_nCreateCalls.store( 0, std::memory_order_relaxed );
		s_nFailOrdinal.store( nFail, std::memory_order_relaxed );
		SIR_THREAD_CHECK( !scheduler.init( &memory, taskConfig ) );
		SIR_THREAD_CHECK( s_nCreateCalls.load( std::memory_order_relaxed ) == nFail );
		SIR_THREAD_CHECK( !scheduler.shutdown() );
	}
	SIRTaskScheduler scheduler;
	s_nCreateCalls.store( 0, std::memory_order_relaxed );
	s_nFailOrdinal.store( 0, std::memory_order_relaxed );
	SIR_THREAD_CHECK( scheduler.init( &memory, taskConfig ) );
	SIR_THREAD_CHECK( s_nCreateCalls.load( std::memory_order_relaxed ) == taskConfig.nWorkerCount );
	std::atomic<uint32_t> nCompleted( 0 );
	sirTaskHandle_t task = {};
	SIR_THREAD_CHECK( scheduler.submit({ runCounter, &nCompleted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &task) );
	SIR_THREAD_CHECK( scheduler.waitTask( task ) );
	SIR_THREAD_CHECK( nCompleted.load( std::memory_order_relaxed ) == 1 );
	SIR_THREAD_CHECK( scheduler.releaseTask( task ) );
	SIR_THREAD_CHECK( scheduler.shutdown() );
	// After shutdown, a released handle must never be resurrected and
	// no late producer or provider acknowledgement may execute a callback.
	sirTaskHandle_t rejected = { 17, 42 };
	SIR_THREAD_CHECK( !scheduler.submit({ runCounter, &nCompleted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &rejected) );
	SIR_THREAD_CHECK( rejected.nSlot == UINT32_MAX && rejected.nGeneration == 0 );
	SIR_THREAD_CHECK( !scheduler.completeProvider( task, SIR_TASK_RESULT_SUCCESS ) );
	SIR_THREAD_CHECK( scheduler.getState( task ) == SIR_TASK_INVALID );
	SIR_THREAD_CHECK( nCompleted.load( std::memory_order_relaxed ) == 1 );
	memory.shutdown();
	puts( "PASS: task scheduler recovers from first and second native worker creation failures" );
	return 0;
}
