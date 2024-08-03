#ifndef __SIRENGINE_SCRIPTLIB_HPP__
#define __SIRENGINE_SCRIPTLIB_HPP__

#include <Engine/Core/EngineApp.h>

#if defined(SIR_ENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

class CScriptLib : public IEngineApp
{
public:
    CScriptLib( void );
    virtual ~CScriptLib();

    virtual const char *GetName( void ) const override;
    virtual bool IsActive( void ) const override;
    virtual uint32_t GetState( void ) const override;

    virtual void Init( void ) override;
    virtual void Shutdown( void ) override;
    virtual void Frame( int64_t msec ) override;

    virtual void SaveGame( void ) override;
    virtual void LoadGame( void ) override;
};

#endif