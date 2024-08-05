#include "ImageLoader.h"

CImageLoader::CImageLoader( const FileSystem::CFilePath& filePath )
{
    const CString ext = FileSystem::CFilePath::GetExtension( filePath.c_str() );
    bool (*LoadFunc)( const CMemoryFile&, CVector<uint8_t>&, uint32_t&, uint32_t&, uint32_t& );

    if ( ext == "tga" ) {
//        LoadFunc = LoadTGA;
    } else if ( ext == "jpg" || ext == "jpeg" ) {
//        LoadFunc = LoadJpeg;
    } else if ( ext == "bmp" ) {
//        LoadFunc = LoadBMP;
    } else if ( ext == "png" ) {
        LoadFunc = LoadPNG;
    } else if ( ext == "pcx" ) {
//        LoadFunc = LoadPCX;
    } else {
        SIRENGINE_ERROR( "Invalid image format for file '%s'", filePath.c_str() );
    }

    CMemoryFile file( filePath );
    if ( !LoadFunc( file, m_ImageBuffer, m_nWidth, m_nHeight, m_nSamples ) ) {
        SIRENGINE_WARNING( "Error loading image file '%s'", filePath.c_str() );
    }
}

CImageLoader::~CImageLoader()
{
}