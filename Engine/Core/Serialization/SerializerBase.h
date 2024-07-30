#ifndef __SERIALIZER_BASE_H__
#define __SERIALIZER_BASE_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Util/CString.h>

class ISerializerBase
{
public:
    ISerializerBase( void );
    virtual ~ISerializerBase();

    virtual const CString& GetFilePath( void ) const;
    virtual const CString& GetFileName( void ) const;

    virtual bool Load( const CString& fileName ) = 0;
    virtual bool Save( void ) = 0;
protected:
    CString m_FilePath;
};

#endif