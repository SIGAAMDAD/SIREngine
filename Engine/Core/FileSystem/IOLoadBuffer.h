#ifndef __FILE_BUFFER_H__
#define __FILE_BUFFER_H__

#pragma once

#include <Engine/Memory/SlabAllocator.h>
#include <Engine/Memory/MemoryBuffer.h>
#include <Engine/Util/CString.h>
#include "FileReader.h"

class CIOLoadBuffer : public IMemoryBuffer<uint8_t, CSlabAllocator>, CFileReader
{
public:
    CIOLoadBuffer( const CString& fileName );
private:
};

#endif