#include "ImageLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "ImageLib/stb_image.h"
#include "RenderContext.h"

using namespace SIREngine::FileSystem;

namespace SIREngine::RenderLib::Backend {

CImageLoader::CImageLoader( const uint8_t *pBuffer, uint64_t nSize )
{
	Load( pBuffer, nSize );
}

CImageLoader::CImageLoader( const FileSystem::CFilePath& filePath )
{
	Load( filePath );
}

CImageLoader::~CImageLoader()
{
}

bool CImageLoader::Load( const uint8_t *pBuffer, uint64_t nSize )
{
	int w, h, channels;
	byte *pOut;

	pOut = stbi_load_from_memory( pBuffer, nSize, &w, &h, &channels, 3 );
	if ( !pOut ) {
		SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Warning, "Error loading image file with stb_image: %s", stbi_failure_reason() );
		return false;
	}

	m_ImageBuffer.resize( w * h * channels );
	memcpy( m_ImageBuffer.data(), pOut, w * h * channels );
	m_nWidth = w;
	m_nHeight = h;
	m_nSamples = channels;

	stbi_image_free( pOut );

	return true;
}

bool CImageLoader::Load( const FileSystem::CFilePath& filePath )
{
	CMemoryFile file( filePath );

	if ( !file.GetSize() ) {
		SIRENGINE_WARNING( "Error loading image file \"%s\", couldn't load the file", filePath.c_str() );
		return false;
	}
	
	if ( !Load( file.GetBuffer(), file.GetSize() ) ) {
		return false;
	}

	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Loaded image file \"%s\".", filePath.c_str() );

	return true;
}

};