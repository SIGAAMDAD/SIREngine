#ifndef __JSON_OBJECT_H__
#define __JSON_OBJECT_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <EASTL/any.h>

class CJsonObject
{
public:
    CJsonObject( const CString& name );
    ~CJsonObject();

    const CString& GetName( void ) const;
    
    uint32_t GetUInt( void ) const;
    uint64_t GetULong( void ) const;
private:
    CString m_Name;
    eastl::any m_Data;
};

SIRENGINE_FORCEINLINE uint32_t CJsonObject::GetUInt( void ) const
{
    return eastl::any_cast<uint32_t>( m_Data );
}

#endif