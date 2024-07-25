#ifndef __VK_BUFFER_HPP__
#define __VK_BUFFER_HPP__

#pragma once

#include "VKCommon.hpp"
#include <Engine/RenderLib/RenderBuffer.hpp>

class VKBuffer : public IRenderBuffer
{
public:
    VKBuffer( gpuBufferType_t nType );
    virtual ~VKBuffer() override;

    virtual void Init( void ) override;
    virtual void Init( uint64_t nItems ) override;
    virtual void Shutdown( void ) override;

    virtual void *GetBuffer( void ) override;
    virtual const void *GetBuffer( void ) const override;
    virtual uint64_t GetSize( void ) const override;

    virtual void Copy( const IRenderBuffer& other ) override;
    virtual void Clear( void ) override;
    virtual void Resize( uint64_t nSize ) override;
private:
    VkBuffer m_pBuffer;
    VmaAllocation m_Allocation;
    void *m_pMappedGPUBuffer;
};

#endif