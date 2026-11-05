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

#include <Core/Platform/SIRPlatformThreadControl.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void sirCheckThreadControl( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		std::fprintf( stderr, "Thread-control test failed: %s\n", pszMessage );
		std::abort();
	}
}

int main( void )
{
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentName( nullptr ) == SIR_THREAD_CONTROL_INVALID_ARGUMENT, "null name" );
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentName( "sir-thread-test" ) == SIR_THREAD_CONTROL_OK, "set name" );
	char szName[256] = {};
	sirCheckThreadControl( SIRPlatformThreadControl::getCurrentName( szName, sizeof( szName ) ) == SIR_THREAD_CONTROL_OK &&
	                           std::strcmp( szName, "sir-thread-test" ) == 0,
	                       "read name" );
	sirThreadAffinity_t affinity{};
	const sirThreadControlResult_t eAffinity = SIRPlatformThreadControl::getCurrentAffinity( &affinity );
	if ( eAffinity == SIR_THREAD_CONTROL_OK ) {
		sirCheckThreadControl( affinity.nMask != 0, "affinity nonzero" );
		sirCheckThreadControl( SIRPlatformThreadControl::setCurrentAffinity( affinity ) == SIR_THREAD_CONTROL_OK, "restore affinity" );
	} else {
		sirCheckThreadControl( eAffinity == SIR_THREAD_CONTROL_UNSUPPORTED, "affinity unsupported contract" );
	}
	sirThreadAffinity_t empty{};
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentAffinity( empty ) == SIR_THREAD_CONTROL_INVALID_ARGUMENT,
	                       "reject zero affinity" );
	sirCheckThreadControl( SIRPlatformThreadControl::setCurrentPriority( ( sirThreadPriority_t )255 ) ==
	                           SIR_THREAD_CONTROL_INVALID_ARGUMENT,
	                       "reject invalid priority" );
	std::atomic<int> nBackgroundResult( -1 );
	std::thread worker( [&nBackgroundResult]() {
		const sirThreadControlResult_t eResult = SIRPlatformThreadControl::setCurrentPriority( SIR_THREAD_PRIORITY_BACKGROUND );
		nBackgroundResult.store( ( int )eResult, std::memory_order_release );
	} );
	worker.join();
	sirCheckThreadControl( nBackgroundResult.load( std::memory_order_acquire ) == SIR_THREAD_CONTROL_OK, "set background worker priority" );
	return 0;
}
