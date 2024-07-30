#ifndef __D3D_BUFFER_H__
#define __D3D_BUFFER_H__

#pragma once

#include "../RenderBuffer.h"
#include <d3d11.h>

class D3DBuffer : public IRenderBuffer
{
public:
    D3DBuffer( void );
    virtual ~D3DBuffer() override;
private:
    ID3D11BUFFER m_hBufferData;
};

#endif