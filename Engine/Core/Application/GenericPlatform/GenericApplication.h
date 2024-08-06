#ifndef __GENERIC_APPLICATION_H__
#define __GENERIC_APPLICATION_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/EngineApp.h>
#include <Engine/Util/CString.h>
#include <Engine/Util/CVector.h>
#include <Engine/Core/FileSystem/FilePath.h>
#include <Engine/Core/Logging/Logger.h>

typedef struct {

} MemoryStats_t;

typedef enum {
    RAPI_OPENGL,
    RAPI_VULKAN,
    RAPI_D3D11,
    RAPI_SOFTWARE
} RenderAPIType_t;

typedef enum {
    FileMode_ReadOnly,
    FileMode_WriteOnly,
    FileMode_Append,
    FileMode_ReadWrite,
} FileMode_t;

typedef enum {
    Crash_SegmentationViolation,
    Crash_BusError,
    Crash_Assertion,
    Crash_Engine,
} CrashType_t;

class CThread;

class IGenericApplication
{
public:
    IGenericApplication( void );
    virtual ~IGenericApplication();

    virtual void Error( const char *pError ) = 0;

    SIRENGINE_FORCEINLINE ApplicationInfo_t& GetAppInfo( void )
    { return m_ApplicationInfo; }
    SIRENGINE_FORCEINLINE const ApplicationInfo_t& GetAppInfo( void ) const
    { return m_ApplicationInfo; }
    SIRENGINE_FORCEINLINE const CVector<CString>& GetCommandLine( void ) const
    { return m_CommandLineArgs; } 
    SIRENGINE_FORCEINLINE RenderAPIType_t GetRenderAPI( void ) const
    { return m_nRendererType; }
    SIRENGINE_FORCEINLINE const FileSystem::CFilePath& GetGamePath( void ) const
    { return m_GamePath; }

    SIRENGINE_FORCEINLINE void SetCommandLine( const CVector<CString>& commandLine )
    { m_CommandLineArgs = eastl::move( commandLine ); }
    SIRENGINE_FORCEINLINE void SetApplicationArgs( const ApplicationInfo_t& appInfo )
    { m_ApplicationInfo = appInfo; }

    virtual void Init( void );
    virtual void Shutdown( void );
    virtual void Run( void );

    virtual FILE *OpenFile( const CString& filePath, const char *mode ) = 0;

    virtual size_t GetOSPageSize( void ) const = 0;

    virtual void *OpenDLL( const char *pName ) = 0;
    virtual void CloseDLL( void *pDLLHandle ) = 0;
    virtual void *GetProcAddress( void *pDLLHandle, const char *pProcName ) = 0;

    virtual size_t GetAllocSize( void *pBuffer ) const = 0;
    virtual void *VirtualAlloc( size_t *nSize, size_t nAlignment ) = 0;
    virtual void VirtualFree( void *pBuffer ) = 0;
    virtual void CommitMemory( void *pMemory, size_t nOffset, size_t nSize ) = 0;
    virtual void DecommitMemory( void *pMemory, size_t nOffset, size_t nSize ) = 0;
    virtual void SetMemoryReadOnly( void *pMemory, size_t nOffset, size_t nSize ) = 0;

    SIRENGINE_FORCEINLINE virtual void CommitByAddress( void *pMemory, size_t nSize )
    { CommitMemory( pMemory, (size_t)( ( (byte *)pMemory ) - ( (byte *)pMemory ) ), nSize ); }
    SIRENGINE_FORCEINLINE virtual void DecommitByAddress( void *pMemory, size_t nSize )
    { DecommitMemory( pMemory, (size_t)( ( (byte *)pMemory ) - ( (byte *)pMemory ) ), nSize ); }

    virtual void *FileOpen( const CString& filePath, FileMode_t nMode ) = 0;
    virtual void FileClose( void *hFile ) = 0;
    virtual size_t FileWrite( const void *pBuffer, size_t nBytes, void *hFile ) = 0;
    virtual size_t FileRead( void *pBuffer, size_t nBytes, void *hFile ) = 0;
    virtual size_t FileTell( void *hFile ) = 0;
    virtual size_t FileLength( void *hFile ) = 0;

    virtual CVector<FileSystem::CFilePath> ListFiles( const FileSystem::CFilePath& dir, bool bDirectoryOnly = false ) = 0;

    virtual void ThreadStart( void *pThread, CThread *, void (CThread::*pFunction)( void ) ) = 0;
    virtual void ThreadJoin( void *pThread, CThread *, uint64_t nTimeout = SIRENGINE_UINT64_MAX ) = 0;

    virtual void OnOutOfMemory( void ) = 0;

    static CrashType_t nEngineCrashReason;
protected:
    ApplicationInfo_t m_ApplicationInfo;
    RenderAPIType_t m_nRendererType;

    FileSystem::CFilePath m_GamePath;

    CVector<CString> m_CommandLineArgs;
};

extern IGenericApplication *g_pApplication;

#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_LINUX)
    #include <Engine/Core/Application/Posix/PosixApplication.h>
    #include <Engine/Core/Application/Posix/PosixTypes.h>
#endif

#include <Engine/Core/FileSystem/FileSystem.h>

#endif