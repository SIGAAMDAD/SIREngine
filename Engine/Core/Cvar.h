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

class IConsoleVar
{
public:
    IConsoleVar( void )
    { }
    IConsoleVar( const char *pName, const char *pDescription, uint32_t iFlags, CvarGroup_t nGroup )
        : m_Name( pName ), m_Description( pDescription ), m_iFlags( iFlags ), m_nGroup( nGroup )
    { }
    ~IConsoleVar()
    { }
protected:
    CString m_Name;
    CString m_Description;
    uint32_t m_iFlags;
    CvarGroup_t m_nGroup;
};

template<typename T>
class CVar : public IConsoleVar
{
public:
    CVar( void )
    { }
    CVar( const char *pName, const T defaultValue, uint32_t iFlags, const char *pDescription, CvarGroup_t nGroup )
        : IConsoleVar( pName, pDescription, iFlags, nGroup ), m_Value( defaultValue )
    { }
    ~CVar()
    { }

    T GetValue( void ) const;
    T& GetRef( void );
    const T& GetRef( void ) const;

    void SetValue( const T& value );
    
    const CString& GetName( void ) const;
    const CString& GetDescription( void ) const;
private:
    T m_Value;
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
SIRENGINE_FORCEINLINE void CVar<T>::SetValue( const T& value )
{ m_Value = value; }

template<typename T>
SIRENGINE_FORCEINLINE const CString& CVar<T>::GetName( void ) const
{ return m_Name; }

template<typename T>
SIRENGINE_FORCEINLINE const CString& CVar<T>::GetDescription( void ) const
{ return m_Description; }

#endif