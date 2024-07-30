#ifndef __IMAGE_LOADER_H__
#define __IMAGE_LOADER_H__

#pragma once

#include "../RenderCommon.h"
#include <Engine/Core/FileSystem/FilePath.h>

class CImageLoader
{
public:
    CImageLoader( const CFilePath& filePath );
    ~CImageLoader();

    SIRENGINE_FORCEINLINE void *GetBuffer( void )
    { return m_ImageBuffer.GetBuffer(); }
    SIRENGINE_FORCEINLINE const void *GetBuffer( void ) const
    { return m_ImageBuffer.GetBuffer(); }
    SIRENGINE_FORCEINLINE uint32_t GetWidth( void ) const
    { return m_nWidth; }
    SIRENGINE_FORCEINLINE uint32_t GetHeight( void ) const
    { return m_nHeight; }
    SIRENGINE_FORCEINLINE uint32_t GetChannels( void ) const
    { return m_nSamples; }

    SIRENGINE_FORCEINLINE size_t GetSize( void ) const
    { return m_ImageBuffer.Size(); }
private:
    static bool LoadJpeg( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
        uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
    static bool LoadPNG( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
        uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
    static bool LoadTGA( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
        uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
    static bool LoadPCX( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
        uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
    static bool LoadBMP( const CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
        uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );

    CVector<uint8_t> m_ImageBuffer;
    uint32_t m_nWidth;
    uint32_t m_nHeight;
    uint32_t m_nSamples;
};

#endif