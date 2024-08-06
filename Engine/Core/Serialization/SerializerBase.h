#ifndef __SERIALIZER_BASE_H__
#define __SERIALIZER_BASE_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Util/CString.h>
#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/FileSystem/FilePath.h>

class ISerializerBase
{
public:
    ISerializerBase( void )
    { }
    virtual ~ISerializerBase()
    { }

    virtual const FileSystem::CFilePath& GetFilePath( void ) const
    { return m_FilePath; }
    virtual const FileSystem::CFilePath& GetFileName( void ) const
    { return m_FilePath; }

    virtual bool Load( const FileSystem::CFilePath& fileName ) = 0;
    virtual bool Save( const FileSystem::CFilePath& filePath ) = 0;
protected:
    FileSystem::CFilePath m_FilePath;
};

#endif