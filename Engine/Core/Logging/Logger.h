#ifndef __SIRENGINE_LOGGER_H__
#define __SIRENGINE_LOGGER_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <EASTL/queue.h>

enum class ELogLevel {
    Pendantic,
    Verbose,
    Developer
};

class CLogManager
{
public:
    CLogManager( void );
    ~CLogManager();

    void LogInfo( const char *pFileName, uint64_t nFileNumber,
        const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 4, 5));
    void LogWarning( const char *pFileName, uint64_t nFileNumber,
        const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 4, 5));
    void LogError( const char *pFileName, uint64_t nFileNumber,
        const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 4, 5));

    static void LaunchLoggingThread( void );
    static void ShutdownLogger( void );

    static CLogManager g_Logger;

    static bool32 bLogIncludeFileInfo;
    static bool32 bLogIncludeTimeInfo;
private:
    static void LogThread( void );

    static const char *GetExtraString( const char *pFileName, const char *pFunction, uint64_t nLineNumber );
};

#include "LogMacros.h"

#endif