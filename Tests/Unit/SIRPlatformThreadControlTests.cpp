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

#include <Core/Platform/Shared/Threading/SIRPlatformThreadControl.h>

#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void sirCheckThreadControl( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		std::fprintf( stderr, "Thread-control test failed: %s\n", pszMessage );
		std::abort();
	}
}

typedef struct sirPlatformThreadWorkerContext_s
{
	uint64_t nThreadId;
	sirThreadControlResult ePriority;
} sirPlatformThreadWorkerContext_t;

static void sirPlatformThreadWorker( void *pUserData )
{
	sirPlatformThreadWorkerContext_t *pContext = ( sirPlatformThreadWorkerContext_t * )pUserData;
	pContext->nThreadId = SIRPlatformThreadControl::getCurrentThreadId();
	pContext->ePriority = SIRPlatformThreadControl::setCurrentPriority( sirThreadPriority::SIR_THREAD_PRIORITY_BACKGROUND );
}

int main( void )
{
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentName( nullptr ) == sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT, "null name" );
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentName( "sir-thread-test" ) == sirThreadControlResult::SIR_THREAD_CONTROL_OK, "set name" );
	char szName[256] = {};
	sirCheckThreadControl( SIRPlatformThreadControl::getCurrentName( szName, sizeof( szName ) ) == sirThreadControlResult::SIR_THREAD_CONTROL_OK &&
	                           std::strcmp( szName, "sir-thread-test" ) == 0,
	                       "read name" );
	sirThreadAffinity_t affinity{};
	const sirThreadControlResult eAffinity = SIRPlatformThreadControl::getCurrentAffinity( &affinity );
	if ( eAffinity == sirThreadControlResult::SIR_THREAD_CONTROL_OK ) {
		sirCheckThreadControl( affinity.nMask != 0, "affinity nonzero" );
		sirCheckThreadControl( SIRPlatformThreadControl::setCurrentAffinity( affinity ) == sirThreadControlResult::SIR_THREAD_CONTROL_OK, "restore affinity" );
	} else {
		sirCheckThreadControl( eAffinity == sirThreadControlResult::SIR_THREAD_CONTROL_UNSUPPORTED, "affinity unsupported contract" );
	}
	sirThreadAffinity_t empty{};
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentAffinity( empty ) == sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT,
	                       "reject zero affinity" );
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentPriority( static_cast<sirThreadPriority>( 255 ) ) ==
	                           sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT,
	                       "reject invalid priority" );
	const uint64_t nMainThreadId = SIRPlatformThreadControl::getCurrentThreadId();
	sirCheckThreadControl( nMainThreadId != 0 && nMainThreadId == SIRPlatformThreadControl::getCurrentThreadId(),
	                       "stable nonzero thread identity" );
	sirPlatformThreadWorkerContext_t workerContext = {};
	SIRThread worker;
	sirCheckThreadControl( worker.start( sirPlatformThreadWorker, &workerContext ) && worker.join(), "start/join engine thread" );
	sirCheckThreadControl( workerContext.ePriority == sirThreadControlResult::SIR_THREAD_CONTROL_OK, "set background worker priority" );
	sirCheckThreadControl( workerContext.nThreadId != 0 && workerContext.nThreadId != nMainThreadId, "distinct worker thread identity" );
	return 0;
}
