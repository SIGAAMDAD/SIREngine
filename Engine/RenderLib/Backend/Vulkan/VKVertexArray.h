#ifndef __VK_VERTEX_ARRAY_H__
#define __VK_VERTEX_ARRAY_H__

#pragma once

#include "../RenderVertexArray.h"
#include "VKBuffer.h"

class VKVertexArray : public IRenderVertexArray
{
public:
    VKVertexArray( void );
    virtual ~VKVertexArray() override;

    virtual void SetVertexAttribs( const VertexAttribInfo_t *vertexAttribs, uint64_t nAttribCount ) override;
};

#endif