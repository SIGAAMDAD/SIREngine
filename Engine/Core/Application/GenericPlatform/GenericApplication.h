#ifndef __GENERIC_APPLICATION_H__
#define __GENERIC_APPLICATION_H__

#pragma once

#include <Engine/Core/EngineApp.h>
#include <Engine/RenderLib/Backend/RenderContext.h>

typedef struct {

} MemoryStats_t;

typedef enum {
    RAPI_OPENGL,
    RAPI_VULKAN,
    RAPI_D3D11,
    RAPI_SOFTWARE
} RenderAPIType_t;

class IGenericApplication
{
public:
    IGenericApplication( const ApplicationInfo_t& appInfo, const CVector<CString>& cmdLine );
    virtual ~IGenericApplication();

    const ApplicationInfo_t& GetAppInfo( void ) const;
    const CVector<CString>& GetCommandLine( void ) const;
    RenderAPIType_t GetRenderAPI( void ) const;

    void Error( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));
    void Warning( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));
    void Log( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));

    virtual void Init( void );
    virtual void Run( void );

    virtual FILE *OpenFile( const CString& filePath, const char *mode ) = 0;

    virtual size_t GetOSPageSize( void ) const = 0;

    virtual size_t GetAllocSize( void *pBuffer ) const = 0;
    virtual void *VirtualAlloc( size_t nSize, size_t nAlignment ) = 0;
    virtual void VirtualFree( void *pBuffer ) = 0;

    virtual void *FileOpen( const CString& filePath ) = 0;
    virtual void FileClose( void *hFile ) = 0;
    virtual size_t FileWrite( const void *pBuffer, size_t nBytes, void *hFile ) = 0;
    virtual size_t FileRead( void *pBuffer, size_t nBytes, void *hFile ) = 0;
    virtual size_t FileTell( void *hFile ) = 0;
    virtual size_t FileLength( void *hFile ) = 0;

    virtual void MutexLock( void * ) = 0;
    virtual void MutexUnlock( void * ) = 0;
protected:
    ApplicationInfo_t m_ApplicationInfo;
    RenderAPIType_t m_nRendererType;

    CVector<CString> m_CommandLineArgs;
};

extern IGenericApplication *g_pApplication;

SIRENGINE_FORCEINLINE const CVector<CString>& IGenericApplication::GetCommandLine( void ) const
{
    return m_CommandLineArgs;
}

SIRENGINE_FORCEINLINE const ApplicationInfo_t& IGenericApplication::GetAppInfo( void ) const
{
    return m_ApplicationInfo;
}

SIRENGINE_FORCEINLINE RenderAPIType_t IGenericApplication::GetRenderAPI( void ) const
{
    return m_nRendererType;
}

#endif