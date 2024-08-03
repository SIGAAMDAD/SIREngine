#ifndef __SIRENGINE_JSON_CACHE_H__
#define __SIRENGINE_JSON_CACHE_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <nlohmann/json.hpp>

class CJsonCache
{
public:
    CJsonCache( void )
    { }
    ~CJsonCache()
    { }

    nlohmann::json& GetJsonObject( const FileSystem::CFilePath& filePath );
private:
    eastl::unordered_map<FileSystem::CFilePath, nlohmann::json> m_JsonCache;
};

#endif