#ifndef __SIRENGINE_CONFIG_MANAGER_H__
#define __SIRENGINE_CONFIG_MANAGER_H__

#pragma once

#include "FileSystem/FilePath.h"
#include "SIREngine.h"
#include <nlohmann/json.hpp>

class CEngineConfigManager
{
public:
    CEngineConfigManager( void );
    ~CEngineConfigManager();

    bool Load( const FileSystem::CFilePath& configPath );
private:
};

#endif