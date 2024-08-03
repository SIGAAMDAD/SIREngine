#ifndef __SIRENGINE_LOGGER_H__
#define __SIRENGINE_LOGGER_H__

#pragma once

#include <Engine/Core/SIREngine.h>

class CLogManager
{
public:
    CLogManager( void );
    ~CLogManager();

    void LogInfo( const char *pFileName, uint64_t nFileNumber,
        const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 4, 5));
    void LogWarning( const char *pFileName, uint64_t nFileNumber,
        const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 4, 5));

    static CLogManager g_Logger;
};

#include "LogMacros.h"

#endif