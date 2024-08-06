#ifndef __SIRENGINE_CONSOLE_MANAGER_H__
#define __SIRENGINE_CONSOLE_MANAGER_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

class CIniSerializer;

#include <Engine/Core/Serialization/Ini/IniSerializer.h>
#include <Engine/Core/Cvar.h>
#include <EASTL/unordered_map.h>

class CConsoleManager
{
public:
    CConsoleManager( void );
    ~CConsoleManager();

    void RegisterCVar( IConsoleVar *pCvar );
    void LoadConfig( void );
    void SaveConfig( void ) const;
private:
    eastl::unordered_map<CString, IConsoleVar *> m_CvarList;
    CIniSerializer *m_pConfigLoader;

    const char *m_pSavedLocale;
};

extern CConsoleManager g_ConsoleManager;

#endif