#ifndef __SIRENGINE_ENGINE_APP_H__
#define __SIRENGINE_ENGINE_APP_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Core/CvarSystem.h>

class IEngineApp
{
public:
    IEngineApp( void );
    virtual ~IEngineApp();

    virtual const char *GetName( void ) const = 0;
    virtual bool IsActive( void ) const = 0;
    virtual uint32_t GetState( void ) const = 0;

    virtual void Init( void ) = 0;
    virtual void Shutdown( void ) = 0;
    virtual void Frame( int64_t msec ) = 0;

    virtual void SaveGame( void ) = 0;
    virtual void LoadGame( void ) = 0;
};

#endif