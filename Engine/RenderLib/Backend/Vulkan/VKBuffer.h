#ifndef __VK_BUFFER_H__
#define __VK_BUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderBuffer.h"
#include "VKContext.h"

namespace SIREngine::RenderLib::Backend::Vulkan {
    class VKBuffer : public IRenderBuffer
    {
    public:
        VKBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize );
        virtual ~VKBuffer() override;

        virtual void Init( const void *pBuffer, uint64_t nSize ) override;
        virtual void Shutdown( void ) override;

        SIRENGINE_FORCEINLINE virtual void *GetBuffer( void ) override
        { return m_pMappedGPUBuffer; }
        SIRENGINE_FORCEINLINE virtual const void *GetBuffer( void ) const override
        { return m_pMappedGPUBuffer; }
        SIRENGINE_FORCEINLINE virtual uint64_t GetSize( void ) const override
        { return m_nBufferSize; }

        virtual void Copy( const IRenderBuffer& other ) override { }
        virtual void Clear( void ) override { }
        virtual void Resize( uint64_t nSize ) override { }
        virtual void SwapData( VKPipelineSet_t *pSet ) { }

        SIRENGINE_FORCEINLINE VkBuffer GetVKObject( void )
        { return m_pBuffer; }

        SIRENGINE_FORCEINLINE void MapGPUBuffer( void )
        { vmaMapMemory( g_pVKContext->GetAllocator(), m_Allocation, &m_pMappedGPUBuffer ); }
        SIRENGINE_FORCEINLINE void UnmapGPUBuffer( void )
        {
            if ( m_pMappedGPUBuffer ) {
                vmaUnmapMemory( g_pVKContext->GetAllocator(), m_Allocation );
                m_pMappedGPUBuffer = NULL;
            }
        }
    protected:
        VkBuffer m_pBuffer;
        VmaAllocation m_Allocation;
        void *m_pMappedGPUBuffer;
    };
};

#endif