#ifndef __VK_SHADER_H__
#define __VK_SHADER_H__

#pragma once

#include "../RenderShader.h"
#include "VKCommon.h"

class VKShader : public IRenderShader
{
public:
    VKShader( const RenderShaderInit_t& shaderInfo );
    virtual ~VKShader() override;

    virtual const char *GetName( void ) const override;
    virtual bool IsValid( void ) const override;
    virtual void Reload( void ) override;
    virtual void Release( void ) override;

    virtual RenderShaderType_t GetType( void ) const override;
    virtual const char *GetName( void ) const override;

    VkShaderModule GetVKModule( void )
    {
        return m_hModule;
    }
private:
    virtual bool Load( const char *pszFilePath ) override;

    VkShaderModule m_hModule;
};

#endif