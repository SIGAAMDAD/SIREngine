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

#include <Core/Diagnostics/Assertions/SIRAssert.h>
#include <Core/Diagnostics/SIREmergencyDiagnostics.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformDebug.h>

#if defined( __linux__ )
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define SIR_TEST_REQUIRE( condition ) \
	do { \
		if ( !( condition ) ) { \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition ); \
			exit( EXIT_FAILURE ); \
		} \
	} while ( false )

class SIRAssertTests final
{
public:
	explicit SIRAssertTests( void ) = delete;
	explicit SIRAssertTests( const SIRAssertTests& ) = delete;
	explicit SIRAssertTests( SIRAssertTests&& ) = delete;

	SIRAssertTests& operator=( const SIRAssertTests& ) = delete;
	SIRAssertTests& operator=( SIRAssertTests&& ) = delete;

	static void testEvaluation( void ) noexcept;
	static void testSourceLocation( void ) noexcept;
	static void testClaim( void ) noexcept;
#if defined( __linux__ )
	static void testNativeWriter( void ) noexcept;
	static void testSubprocesses( void ) noexcept;
#endif

private:
	static bool ensureOnce( int32_t& nConditions, int32_t& nMessages, bool bPass ) noexcept;
#if defined( __linux__ )
	typedef struct sirClaimWork_s
	{
		std::atomic<uint32_t> nReady{ 0 };
		std::atomic<uint32_t> nStarted{ 0 };
		std::atomic<uint32_t> nClaim{ 0 };
		std::atomic<uint32_t> nWinners{ 0 };
	} sirClaimWork_t;

	static void *runClaim( void *pContext ) noexcept;
	static void runChild( const char *pszCase, bool bFatal, const char *pszExpected ) noexcept;
	static void executeCase( const char *pszCase ) noexcept;
	static const char *failRecursively( void ) noexcept;
#endif
};

bool SIRAssertTests::ensureOnce( int32_t& nConditions, int32_t& nMessages, bool bPass ) noexcept
{
	return SIR_ENSURE_MSG( ( ++nConditions, bPass ), "once %d", ++nMessages );
}

void SIRAssertTests::testEvaluation( void ) noexcept
{
	int32_t nConditions = 0;
	int32_t nMessages = 0;
	SIR_ASSERT( ++nConditions != 0 );
	SIR_ASSERT_MSG( ++nConditions != 0, "unused %d", ++nMessages );
	SIR_TEST_REQUIRE( nConditions == ( SIR_DIAGNOSTIC_ASSERT_ENABLED ? 2 : 0 ) );
	SIR_TEST_REQUIRE( nMessages == 0 );
	nConditions = 0;
	SIR_VERIFY( ++nConditions == 1 );
	SIR_VERIFY_MSG( ++nConditions == 2, "unused %d", ++nMessages );
	SIR_CHECK( ++nConditions == 3 );
	SIR_CHECK_MSG( ++nConditions == 4, "unused %d", ++nMessages );
	SIR_TEST_REQUIRE( nConditions == 4 && nMessages == 0 );

	nConditions = 0;
	SIR_TEST_REQUIRE( ensureOnce( nConditions, nMessages, true ) );
	SIR_TEST_REQUIRE( !ensureOnce( nConditions, nMessages, false ) );
	SIR_TEST_REQUIRE( !ensureOnce( nConditions, nMessages, false ) );
	SIR_TEST_REQUIRE( ensureOnce( nConditions, nMessages, true ) );
	SIR_TEST_REQUIRE( nConditions == 4 && nMessages == 1 );
	for ( uint32_t i = 0; i < 2; ++i ) {
		SIR_TEST_REQUIRE( !SIR_ENSURE_ALWAYS_MSG( ++nConditions == 0, "always %d", ++nMessages ) );
	}
	SIR_TEST_REQUIRE( nConditions == 6 && nMessages == 3 );
	SIR_TEST_REQUIRE( SIR_ENSURE( true ) && SIR_ENSURE_ALWAYS( true ) );
	static_assert( std::is_same_v<decltype( SIR_ENSURE( true ) ), bool> );
}

void SIRAssertTests::testSourceLocation( void ) noexcept
{
	const uint32_t nExpectedLine = __LINE__ + 1;
	const sirDiagnosticSourceLocation_t location = SIR_DIAGNOSTIC_SOURCE_LOCATION;
	SIR_TEST_REQUIRE( strcmp( location.pszFile, "Tests/Unit/SIRAssertTests.cpp" ) == 0 );
	SIR_TEST_REQUIRE( strcmp( location.pszFunction, "testSourceLocation" ) == 0 );
	SIR_TEST_REQUIRE( location.nLine == nExpectedLine );
}

void SIRAssertTests::testClaim( void ) noexcept
{
	std::atomic<uint32_t> nClaim{ 0 };
	SIR_TEST_REQUIRE( SIRAssert::claimEnsure( nClaim ) );
	SIR_TEST_REQUIRE( !SIRAssert::claimEnsure( nClaim ) );
#if defined( __linux__ )
	sirClaimWork_t work{};
	pthread_t szWorkers[16]{};
	for ( uint32_t i = 0; i < 16; ++i ) {
		SIR_TEST_REQUIRE( pthread_create( &szWorkers[i], nullptr, runClaim, &work ) == 0 );
	}
	while ( work.nReady.load( std::memory_order_relaxed ) != 16 ) {
		sched_yield();
	}
	work.nStarted.store( 1, std::memory_order_relaxed );
	for ( uint32_t i = 0; i < 16; ++i ) {
		SIR_TEST_REQUIRE( pthread_join( szWorkers[i], nullptr ) == 0 );
	}
	SIR_TEST_REQUIRE( work.nWinners.load( std::memory_order_relaxed ) == 1 );
#endif
}

#if defined( __linux__ )
void *SIRAssertTests::runClaim( void *pContext ) noexcept
{
	sirClaimWork_t& work = *static_cast<sirClaimWork_t *>( pContext );
	work.nReady.fetch_add( 1, std::memory_order_relaxed );
	while ( work.nStarted.load( std::memory_order_relaxed ) == 0 ) {
		sched_yield();
	}
	if ( SIRAssert::claimEnsure( work.nClaim ) ) {
		work.nWinners.fetch_add( 1, std::memory_order_relaxed );
	}
	return nullptr;
}

void SIRAssertTests::testNativeWriter( void ) noexcept
{
	const char szBytes[] = { 'A', '\0', 'B' };
	uint32_t nWritten = 0;
	SIR_TEST_REQUIRE( SIRPlatformDebug::writeEmergency( { nullptr, 1 }, &nWritten ) == sirDiagnosticResult::SIR_DIAGNOSTIC_INVALID_ARGUMENT );
	SIR_TEST_REQUIRE( nWritten == 0 );

	int32_t szPipe[2]{};
	SIR_TEST_REQUIRE( pipe( szPipe ) == 0 );
	const int32_t nStderr = dup( STDERR_FILENO );
	SIR_TEST_REQUIRE( nStderr >= 0 );
	SIR_TEST_REQUIRE( dup2( szPipe[1], STDERR_FILENO ) == STDERR_FILENO );
	SIR_TEST_REQUIRE( close( szPipe[1] ) == 0 );
	const sirDiagnosticResult eWritten = SIRPlatformDebug::writeEmergency( { szBytes, sizeof( szBytes ) }, &nWritten );
	char szReceived[3]{};
	const ssize_t nReceived = read( szPipe[0], szReceived, sizeof( szReceived ) );
	SIR_TEST_REQUIRE( close( szPipe[0] ) == 0 );
	const sirDiagnosticResult eBrokenPipe = SIRPlatformDebug::writeEmergency( { szBytes, sizeof( szBytes ) }, nullptr );
	SIR_TEST_REQUIRE( dup2( nStderr, STDERR_FILENO ) == STDERR_FILENO );
	SIR_TEST_REQUIRE( close( nStderr ) == 0 );
	SIR_TEST_REQUIRE( eWritten == sirDiagnosticResult::SIR_DIAGNOSTIC_OK && nWritten == sizeof( szBytes ) );
	SIR_TEST_REQUIRE( nReceived == sizeof( szBytes ) && memcmp( szBytes, szReceived, sizeof( szBytes ) ) == 0 );
	SIR_TEST_REQUIRE( eBrokenPipe == sirDiagnosticResult::SIR_DIAGNOSTIC_OS_ERROR );
}

const char *SIRAssertTests::failRecursively( void ) noexcept
{
	SIR_CHECK_MSG( false, "nested failure must not write" );
	return "";
}

void SIRAssertTests::executeCase( const char *pszCase ) noexcept
{
	if ( strcmp( pszCase, "assert" ) == 0 ) {
		int32_t nConditions = 0;
		int32_t nArguments = 0;
		SIR_ASSERT_MSG( ++nConditions == 0, "assert arg %d", ++nArguments );
		SIR_TEST_REQUIRE( nConditions == 0 && nArguments == 0 );
	} else if ( strcmp( pszCase, "verify" ) == 0 ) {
		int32_t nConditions = 0;
		int32_t nArguments = 0;
		SIR_VERIFY_MSG( ++nConditions == 0, "verify arg %d", ++nArguments );
		SIR_TEST_REQUIRE( nConditions == 1 && nArguments == 0 );
	} else if ( strcmp( pszCase, "check" ) == 0 ) {
		SIR_CHECK_MSG( false, "check %s %" PRIu64 " %08x %% %.2f", "value", UINT64_C( 4294967297 ), 42u, 1.25 );
	} else if ( strcmp( pszCase, "unreachable" ) == 0 ) {
		SIR_UNREACHABLE();
	} else if ( strcmp( pszCase, "unimplemented" ) == 0 ) {
		SIR_UNIMPLEMENTED();
	} else if ( strcmp( pszCase, "recursive" ) == 0 ) {
		SIR_CHECK_MSG( false, "outer %s", failRecursively() );
	} else if ( strcmp( pszCase, "record" ) == 0 ) {
		sirEmergencyRecord_t record{};
		memcpy( record.szMessage, "standalone record", sizeof( "standalone record" ) );
		SIR_TEST_REQUIRE( SIREmergencyDiagnostics::record( record ) );
	} else {
		SIR_TEST_REQUIRE( false );
	}
}

void SIRAssertTests::runChild( const char *pszCase, bool bFatal, const char *pszExpected ) noexcept
{
	int32_t szPipe[2]{};
	SIR_TEST_REQUIRE( pipe( szPipe ) == 0 );
	const pid_t nChild = fork();
	SIR_TEST_REQUIRE( nChild >= 0 );
	if ( nChild == 0 ) {
		alarm( 5 );
		SIR_TEST_REQUIRE( close( szPipe[0] ) == 0 );
		SIR_TEST_REQUIRE( dup2( szPipe[1], STDERR_FILENO ) == STDERR_FILENO );
		SIR_TEST_REQUIRE( close( szPipe[1] ) == 0 );
		executeCase( pszCase );
		_exit( 0 );
	}

	SIR_TEST_REQUIRE( close( szPipe[1] ) == 0 );
	char szOutput[4096]{};
	uint32_t nUsed = 0;
	while ( nUsed < sizeof( szOutput ) - 1 ) {
		const ssize_t nRead = read( szPipe[0], szOutput + nUsed, sizeof( szOutput ) - 1 - nUsed );
		if ( nRead < 0 && errno == EINTR ) {
			continue;
		}
		SIR_TEST_REQUIRE( nRead >= 0 );
		if ( nRead == 0 ) {
			break;
		}
		nUsed += static_cast<uint32_t>( nRead );
	}
	SIR_TEST_REQUIRE( close( szPipe[0] ) == 0 );
	int32_t nStatus = 0;
	SIR_TEST_REQUIRE( waitpid( nChild, &nStatus, 0 ) == nChild );
	SIR_TEST_REQUIRE( WIFEXITED( nStatus ) && WEXITSTATUS( nStatus ) == ( bFatal ? 134 : 0 ) );
	if ( pszExpected == nullptr ) {
		SIR_TEST_REQUIRE( nUsed == 0 );
	} else {
		SIR_TEST_REQUIRE( strstr( szOutput, pszExpected ) != nullptr );
	}
}

void SIRAssertTests::testSubprocesses( void ) noexcept
{
	const bool bAssertions = SIR_DIAGNOSTIC_ASSERT_ENABLED == 1;
	runChild( "assert", bAssertions, bAssertions ? "assert arg 1" : nullptr );
	runChild( "verify", bAssertions, bAssertions ? "verify arg 1" : nullptr );
	runChild( "check", true, "check value 4294967297 0000002a % 1.25" );
	runChild( "unreachable", true, "unreachable" );
	runChild( "unimplemented", true, "unimplemented" );
	runChild( "recursive", true, nullptr );
	runChild( "record", false, "standalone record" );
}
#endif

int main( void )
{
	SIRAssertTests::testEvaluation();
	SIRAssertTests::testSourceLocation();
	SIRAssertTests::testClaim();
#if defined( __linux__ )
	SIRAssertTests::testNativeWriter();
	SIRAssertTests::testSubprocesses();
#endif
	return 0;
}
