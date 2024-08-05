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
    inline CVar( void )
    { }
    inline CVar( const char *pName, const T defaultValue, uint32_t iFlags, const char *pDescription, CvarGroup_t nGroup )
        : IConsoleVar( pName, pDescription, iFlags, nGroup ), m_Value( defaultValue )
    { }
    inline ~CVar()
    { }

    inline T GetValue( void ) const
    { return m_Value; }
    inline T& GetRef( void )
    { return m_Value; }
    inline const T& GetRef( void ) const
    { return m_Value; }

    inline void SetValue( const T& value )
    { m_Value = value; }
    inline void SetValue( T&& value )
    { m_Value = eastl::move( value ); }

    inline const CString& GetName( void ) const
    { return m_Name; }
    inline const CString& GetDescription( void ) const
    { return m_Description; }
private:
    T m_Value;
};

template<typename T>
class CVarRef : public IConsoleVar
{
public:
    inline CVarRef( void )
    { }
    inline CVarRef( const char *pName, T& valueRef, uint32_t iFlags, const char *pDescription, CvarGroup_t nGroup )
        : IConsoleVar( pName, pDescription, iFlags, nGroup ), m_pRefValue( eastl::addressof( valueRef ) )
    { }
    inline ~CVarRef()
    { }

    inline T GetValue( void ) const
    { return *m_pRefValue; }
    inline T& GetRef( void )
    { return *m_pRefValue; }
    inline const T& GetRef( void ) const
    { return *m_pRefValue; }

    inline void SetValue( const T& value )
    { *m_pRefValue = value; }
    inline void SetValue( T&& value )
    { *m_pRefValue = eastl::move( value ); }

    inline const CString& GetName( void ) const
    { return m_Name; }
    inline const CString& GetDescription( void ) const
    { return m_Description; }
private:
    T *m_pRefValue;
};

#endif