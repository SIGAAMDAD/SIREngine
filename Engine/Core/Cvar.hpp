#ifndef __CVAR_HPP__
#define __CVAR_HPP__

#pragma once

#include "SIREngine.hpp"
#include <EASTL/fixed_string.h>

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

class CVar
{
public:
    CVar( void );
    CVar( const char *pszName, const char *pszDefaultValue, uint32_t iFlags, const char *pszDescription, CvarGroup_t nGroup );
    ~CVar();

    int64_t GetInt( void ) const;
    double GetFloat( void ) const;
    bool GetBool( void ) const;
    const eastl::string& GetString( void ) const;

    const eastl::string& GetName( void ) const;
    const eastl::string& GetDescription( void ) const;
private:
    eastl::string m_Name;
    eastl::string m_Description;
    eastl::string m_Value;
    double m_FloatValue;
    int64_t m_IntValue;
    
    CVar *m_pNext;
    CVar *m_pPrev;

    CvarGroup_t m_nGroup;
    uint32_t m_iFlags;
};

#endif