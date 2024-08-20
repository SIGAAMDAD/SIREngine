#include "../ImageLoader.h"
#include "stb_image.h"
#include "../RenderContext.h"

using namespace SIREngine::FileSystem;
using namespace SIREngine;

namespace SIREngine::RenderLib::Backend {

bool CImageLoader::LoadJpeg( const uint8_t *fileBuffer, uint64_t nSize, CVector<uint8_t>& outBuffer,
	uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples )
{
	int w, h, channels;
	byte *pOut;
	
	pOut = stbi_load_from_memory( fileBuffer.GetBuffer(), fileBuffer.GetSize(), &w, &h, &channels, 3 );
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