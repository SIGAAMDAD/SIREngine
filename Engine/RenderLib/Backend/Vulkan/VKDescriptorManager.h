#ifndef __VK_DESCRIPTOR_MANAGER_H__
#define __VK_DESCRIPTOR_MANAGER_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"

typedef struct DescriptorCacheEntry {
    VkDescriptorSet m_hSetData;
    VkDescriptorSetLayout m_hLayout;
} DescriptorCacheEntry_t;

class VKDescriptorManager
{
public:
    VKDescriptorManager( void );
    ~VKDescriptorManager();

    DescriptorCacheEntry_t& AddUniformSet( const char *pName, const UniformInfo_t *pUniformInfo );
private:
    eastl::unordered_map<CString, DescriptorCacheEntry_t> m_UniformCache;
};

#endif