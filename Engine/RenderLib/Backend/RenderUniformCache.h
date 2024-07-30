#ifndef __RENDER_UNIFORM_CACHE_H__
#define __RENDER_UNIFORM_CACHE_H__

#pragma once

#include "../RenderCommon.h"
#include <EASTL/unordered_map.h>

typedef struct {
    CString name;

    union {
        float f;
        CVec2 f2;
        CVec3 f3;
        CVec4 f4;

        unsigned u;
        CUVec2 u2;
        CUVec3 u3;
        CUVec4 u4;

        int i;
        CIVec2 i2;
        CIVec3 i3;
        CIVec4 i4;
    };
} UniformData_t;

class CRenderUniformCache
{
public:
    CRenderUniformCache( void )
    { }
    ~CRenderUniformCache()
    { }

    UniformData_t& GetUniform( const CString& name )
    {
        auto it = m_Uniforms.find( name );
        if ( it != m_Uniforms.end() ) {
            return it->second;
        }
        return m_UniformInvalid;
    }
private:
    eastl::unordered_map<CString, UniformData_t> m_Uniforms;
    static UniformData_t m_UniformInvalid;
};

#endif