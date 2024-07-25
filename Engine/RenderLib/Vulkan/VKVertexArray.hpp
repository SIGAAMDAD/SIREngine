#ifndef __VK_VERTEX_ARRAY_HPP__
#define __VK_VERTEX_ARRAY_HPP__

#pragma once

#include "VKBuffer.hpp"

class VKVertexArray
{
public:
    VKVertexArray( void );
    ~VKVertexArray();
private:
    typedef struct {
        VkVertexInputBindingDescription m_Binding;
        VkVertexInputAttributeDescription m_Attrib;
    } VertexAttrib_t;

    VertexAttrib_t m_szAttribs;
};

#endif