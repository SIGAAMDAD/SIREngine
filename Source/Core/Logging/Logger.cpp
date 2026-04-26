#include "Logger.h"

using namespace SIREngine::Core::Logger;

CLogCategory::CLogCategory( const eastl::string& categoryName, ELogLevel::Type nDefaultVerbosity )
	: m_nVerbosityLevel( nDefaultVerbosity ),
	m_Name( categoryName )
{
}

CLogCategory::~CLogCategory()
{
}

void CLogCategory::SetVerbosity( ELogLevel::Type nVerbosity )
{
}

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
	if ( s_pLogThread ) {
		s_pLogThread->Join();
		free( s_pLogThread );
	}
}

static const char *GetTime( void )
{
	static char szBuffer[128];
	time_t current;
	struct tm ts;

	current = time( NULL );
	localtime_r( &current, &ts );

	strftime( szBuffer, sizeof( szBuffer ) - 1, "%Y.%m.%d-%H.%M.%S", &ts );
	SIREngine_snprintf_append( szBuffer, sizeof( szBuffer ) - 1, ":%3li", current );

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
	
	LogMessageQueue.push( CMessage( buf ) );
}

void SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) CLogManager::LogCategory( const LogData_t& data, const char *fmt, ... )
{
	if ( ( data.pCategory->GetVerbosity() & ELogLevel::VerbosityMask ) < data.nLevel ) {
		return;
	}

	va_list argptr;
	char msg[8192];
	char buf[20000];
	int len;

	va_start( argptr, fmt );
	SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
	va_end( argptr );

	switch ( ( data.pCategory->GetVerbosity() & ELogLevel::VerbosityMask ) ) {
	case ELogLevel::Error:
		len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
			"\x1B[" TTY_COLOR_RED "mERROR \x1B[" TTY_COLOR_YELLOW "m"
			" %s%s: %s \x1B[0m\n"
			, GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ), data.pCategory->GetCategoryName().c_str(), msg
		);
		break;
	case ELogLevel::Warning:
		len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
			"\x1B[" TTY_COLOR_RED "mWARNING \x1B[" TTY_COLOR_YELLOW "m"
			" %s%s: %s \x1B[0m\n"
			, GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ), data.pCategory->GetCategoryName().c_str(), msg
		);
		break;
	case ELogLevel::Info:
	case ELogLevel::Verbose:
	case ELogLevel::Spam:
		len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
			"%s%s: %s\n"
			, GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ),
			data.pCategory->GetCategoryName().c_str(), msg
		);
		break;
	case ELogLevel::Developer:
		len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
			"\x1B[" TTY_COLOR_CYAN "mDEVELOPER \x1B[" TTY_COLOR_MAGENTA "m"
			" %s%s: %s \x1B[0m\n"
			, GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ), data.pCategory->GetCategoryName().c_str(), msg
		);
		break;
	case ELogLevel::Fatal:
		len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
			"\x1B[" TTY_COLOR_RED "mFATAL ERROR \x1B[" TTY_COLOR_RED "m"
			" %s%s: %s \x1B[0m\n"
			, GetExtraString( data.pFileName, data.pFunction, data.nLineNumber ), data.pCategory->GetCategoryName().c_str(), msg
		);
		Application::Get()->Shutdown();
		break;
	};
	
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

	Application::g_bExitApp.store( true );

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

	while ( 1 ) {
		while ( LogMessageQueue.pop( queueMessage ) ) {
//		    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
		    g_pApplication->FileWrite( queueMessage.szMessage, queueMessage.nStringLength, SIRENGINE_STDOUT_HANDLE );
		    if ( s_pLogFile ) {
		        s_pLogFile->Write( queueMessage.szMessage, queueMessage.nStringLength );
		    }
		}
		
		if ( g_bExitApp.load() ) {
			break;
		} else {
			boost::this_thread::sleep_for( boost::chrono::milliseconds( 10 ) );
		}
	}
}