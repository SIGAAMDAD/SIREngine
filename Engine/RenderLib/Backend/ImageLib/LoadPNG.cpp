#include "../ImageLoader.h"
#include <pngloader/lodepng.h>

using namespace SIREngine::FileSystem;
using namespace SIREngine;

namespace SIREngine::RenderLib::Backend {

bool CImageLoader::LoadPNG( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
    uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples )
{
    lodepng::decode( outBuffer, nWidth, nHeight, fileBuffer.GetBuffer(), fileBuffer.GetSize() );

    return true;
}

};