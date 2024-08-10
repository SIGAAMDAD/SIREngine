#ifndef __RENDER_UNIFORM_CACHE_H__
#define __RENDER_UNIFORM_CACHE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "../RenderCommon.h"
#include <EASTL/unordered_map.h>

class CRenderUniformCache
{
public:
    CRenderUniformCache( void )
    { }
    ~CRenderUniformCache()
    { }
private:
    eastl::unordered_map<CString, UniformData_t> m_Uniforms;
    static UniformData_t m_UniformInvalid;
};

#endif