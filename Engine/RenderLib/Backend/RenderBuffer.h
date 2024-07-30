#ifndef __RENDER_BUFFER_H__
#define __RENDER_BUFFER_H__

#pragma once

#include "../RenderCommon.h"

class IRenderBuffer
{
public:
    IRenderBuffer( void )
        : m_nBufferSize( 0 ), m_nType( BUFFER_TYPE_NONE )
    { }
    virtual ~IRenderBuffer()
    { }

    virtual const char *GetName( void ) const;
    virtual uint64_t GetSize( void ) const;
    virtual GPUBufferType_t GetType( void ) const;

    virtual void Init( void ) = 0;
    virtual void Init( uint64_t nItems ) = 0;
    virtual void Shutdown( void ) = 0;

    virtual void Copy( const IRenderBuffer& other ) = 0;
    virtual void Clear( void ) = 0;
    virtual void Resize( uint64_t nSize ) = 0;

    virtual void *GetBuffer( void ) = 0;
    virtual const void *GetBuffer( void ) const = 0;
    virtual uint64_t GetSize( void ) const = 0;
protected:
    CString m_BufferName;
    uint64_t m_nBufferSize;
    GPUBufferType_t m_nType;
};

SIRENGINE_FORCEINLINE const char *IRenderBuffer::GetName( void ) const
{
    return m_BufferName.c_str();
}

SIRENGINE_FORCEINLINE uint64_t IRenderBuffer::GetSize( void ) const
{
    return m_nBufferSize;
}

SIRENGINE_FORCEINLINE GPUBufferType_t IRenderBuffer::GetType( void ) const
{
    return m_nType;
}

#endif