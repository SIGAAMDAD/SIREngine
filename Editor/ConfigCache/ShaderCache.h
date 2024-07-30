#ifndef __SHADER_CACHE_H__
#define __SHADER_CACHE_H__

#pragma once

#include "ConfigCacheBase.h"

typedef struct {
    CString id;
    VertexAttribType_t nType;
    uint32_t nBinding;
} ShaderData_t;

typedef struct {
    CString name;
    CVector<ShaderData_t> input;
    CVector<ShaderData_t> uniforms;
} VertexObject_t;

typedef struct {
} FragmentObject_t;

class CShaderCache : public IConfigCacheBase
{
public:
    CShaderCache( void );
    virtual ~CShaderCache();
private:
    CVector<VertexObject_t> m_VertexObjects;
};

#endif