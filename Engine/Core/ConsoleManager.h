#ifndef __SIRENGINE_CONSOLE_MANAGER_H__
#define __SIRENGINE_CONSOLE_MANAGER_H__

#pragma once

#include <Engine/Core/Cvar.h>
#include <EASTL/unordered_map.h>

class CConsoleManager
{
public:
    CConsoleManager( void );
    ~CConsoleManager();

    void RegisterCVar( IConsoleVar *pCvar );
private:
    eastl::unordered_map<CString, IConsoleVar *> m_CvarList;

    CString m_SavedLocale;
};

#endif