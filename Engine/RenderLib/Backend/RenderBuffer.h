#ifndef __RENDER_BUFFER_H__
#define __RENDER_BUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>

namespace SIREngine::RenderLib::Backend {
    class IRenderBuffer
    {
    public:
        IRenderBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage )
            : m_nBufferSize( 0 ), m_nType( nType ), m_nUsage( nUsage )
        { }
        virtual ~IRenderBuffer()
        { }

        static IRenderBuffer *Create( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize );

        virtual const char *GetName( void ) const;
        virtual GPUBufferType_t GetType( void ) const;
        virtual GPUBufferUsage_t GetUsage( void ) const;

        virtual void Init( const void *pBuffer, uint64_t nSize ) = 0;
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
        GPUBufferUsage_t m_nUsage;
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

    SIRENGINE_FORCEINLINE GPUBufferUsage_t IRenderBuffer::GetUsage( void ) const
    {
        return m_nUsage;
    }
};

#endif