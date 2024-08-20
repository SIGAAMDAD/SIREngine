#include "../ImageLoader.h"
#include "stb_image.h"
#include "../RenderContext.h"

using namespace SIREngine::FileSystem;
using namespace SIREngine;

namespace SIREngine::RenderLib::Backend {

bool CImageLoader::LoadTGA( const uint8_t *pBuffer, uint64_t nSize, CVector<uint8_t>& outBuffer,
	uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples )
{
	int w, h, channels;
	byte *pOut;
	
	pOut = stbi_load_from_memory( pBuffer, nSize, &w, &h, &channels, 4 );
	if ( !pOut ) {
		SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Warning, "Error loading image file with stb_image: %s", stbi_failure_reason() );
		return false;
	}

	outBuffer.reserve( w * h * channels );
	outBuffer.insert( outBuffer.end(), pOut, pOut + ( w * h * channels ) );
	nWidth = w;
	nHeight = h;
	nSamples = channels;

	stbi_image_free( pOut );

	return true;
}

};
/*
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
*/