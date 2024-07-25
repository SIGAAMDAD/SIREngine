#ifndef __RENDER_BUFFER_HPP__
#define __RENDER_BUFFER_HPP__

#pragma once

#include "RenderCommon.hpp"

class IRenderBuffer
{
public:
    IRenderBuffer( gpuBufferType_t nType );
    virtual ~IRenderBuffer();

    virtual void Init( void ) = 0;
    virtual void Init( uint64_t nItems ) = 0;
    virtual void Shutdown( void ) = 0;

    virtual void Copy( const IRenderBuffer& other ) = 0;
    virtual void Clear( void ) = 0;
    virtual void Resize( uint64_t nSize );

    virtual void *GetBuffer( void ) = 0;
    virtual const void *GetBuffer( void ) const = 0;
    virtual uint64_t GetSize( void ) const = 0;
protected:
    char m_szBufferName[64];
    uint64_t m_nBufferSize;
    gpuBufferType_t m_nType;
};

#endif