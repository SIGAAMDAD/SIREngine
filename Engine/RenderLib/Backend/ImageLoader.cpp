#include "ImageLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "ImageLib/stb_image.h"
#include "RenderContext.h"

using namespace SIREngine::FileSystem;

namespace SIREngine::RenderLib::Backend {

CImageLoader::CImageLoader( const FileSystem::CFilePath& filePath )
{
	Load( filePath );
}

CImageLoader::~CImageLoader()
{
}

bool CImageLoader::Load( const FileSystem::CFilePath& filePath )
{
	const CString ext = FileSystem::CFilePath::GetExtension( filePath.c_str() );
	bool (*LoadFunc)( const CMemoryFile&, CVector<uint8_t>&, uint32_t&, uint32_t&, uint32_t& );

	if ( ext == "tga" ) {
        LoadFunc = LoadTGA;
	} else if ( ext == "jpg" || ext == "jpeg" ) {
        LoadFunc = LoadJpeg;
	} else if ( ext == "bmp" ) {
        LoadFunc = LoadBMP;
	} else if ( ext == "png" ) {
		LoadFunc = LoadPNG;
	} else if ( ext == "pcx" ) {
        LoadFunc = LoadPCX;
	} else if ( ext == "svg" ) {
		LoadFunc = LoadSVG;
	} else {
		SIRENGINE_ERROR( "Invalid image format for file '%s'", filePath.c_str() );
	}

	/*
	int w, h, channels;
	byte *pOut;
	
	pOut = stbi_load( filePath.c_str(), &w, &h, &channels, 3 );
	if ( !pOut ) {
		SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Warning, "Error loading image file with stb_image: %s", stbi_failure_reason() );
		return false;
	}

	m_ImageBuffer.reserve( w * h * channels );
	m_ImageBuffer.insert( m_ImageBuffer.end(), pOut, pOut + ( w * h * channels ) );
	m_nWidth = w;
	m_nHeight = h;
	m_nSamples = channels;

	stbi_image_free( pOut );
	*/

	CMemoryFile file( filePath );
	if ( !file.GetSize() ) {
		SIRENGINE_WARNING( "Error loading image file \"%s\", couldn't load the file", filePath.c_str() );
		return false;
	}
	if ( !LoadFunc( file, m_ImageBuffer, m_nWidth, m_nHeight, m_nSamples ) ) {
		SIRENGINE_WARNING( "Error loading image file '%s'", filePath.c_str() );
		return false;
	}
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Loaded image file \"%s\".", filePath.c_str() );

	return true;
}

};