#include "ImageLoader.h"

CImageLoader::CImageLoader( const CFilePath& filePath )
{
    const CString ext = filePath.GetExtension();
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
    } else {
        g_pApplication->Error( "" );
    }

    CMemoryFile file( filePath );
    if ( !LoadFunc( file, m_ImageBuffer, m_nWidth, m_nHeight, m_nSamples ) ) {
        g_pApplication->Warning( "" );
    }
}

CImageLoader::~CImageLoader()
{
}