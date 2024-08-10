#ifndef __JSON_READER_H__
#define __JSON_READER_H__

#pragma once

#include <Engine/Core/Serialization/SerializerBase.h>
#include <Engine/Core/FileSystem/MemoryFile.h>
#include "JsonObject.h"

class CJsonReader : public ISerializerBase
{
public:
    CJsonReader( void );
    virtual ~CJsonReader() override;

    virtual bool Load( const FileSystem::CFilePath& fileName ) override;
    virtual bool Save( const FileSystem::CFilePath& filePath ) override;
private:
    bool Parse( const CMemoryFile& fileData );

    eastl::unordered_map<CString, CJsonObject> m_Objects;
};

#endif