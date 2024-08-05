#include "Logger.h"
#include <unistd.h>
#include <time.h>
#include <Engine/Core/ThreadSystem/Thread.h>

#define TTY_COLOR_BLACK "30"
#define TTY_COLOR_RED "31"
#define TTY_COLOR_GREEN "32"
#define TTY_COLOR_YELLOW "33"
#define TTY_COLOR_BLUE "34"
#define TTY_COLOR_MAGENTA "35"
#define TTY_COLOR_CYAN "36"
#define TTY_COLOR_WHITE "0"

CLogManager CLogManager::g_Logger;

static CThreadAtomic<bool32> s_bLogAvailable;
static CThreadAtomic<bool32> s_bExitApp;
static CThread *s_pLogThread;
static eastl::vector<CString> LogMessageQueue;
static CThreadMutex s_LoggerLock;

// if true, each log will include the call source
bool32 CLogManager::bLogIncludeFileInfo = false;

// if true, each log will include the time
bool32 CLogManager::bLogIncludeTimeInfo = true;

static CVar<uint32_t> e_LogLevel(
    "e.LogLevel",
    0,
    Cvar_Save,
    "Sets the global log level of the engine.",
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
    s_bExitApp.store( false );

    LogMessageQueue.reserve( 1000 );

    s_pLogThread = new ( malloc( sizeof( *s_pLogThread ) ) ) CThread( "LogThread" );
    s_pLogThread->Start( CLogManager::LogThread );
}

void CLogManager::ShutdownLogger( void )
{
    if ( !s_bExitApp.load() ) {
        s_bExitApp.store( true );
    }
    LogMessageQueue.clear();

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

void SIRENGINE_ATTRIBUTE(format(printf, 4, 5)) CLogManager::LogInfo( const char *pFileName, uint64_t nLineNumber,
    const char *fmt, ... )
{
    va_list argptr;
    char msg[8192];
    char buf[20000];
    int len;

    va_start( argptr, fmt );
    SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
    va_end( argptr );

    len = SIREngine_snprintf( buf, sizeof( buf ) - 1, "%s %s\n", GetExtraString( pFileName, NULL, nLineNumber ), msg );
    
    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
    LogMessageQueue.emplace_back( buf );
}

void SIRENGINE_ATTRIBUTE(format(printf, 4, 5)) CLogManager::LogWarning( const char *pFileName, uint64_t nLineNumber,
    const char *fmt, ... )
{
    va_list argptr;
    char msg[8192];
    char buf[20000];
    int len;

    va_start( argptr, fmt );
    SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
    va_end( argptr );

    len = SIREngine_snprintf( buf, sizeof( buf ) - 1,
        "\x1B[" TTY_COLOR_RED "m WARNING \x1B[" TTY_COLOR_YELLOW "m"
        " %s %s \x1B[0m\n", GetExtraString( pFileName, NULL, nLineNumber ), msg );
    
    CThreadAutoLock<CThreadMutex> _( s_LoggerLock );
    LogMessageQueue.emplace_back( buf );
}

void SIRENGINE_ATTRIBUTE(format(printf, 4, 5)) CLogManager::LogError( const char *pFileName, uint64_t nLineNumber,
    const char *fmt, ... )
{
    va_list argptr;
    char msg[8192];
//    char buf[20000];
//    int len;

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

    while ( !s_bExitApp.load() ) {
        if ( LogMessageQueue.size() ) {
            CThreadAutoLock<CThreadMutex> _( lock );
            const CString& queueMessage = LogMessageQueue.back();
            g_pApplication->FileWrite( queueMessage.c_str(), queueMessage.size(), SIRENGINE_STDOUT_HANDLE );

            LogMessageQueue.pop_back();
        }
    }
}