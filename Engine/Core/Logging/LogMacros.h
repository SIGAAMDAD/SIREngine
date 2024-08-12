#ifndef __SIRENGINE_LOGMACROS_H__
#define __SIRENGINE_LOGMACROS_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#define SIRENGINE_LOG( ... ) \
    SIREngine::Logging::CLogManager::g_Logger.LogInfo( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )
#define SIRENGINE_WARNING( ... ) \
    SIREngine::Logging::CLogManager::g_Logger.LogWarning( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )
#define SIRENGINE_ERROR( ... ) \
    SIREngine::Logging::CLogManager::g_Logger.LogError( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )

#define SIRENGINE_NOTIFICATION( ... ) \
    SIREngine::Logging::CLogManager::g_Logger.SendNotification( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )

#define SIRENGINE_LOG_LEVEL( Category, Level, ... ) \
    SIREngine::Logging::CLogManager::g_Logger.LogCategory( { __FILE__, SIRENGINE_FUNC_NAME, __LINE__ }, __VA_ARGS__ )

#define SIRENGINE_DEFINE_LOG_CATEGORY( Category, Verbosity )
#define SIRENGINE_SILENCE_LOG_CATEGORY( Category )
#define SIRENGINE_SET_LOG_CATEGORY_VERBOSITY( Category, Verbosity )

#endif