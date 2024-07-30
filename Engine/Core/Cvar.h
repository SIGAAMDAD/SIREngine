#ifndef __SIRENGINE_CVAR_H__
#define __SIRENGINE_CVAR_H__

#pragma once

#include "SIREngine.h"
#include <Engine/Util/CString.h>

typedef enum {
    Cvar_Save       = 0x0001,
    Cvar_ReadOnly   = 0x0002,
    
    Cvar_Default    = 0x0000
} CvarFlags_t;

typedef enum {
    CVG_RENDERER,
    CVG_SOUNDSYSTEM,
    CVG_FILESYSTEM,
    CVG_SYSTEMINFO,
    CVG_USERINFO,
    CVG_NONE, // special case -- user created

    NUMCVARGROUPS
} CvarGroup_t;

typedef enum {
    CVT_NONE,
    CVT_INT,
    CVT_FLOAT,
    CVT_STRING,
    CVT_BOOL,
    CVT_FILEPATH,

    CVT_MAX
} CvarType_t;

template<typename T>
class CVar
{
public:
    inline CVar( void )
    { }
    inline CVar( const char *pszName, const T defaultValue, uint32_t iFlags, const char *pszDescription, CvarGroup_t nGroup )
        : m_Name( pszName ), m_Description( pszDescription ), m_Value( defaultValue ),
        m_nGroup( nGroup ), m_iFlags( iFlags )
    { }
    ~CVar()
    { }

    T GetValue( void ) const;
    T& GetRef( void );
    const T& GetRef( void ) const;
    
    const CString& GetName( void ) const;
    const CString& GetDescription( void ) const;
private:
    CString m_Name;
    CString m_Description;
    CVar *m_pNext;
    CVar *m_pPrev;
    T m_Value;

    CvarGroup_t m_nGroup;
    uint32_t m_iFlags;
};

template<typename T>
SIRENGINE_FORCEINLINE T CVar<T>::GetValue( void ) const
{ return m_Value; }

template<typename T>
SIRENGINE_FORCEINLINE T& CVar<T>::GetRef( void )
{ return m_Value; }

template<typename T>
SIRENGINE_FORCEINLINE const T& CVar<T>::GetRef( void ) const
{ return m_Value; }

template<typename T>
SIRENGINE_FORCEINLINE const CString& CVar<T>::GetName( void ) const
{ return m_Name; }

template<typename T>
SIRENGINE_FORCEINLINE const CString& CVar<T>::GetDescription( void ) const
{ return m_Description; }

#endif