#ifndef __POSIX_APPLICATION_H__
#define __POSIX_APPLICATION_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Core/FileSystem/FilePath.h>
#include <pthread.h>

class CPosixApplication : public IGenericApplication
{
public:
    CPosixApplication( void );
    virtual ~CPosixApplication() override;

    void Error( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));
    void Warning( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));
    void Log( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));

    virtual size_t GetOSPageSize( void ) const override;

    virtual FILE *OpenFile( const CString& filePath, const char *mode ) override;

    virtual void *OpenDLL( const char *pName ) override;
    virtual void CloseDLL( void *pDLLHandle ) override;
    virtual void *GetProcAddress( void *pDLLHandle, const char *pName ) override;

    virtual size_t GetAllocSize( void *pBuffer ) const override;
    virtual void *VirtualAlloc( size_t *nSize, size_t nAlignment ) override;
    virtual void VirtualFree( void *pBuffer ) override;
    virtual void CommitMemory( void *pMemory, size_t nOffset, size_t nSize ) override;
    virtual void DecommitMemory( void *pMemory, size_t nOffset, size_t nSize ) override;
    virtual void SetMemoryReadOnly( void *pMemory, size_t nOffset, size_t nSize ) override;

    virtual void *FileOpen( const CString& filePath, FileMode_t nMode ) override;
    virtual void FileClose( void *hFile ) override;
    virtual size_t FileWrite( const void *pBuffer, size_t nBytes, void *hFile ) override;
    virtual size_t FileRead( void *pBuffer, size_t nBytes, void *hFile ) override;
    virtual size_t FileTell( void *hFile ) override;
    virtual size_t FileLength( void *hFile ) override;

    virtual void MutexLock( void *pMutex ) override;
    virtual void MutexUnlock( void *pMutex ) override;
    virtual bool MutexTryLock( void *pMutex ) override;
    virtual void MutexInit( void *pMutex ) override;
    virtual void MutexShutdown( void *pMutex ) override;
    virtual void MutexRWUnlock( void *pMutex ) override;
    virtual void MutexWriteLock( void *pMutex ) override;
    virtual void MutexReadLock( void *pMutex ) override;
    virtual bool MutexRWTryReadLock( void *pMutex ) override;
    virtual bool MutexRWTryWriteLock( void *pMutex ) override;
    virtual void MutexRWInit( void *pMutex ) override;
    virtual void MutexRWShutdown( void *pMutex ) override;

    virtual void ConditionVarWait( void *pCoditionVariable, void *pMutex ) override;

    virtual void ConditionVarInit( void *pConditionVariable ) override;
    virtual void ConditionVarShutdown( void *pConditionVariable ) override;

    virtual void ThreadStart( void *pThread, ThreadFunc_t pFunction ) override;
    virtual void ThreadJoin( void *pThread, uint64_t nTimeout = SIRENGINE_UINT64_MAX ) override;

    virtual const CVector<FileSystem::CFilePath>& ListFiles( const FileSystem::CFilePath& dir, bool bDirectoryOnly = false ) override;

    virtual void OnOutOfMemory( void ) override;

    static void CatchSignal( int nSignum );

    static void *pOOMBackup;
    static size_t nOOMBackupSize;
private:
    bool32 m_bBacktraceError;

    void GetPwd( void );
    static void *ThreadFunction( void *pArgument );
};

namespace PlatformTypes {
    typedef pthread_mutex_t mutex_t;
    typedef pthread_rwlock_t rwlock_t;
    typedef pthread_cond_t condition_variable_t;
    typedef pthread_t thread_t;
    typedef int file_t;
};

extern CPosixApplication *g_pApplication;

#endif