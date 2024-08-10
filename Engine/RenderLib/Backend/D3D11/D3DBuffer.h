#ifndef __D3D_BUFFER_H__
#define __D3D_BUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

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