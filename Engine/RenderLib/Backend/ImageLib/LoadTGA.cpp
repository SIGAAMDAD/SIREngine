#include "../ImageLoader.h"

typedef struct {
    uint8_t idLength;
    uint8_t colormapType;
    uint8_t imageType;
    uint16_t colormapIndex;
    uint16_t colormapLength;
    uint8_t colormapSize;
    uint16_t xOrigin, yOrigin;
    uint16_t width, height;
    uint8_t pixelSize, attributes;
} tgaHeader_t;

bool CImageLoader::LoadTGA( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
    uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples )
{
    const tgaHeader_t *pHeader;

    pHeader = (const tgaHeader_t *)fileBuffer.GetBuffer();

    if ( fileBuffer.GetSize() < 18 ) {
        return false;
    }

    return true;
}

