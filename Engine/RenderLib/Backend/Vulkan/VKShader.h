#ifndef __VK_SHADER_H__
#define __VK_SHADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderShader.h"

class VKShader : public IRenderShader
{
public:
    VKShader( const RenderShaderInit_t& shaderInfo );
    virtual ~VKShader() override;

    virtual const char *GetName( void ) const override { return m_szName; }
    virtual bool IsValid( void ) const override { return false; }
    virtual void Reload( void ) override { }
    virtual void Release( void ) override { }

    virtual RenderShaderType_t GetType( void ) const override
    { return m_nType; }

    VkShaderModule GetVKModule( void )
    {
        return m_hModule;
    }
private:
    virtual bool Load( const char *pszFilePath ) override { return false; }

    VkShaderModule m_hModule;
};

#endif