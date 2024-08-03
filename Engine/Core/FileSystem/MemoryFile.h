#ifndef __SIRENGINE_MEMORYFILE_H__
#define __SIRENGINE_MEMORYFILE_H__

#pragma once

#include <Engine/Memory/Allocators/SlabAllocator.h>
#include <Engine/Memory/Allocators/MemoryBuffer.h>
#include "FileStream.h"
#include "FileReader.h"

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
    FileSystem::CFileReader file( filePath );

    if ( !file.IsOpen() ) {
        SIRENGINE_WARNING( "Error opening file '%s'\n", filePath.c_str() );
        return false;
    }

    m_Data.resize( file.GetLength() );
    file.Read( m_Data.data(), m_Data.size() );

    return true;
}

#endif