#ifndef __POSIX_APPLICATION_H__
#define __POSIX_APPLICATION_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>
#include <pthread.h>

class CPosixApplication : public IGenericApplication
{
public:
    CPosixApplication( const ApplicationInfo_t& appInfo, const CVector<CString>& cmdLine );
    virtual ~CPosixApplication() override;

    virtual size_t GetOSPageSize( void ) const override;

    virtual FILE *OpenFile( const CString& filePath, const char *mode ) override;

    virtual size_t GetAllocSize( void *pBuffer ) const override;
    virtual void *VirtualAlloc( size_t nSize, size_t nAlignment ) override;
    virtual void VirtualFree( void *pBuffer ) override;

    virtual void *FileOpen( const CString& filePath ) override;
    virtual void FileClose( void *hFile ) override;
    virtual size_t FileWrite( const void *pBuffer, size_t nBytes, void *hFile ) override;
    virtual size_t FileRead( void *pBuffer, size_t nBytes, void *hFile ) override;
    virtual size_t FileTell( void *hFile ) override;
    virtual size_t FileLength( void *hFile ) override;

    virtual void MutexLock( void *pMutex ) override;
    virtual void MutexUnlock( void *pMutex ) override;
};

namespace PlatformTypes {
    typedef pthread_mutex_t mutex_t;
    typedef pthread_cond_t condition_variable_t;
    typedef pthread_t thread_t;
    typedef int file_t;
};

#endif