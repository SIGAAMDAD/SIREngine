#include "Logger.h"
#include <unistd.h>
#include <time.h>

#define TTY_COLOR_BLACK "30"
#define TTY_COLOR_RED "31"
#define TTY_COLOR_GREEN "32"
#define TTY_COLOR_YELLOW "33"
#define TTY_COLOR_BLUE "34"
#define TTY_COLOR_MAGENTA "35"
#define TTY_COLOR_CYAN "36"
#define TTY_COLOR_WHITE "0"

CLogManager CLogManager::g_Logger;

CLogManager::CLogManager( void )
{
}

CLogManager::~CLogManager()
{
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

    len = SIREngine_snprintf( buf, sizeof( buf ) - 1, "(%s:%lu)[%s] %s\n", pFileName, nLineNumber, GetTime(),
        msg );
    
    g_pApplication->FileWrite( buf, len, SIRENGINE_STDOUT_HANDLE );
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
        " (%s:%lu)[%s] %s \x1B[0m\n", pFileName, nLineNumber, GetTime(), msg );
    
    g_pApplication->FileWrite( buf, len, SIRENGINE_STDERR_HANDLE );
}