#ifndef __SIRENGINE_LOGMACROS_H__
#define __SIRENGINE_LOGMACROS_H__

#pragma once

#define SIRENGINE_LOG( ... ) \
    CLogManager::g_Logger.LogInfo( __FILE__, __LINE__, __VA_ARGS__ )
#define SIRENGINE_WARNING( ... ) \
    CLogManager::g_Logger.LogWarning( __FILE__, __LINE__, __VA_ARGS__ )

#endif