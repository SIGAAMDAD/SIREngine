#include <Core/Platform/SIRPlatformPipe.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <thread>

#define SIR_REQUIRE( condition )                                                                                                           \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static void sirTestPipeLifecycle( void )
{
	SIRPlatformPipe pipe;
	char szBuffer[32] = {};
	uint64_t nTransferred = 123;
	SIR_REQUIRE( pipe.read( szBuffer, sizeof( szBuffer ), &nTransferred ) == SIRPipeResult::SIR_PIPE_CLOSED && nTransferred == 0 );
	SIR_REQUIRE( pipe.write( szBuffer, sizeof( szBuffer ), &nTransferred ) == SIRPipeResult::SIR_PIPE_CLOSED );
	SIR_REQUIRE( pipe.create() == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( pipe.create() == SIRPipeResult::SIR_PIPE_INVALID_ARGUMENT );
	SIR_REQUIRE( pipe.read( nullptr, 1, &nTransferred ) == SIRPipeResult::SIR_PIPE_INVALID_ARGUMENT );
	SIR_REQUIRE( pipe.write( nullptr, 1, &nTransferred ) == SIRPipeResult::SIR_PIPE_INVALID_ARGUMENT );
	SIR_REQUIRE( pipe.read( nullptr, 0, &nTransferred ) == SIRPipeResult::SIR_PIPE_OK && nTransferred == 0 );
	SIR_REQUIRE( pipe.write( nullptr, 0, &nTransferred ) == SIRPipeResult::SIR_PIPE_OK && nTransferred == 0 );
	const char szMessage[] = "GRDG pipe test";
	SIR_REQUIRE( pipe.write( szMessage, sizeof( szMessage ), &nTransferred ) == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( nTransferred == sizeof( szMessage ) );
	SIR_REQUIRE( pipe.closeWrite() == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( pipe.read( szBuffer, sizeof( szBuffer ), &nTransferred ) == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( nTransferred == sizeof( szMessage ) && memcmp( szBuffer, szMessage, nTransferred ) == 0 );
	SIR_REQUIRE( pipe.read( szBuffer, 1, &nTransferred ) == SIRPipeResult::SIR_PIPE_END_OF_STREAM && nTransferred == 0 );
	SIR_REQUIRE( pipe.closeRead() == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( pipe.close() == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( pipe.create() == SIRPipeResult::SIR_PIPE_OK );
	SIR_REQUIRE( pipe.closeRead() == SIRPipeResult::SIR_PIPE_OK );
	// Linux must not terminate the process with SIGPIPE on a broken pipe.
	SIR_REQUIRE( pipe.write( szMessage, sizeof( szMessage ), &nTransferred ) == SIRPipeResult::SIR_PIPE_BROKEN );
	SIR_REQUIRE( nTransferred == 0 );
	SIR_REQUIRE( pipe.close() == SIRPipeResult::SIR_PIPE_OK );
	for ( uint32_t i = 0; i < 512; ++i ) {
		SIR_REQUIRE( pipe.create() == SIRPipeResult::SIR_PIPE_OK );
		SIR_REQUIRE( pipe.close() == SIRPipeResult::SIR_PIPE_OK );
	}
}

static void sirTestConcurrentPipe( void )
{
	SIRPlatformPipe pipe;
	SIR_REQUIRE( pipe.create() == SIRPipeResult::SIR_PIPE_OK );
	constexpr uint32_t nTotal = 1024 * 1024;
	static uint8_t s_szInput[nTotal];
	static uint8_t s_szOutput[nTotal];
	for ( uint32_t i = 0; i < nTotal; ++i ) {
		s_szInput[i] = ( uint8_t )( i * 19 + ( i >> 7 ) );
	}
	bool bWriteOk = false;
	std::thread producer( [&]() {
		uint64_t nWritten = 0;
		bWriteOk = pipe.write( s_szInput, nTotal, &nWritten ) == SIRPipeResult::SIR_PIPE_OK && nWritten == nTotal;
		pipe.closeWrite();
	} );
	uint64_t nConsumed = 0;
	while ( nConsumed < nTotal ) {
		uint64_t nRead = 0;
		SIRPipeResult eResult = pipe.read( s_szOutput + nConsumed, nTotal - nConsumed, &nRead );
		SIR_REQUIRE( eResult == SIRPipeResult::SIR_PIPE_OK && nRead > 0 );
		nConsumed += nRead;
	}
	producer.join();
	SIR_REQUIRE( bWriteOk && nConsumed == nTotal );
	SIR_REQUIRE( memcmp( s_szInput, s_szOutput, nTotal ) == 0 );
	uint64_t nRead = 0;
	SIR_REQUIRE( pipe.read( s_szOutput, 1, &nRead ) == SIRPipeResult::SIR_PIPE_END_OF_STREAM );
	SIR_REQUIRE( pipe.close() == SIRPipeResult::SIR_PIPE_OK );
}

int main( void )
{
	sirTestPipeLifecycle();
	sirTestConcurrentPipe();
	return 0;
}
