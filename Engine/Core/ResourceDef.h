#ifndef __SIRENGINE_RESOURCE_DEF_H__
#define __SIRENGINE_RESOURCE_DEF_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <stdint.h>
#include <stddef.h>

#define MAX_RESOURCE_PATH 128

typedef enum {
    RES_SHADER,
    RES_SOUND,
    RES_MAP,

    RES_INVALID
} resourceType_t;

typedef enum {
    RS_INVALID,
    RS_DEFAULTED,
    RS_LOADED
} resourceState_t;

class IResourceDef
{
public:
    IResourceDef( void )
        : m_nType( RES_INVALID ), m_nState( RS_INVALID )
    { }
    virtual ~IResourceDef()
    { }

    virtual const char *GetName( void ) const = 0;
    virtual bool IsValid( void ) const = 0;
    virtual void Reload( void ) = 0;
    virtual void Release( void ) = 0;
protected:
    char m_szName[MAX_RESOURCE_PATH];
    resourceType_t m_nType;
    resourceState_t m_nState;
};

class CResourceDef : public IResourceDef
{
public:
    CResourceDef( void )
    { }
    virtual ~CResourceDef() override
    { }
};

class CResourceTextDef : public IResourceDef
{
public:
    CResourceTextDef( void )
        : m_pBuffer( NULL ), m_nBufLength( 0 )
    { }
    virtual ~CResourceTextDef() override
    { }

    virtual uint64_t GetTextLength( void ) const = 0;
    virtual const char *GetText( void ) const = 0;
    virtual void SetText( const char *pText ) = 0;
private:
    char *m_pBuffer;
    uint64_t m_nBufLength;
};

#endif