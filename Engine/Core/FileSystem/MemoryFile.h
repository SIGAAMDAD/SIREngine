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
    CMemoryFile( const CString& filePath )
    { Open( filePath ); }
    virtual ~CMemoryFile() override
    { }

    bool Open( const CString& filePath );
};

bool CMemoryFile::Open( const CString& filePath )
{
    CFileReader file( filePath );

    if ( !file.IsOpen() ) {
        return false;
    }

    m_Data.Resize( file.GetLength() );
    file.Read( m_Data.GetBuffer(), m_Data.Size() );

    return true;
}

#endif