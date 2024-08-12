#ifndef __SIRENGINE_MEMORYFILE_H__
#define __SIRENGINE_MEMORYFILE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Memory/Allocators/SlabAllocator.h>
#include <Engine/Memory/Allocators/MemoryBuffer.h>
#include "FileStream.h"
#include "FileReader.h"
#include "FileSystem.h"

namespace SIREngine::FileSystem {
    class CMemoryFile : public IMemoryBuffer<uint8_t, UtlMallocAllocator>
    {
    public:
        CMemoryFile( const FileSystem::CFilePath& filePath )
        { Open( filePath ); }
        virtual ~CMemoryFile() override
        { }

        bool Open( const FileSystem::CFilePath& filePath );
    };

    inline bool CMemoryFile::Open( const FileSystem::CFilePath& filePath )
    {
        FileSystem::CFileReader *hFile = g_pFileSystem->OpenFileReader( filePath );

        if ( !hFile ) {
            SIRENGINE_WARNING( "Error opening file '%s'\n", filePath.c_str() );
            return false;
        }

        m_Data.resize( hFile->GetLength() );
        hFile->Read( m_Data.data(), m_Data.size() );

        delete hFile;

        return true;
    }
};

#endif