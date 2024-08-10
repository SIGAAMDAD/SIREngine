#ifndef __CSTR_DICT_H__
#define __CSTR_DICT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>

#define STRTABLE_ID "#str_"

class CStrDict
{
public:
    CStrDict( void );
    ~CStrDict();

    void SetBaseID( uint64_t nId );
private:
    uint64_t GetNextHashID( void ) const;

    eastl::map<CString, CString> m_StrList;
    uint64_t m_nBaseID;
};

SIRENGINE_FORCEINLINE void CStrDict::SetBaseID( uint64_t nId )
{
    m_nBaseID = nId;
}

SIRENGINE_FORCEINLINE uint64_t CStrDict::GetNextHashID( void ) const
{
    uint64_t count, id, test;
    CString work;

    count = m_StrList.size();
    id = m_nBaseID;

    if ( count == 0 ) {
        return id;
    }

    for ( auto it = m_StrList.cbegin(); it != m_StrList.cend(); it++ ) {
        work = it->first;
        work.StripLeading( STRTABLE_ID );
        test = (uint64_t)atoll( work.c_str() );
        if ( test > id ) {
            id = test;
        }
    }
    return id + 1;
}



#endif