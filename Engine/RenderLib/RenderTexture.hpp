#ifndef __RENDER_TEXTURE_HPP__
#define __RENDER_TEXTURE_HPP__

#pragma once

#include <Engine/RenderLib/RenderCommon.hpp>

class IRenderTexture : public CResourceDef
{
public:
    IRenderTexture( void );
    IRenderTexture( const char *pszFileName );
    virtual ~IRenderTexture();

    virtual const char *GetPath( void ) const = 0;

    virtual void *GetBuffer( void ) = 0;
    virtual const void *GetBuffer( void ) const = 0;
    virtual uint64_t GetSize( void ) const = 0;
protected:
    virtual void Upload( void );
};

#endif