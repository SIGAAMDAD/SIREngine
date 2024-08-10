#ifndef __SIRENGINE_MODULE_MANAGER_H__
#define __SIRENGINE_MODULE_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Util/CVector.h>
#include <Engine/Util/CString.h>
#include <Engine/Core/Compiler.h>
#include <EASTL/shared_ptr.h>

class CModuleInfo
{
public:
    CModuleInfo( void );
    ~CModuleInfo();

    template<typename Fn>
    void LoadProc( Fn*&& fn );
private:
    char m_szName[ SIRENGINE_MAX_OSPATH ];
};

class CModuleManager
{
public:
    eastl::shared_ptr<CModuleInfo> GetModule( const CString& moduleName );
private:
    CVector<eastl::shared_ptr<CModuleInfo>> m_LoadedModules;
};

#endif