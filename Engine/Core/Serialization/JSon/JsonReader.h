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

    virtual bool Load( const CString& fileName ) override;
    virtual bool Save( void ) override;
private:
    bool Parse( const CMemoryFile& fileData );

    CVector<CJsonObject> m_Objects;
};

#endif