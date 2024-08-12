#define SIRENGINE_NEW_AND_DELETE_OVERRIDE
#include "Logger.h"
#include <Engine/Core/ThreadSystem/Thread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <EASTL/fixed_vector.h>
#include <Engine/Core/Util.h>
#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/ConsoleManager.h>
#include <boost/lockfree/queue.hpp>
#include <EASTL/chrono.h>
#include <boost/thread.hpp>

#define TTY_COLOR_BLACK "30"
#define TTY_COLOR_RED "31"
#define TTY_COLOR_GREEN "32"
#define TTY_COLOR_YELLOW "33"
#define TTY_COLOR_BLUE "34"
#define TTY_COLOR_MAGENTA "35"
#define TTY_COLOR_CYAN "36"
#define TTY_COLOR_WHITE "0"

using namespace SIREngine;
using namespace SIREngine::Logging;
using namespace SIREngine::Application;

SIREngine::Logging::CLogManager CLogManager::g_Logger;

struct CMessage {
	CMessage( void )
	{ }
	CMessage( const char *pszMessage )
	{
		SIREngine_strncpyz( szMessage, pszMessage, sizeof( szMessage ) );
		nStringLength = ::strlen( szMessage );
	}

	char szMessage[2048];
	uint64_t nStringLength;
};

static CThread *s_pLogThread;
static boost::lockfree::queue<CMessage, boost::lockfree::capacity<1024>> LogMessageQueue;
//static eastl::fixed_vector<CMessage, 256> LogMessageQueue;
static CThreadMutex s_LoggerLock;
static FileSystem::CFileWriter *s_pLogFile;
static CThreadAtomic<bool> s_bExitApp;

// if true, each log will include the call source
bool32 CLogManager::bLogIncludeFileInfo = false;

// if true, each log will include the time
bool32 CLogManager::bLogIncludeTimeInfo = true;

static CVar<uint32_t> e_LogLevel(
	"e.LogLevel",
	ELogLevel::Verbose,
	Cvar_Save,
	"Sets the global log level of the engine.",
	CVG_SYSTEMINFO
);

static CVar<bool32> e_LogToFile(
	"e.LogToFile",
	true,
	Cvar_Save,
	"Outputs log data to a file.",
	CVG_SYSTEMINFO
);

static CVarRef<bool32> e_LogIncludeFileInfo(
	"e.LogIncludeFileInfo",
	CLogManager::bLogIncludeFileInfo,
	Cvar_Default,
	"If true, each log will include the call source",
	CVG_SYSTEMINFO
);

static CVarRef<bool32> e_LogIncludeTimeInfo(
	"e.LogIncludeTimeInfo",
	CLogManager::bLogIncludeTimeInfo,
	Cvar_Default,
	"If true, each log will include the time",
	CVG_SYSTEMINFO
);

CLogManager::CLogManager( void )
{
}

CLogManager::~CLogManager()
{
}

void CLogManager::LaunchLoggingThread( void )
{
	e_LogLevel.Register();
	e_LogToFile.Register();
	e_LogIncludeFileInfo.Register();
	e_LogIncludeTimeInfo.Register();

//	LogMessageQueue.reserve( 1024 );
	s_bExitApp.store( false );

	s_pLogThread = new ( malloc( sizeof( CThread ) ) ) CThread( "LoggingThread" );
	s_pLogThread->Start( CLogManager::LogThread );

	if ( e_LogToFile.GetValue() ) {
		s_pLogFile = g_pFileSystem->OpenFileWriter( "Config/debug.log" );
	}
}

void CLogManager::ShutdownLogger( void )
{
	if ( s_pLogFile ) {
		delete s_pLogFile;
	}
	s_bExitApp.store( true );
	if ( s_pLogThread ) {
		s_pLogThread->Join();
		free( s_pLogThread );
	}
}

static const char *GetTime( void )
{
	static char szBuffer[64];
	time_t current;
	struct tm ts;

	current = time( NULL );
	localtime_r( &current, &ts );

	strftime( szBuffer, sizeof( szBuffer ) - 1, "%H:%M:%S", &ts );

	return szBuffer;
}

void SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) CLogManager::LogInfo( const LogData_t& data,
	const char *fmt, ... )
{
	if ( e_LogLevel.GetValue() < ELogLevel::Info ) {
		return;
	}

	va_list argptr;
	char msg[8192];
	char buf[20000];
	int len;

	va_start( argptr, fmt );
	SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
	va_end( argptr );

	len = SIREngine_snprintf( buf, sizeof( buf ) - 1, "%s %s\n", GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ),
		msg );
	
//    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
//    g_pApplication->FileWrite( buf, len, SIRENGINE_STDOUT_HANDLE );
//    if ( s_pLogFile ) {
//        s_pLogFile->Write( buf, len );
//    }
	LogMessageQueue.push( CMessage( buf ) );
}

void SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) CLogManager::LogWarning( const LogData_t& data,
	const char *fmt, ... )
{
	if ( e_LogLevel.GetValue() < ELogLevel::Warning ) {
		return;
	}

	va_list argptr;
	char msg[8192];
	char buf[20000];
	int len;

	va_start( argptr, fmt );
	SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
	va_end( argptr );

	len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
		"\x1B[" TTY_COLOR_RED "m WARNING \x1B[" TTY_COLOR_YELLOW "m"
		" %s %s \x1B[0m\n", GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ), msg );
	
//    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
//    g_pApplication->FileWrite( buf, len, SIRENGINE_STDOUT_HANDLE );
//    if ( s_pLogFile ) {
//        s_pLogFile->Write( buf, len );
//    }
	LogMessageQueue.push( CMessage( buf ) );
}

void SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) CLogManager::SendNotification( const LogData_t& data,
	const char *fmt, ... )
{
	if ( e_LogLevel.GetValue() < ELogLevel::Spam ) {
		return;
	}

	va_list argptr;
	char msg[8192];
	char buf[20000];
	int len;

	va_start( argptr, fmt );
	SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
	va_end( argptr );

	len = SIREngine_snprintf( buf, sizeof( buf ) - 1, "[NOTIFICATION] %s %s\n",
		GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ),
		msg );
	
//    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
//    g_pApplication->FileWrite( buf, len, SIRENGINE_STDOUT_HANDLE );
//    if ( s_pLogFile ) {
//        s_pLogFile->Write( buf, len );
//    }
	LogMessageQueue.push( CMessage( buf ) );
}

void SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) CLogManager::LogError( const LogData_t& data, const char *fmt, ... )
{
	if ( e_LogLevel.GetValue() < ELogLevel::Error ) {
		return;
	}

	va_list argptr;
	char msg[8192];

	va_start( argptr, fmt );
	SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
	va_end( argptr );

	s_bExitApp.store( true );

	g_pApplication->Error( msg );
}

const char *CLogManager::GetExtraString( const char *pFileName, const char *pFunction, uint64_t nLineNumber )
{
	static char szExtraString[1024];
	
	szExtraString[0] = '\0';
	if ( bLogIncludeFileInfo ) {
		SIREngine_snprintf_append( szExtraString, sizeof( szExtraString ) - 1, "(%s:%lu)", pFileName, nLineNumber );
	}
	if ( bLogIncludeTimeInfo ) {
		SIREngine_snprintf_append( szExtraString, sizeof( szExtraString ) - 1, "[%s]", GetTime() );
	}

	return szExtraString;
}

void CLogManager::LogThread( void )
{
	CThreadMutex lock;
	CMessage queueMessage;

	while ( !s_bExitApp.load() ) {
		if ( LogMessageQueue.pop( queueMessage ) ) {
//		    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
		    g_pApplication->FileWrite( queueMessage.szMessage, queueMessage.nStringLength, SIRENGINE_STDOUT_HANDLE );
		    if ( s_pLogFile ) {
		        s_pLogFile->Write( queueMessage.szMessage, queueMessage.nStringLength );
		    }
		} else {
			boost::this_thread::sleep_for( boost::chrono::milliseconds( 100 ) );
		}
	}

	// pump out any leftovers
	while ( LogMessageQueue.pop( queueMessage ) ) {
	    g_pApplication->FileWrite( queueMessage.szMessage, queueMessage.nStringLength, SIRENGINE_STDOUT_HANDLE );
	    if ( s_pLogFile ) {
	        s_pLogFile->Write( queueMessage.szMessage, queueMessage.nStringLength );
	    }
	}
}