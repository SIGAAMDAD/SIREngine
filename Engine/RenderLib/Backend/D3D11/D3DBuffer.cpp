#include "D3DBuffer.h"

D3DBuffer::D3DBuffer( void )
{
    D3D11_SUBRESOURCE_DATA bufferData;

    CreateBuffer(  );
}