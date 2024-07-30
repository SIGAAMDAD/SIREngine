#include <Engine/Core/SIREngine.h>
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

const char *SIRENGINE_ATTRIBUTE(format(printf, 1, 2)) SIRENGINE_TEMP_VSTRING( const char *fmt, ... )
{
    va_list argptr;
    char str[8192];
    static char *msg;
    int length;

    va_start( argptr, fmt );
    length = SIREngine_Vsnprintf( str, sizeof( str ) - 1, fmt, argptr );
    va_end( argptr );

    msg = (char *)alloca( length );
    memcpy( msg, str, length );

    return msg;
}

size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf( char *pszBuffer, size_t nMaxSize, const char *fmt, ... )
{
    va_list argptr;
    size_t nLength;

    va_start( argptr, fmt );
    nLength = SIREngine_Vsnprintf( pszBuffer, nMaxSize, fmt, argptr );
    va_end( argptr );

    return nLength;
}

void SIREngine_strcpyz( char *pDest, const char *pSource, size_t nLength )
{
    if ( !pDest ) {
        g_pApplication->Error( "SIREngine_strcpyz: NULL destination" );
    }
    if ( !pSource ) {
        g_pApplication->Error( "SIREngine_strcpyz: NULL source" );
    }
    if ( nLength < 1 ) {
        g_pApplication->Error( "SIREngine_strcpyz: funny length" );
    }

    strncpy( pDest, pSource, nLength - 1 );
    pDest[ nLength - 1 ] = '\0';
}