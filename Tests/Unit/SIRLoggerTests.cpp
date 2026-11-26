#include <Core/Diagnostics/Logging/SIRLoggerSystem.h>
#include <Core/Console/SIRConsoleLogSink.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check( bool bValue, const char *pszMessage ) { if ( !bValue ) { std::fprintf( stderr, "%s\n", pszMessage ); std::abort(); } }
int main( void )
{
	sirLoggerSystemConfig_t config{}; config.nQueueCapacity = 8; config.bEnableConsole = false;
	SIRLoggerSystem logger( config ); SIRConsoleLogSink console;
	check( logger.initialize() == SIRSystemResult::SIR_SYSTEM_OK, "logger initialize" );
	check( logger.attachSink( logger ) == sirLogSinkResult::SIR_LOG_SINK_INVALID, "reject self attach" );
	check( console.initialize( 8 ), "console initialize" );
	check( logger.attachSink( console ) == sirLogSinkResult::SIR_LOG_SINK_OK, "attach console" );
	sirLogRecord_t record{}; record.eSeverity = sirLogSeverity::SIR_LOG_INFO; std::snprintf( record.szMessage, sizeof( record.szMessage ), "hello" ); record.nMessageBytes = 5;
	check( logger.write( record ) == sirLogSinkResult::SIR_LOG_SINK_OK, "admit record" );
	sirLogRecord_t received{}; for ( uint32_t i = 0; i < 100 && console.drain( &received, 1 ) == 0; ++i ) SIRThread::yieldThread();
	check( std::strcmp( received.szMessage, "hello" ) == 0, "deliver record" );
	check( logger.detachSink( console ) == sirLogSinkResult::SIR_LOG_SINK_OK, "detach console" );
	check( logger.shutdown() == SIRSystemResult::SIR_SYSTEM_OK, "logger shutdown" ); return 0;
}
