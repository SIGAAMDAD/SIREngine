#ifndef __SIRENGINE_LOGMACROS_H__
#define __SIRENGINE_LOGMACROS_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "Logger.h"

#define SIRENGINE_LOG( ... ) \
    SIREngine::CLogManager::g_Logger.LogInfo( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )
#define SIRENGINE_WARNING( ... ) \
    SIREngine::CLogManager::g_Logger.LogWarning( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )
#define SIRENGINE_ERROR( ... ) \
    SIREngine::CLogManager::g_Logger.LogError( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )

#define SIRENGINE_NOTIFICATION( ... ) \
    SIREngine::CLogManager::g_Logger.SendNotification( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )

#define SIRENGINE_LOG_LEVEL( Category, Level, ... ) \
    SIREngine::CLogManager::g_Logger.LogCategory( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__, &Log ## Category, Level }, __VA_ARGS__ )

#define SIRENGINE_DEFINE_LOG_CATEGORY( Category, Verbosity ) \
    SIREngine::CLogCategory Log ## Category ( SIRENGINE_XSTRING( Log ## Category ), Verbosity )
#define SIRENGINE_DECLARE_LOG_CATEGORY( Category, Verbosity ) \
    extern SIREngine::CLogCategory Log ## Category
#define SIRENGINE_SILENCE_LOG_CATEGORY( Category )
#define SIRENGINE_SET_LOG_CATEGORY_VERBOSITY( Category, Verbosity )

#endif