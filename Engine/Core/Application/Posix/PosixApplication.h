#ifndef __POSIX_APPLICATION_H__
#define __POSIX_APPLICATION_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#ifndef PLATFORM_HEADER
#define PLATFORM_HEADER
#endif

#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>
#include <Engine/Core/FileSystem/FilePath.h>

#include <pthread.h>

#if defined(SIRENGINE_PLATFORM_LINUX)
#include <sys/prctl.h>
#include <signal.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#elif defined(SIRENGINE_PLATFORM_APPLE) || defined(SIRENGINE_PLATFORM_BSD)
#include <signal.h>
#endif

namespace SIREngine::Application {

class CPosixApplication : public IGenericApplication
{
public:
	CPosixApplication( void );
	virtual ~CPosixApplication() override;

	virtual void Error( const char *pError ) override;

	virtual size_t GetOSPageSize( void ) const override;

	virtual FILE *OpenFile( const CString& filePath, const char *mode ) override;

	virtual void *OpenDLL( const char *pName ) override;
	virtual void CloseDLL( void *pDLLHandle ) override;
	virtual void *GetProcAddress( void *pDLLHandle, const char *pName ) override;

	virtual void *MapFile( void *hFile, size_t *pSize ) override;
	virtual void UnmapFile( void *pMemory, size_t nSize ) override;

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
	virtual size_t FileSeek( void *hFile, size_t nOffset, int whence ) override;
	virtual size_t FileTell( void *hFile ) override;
	virtual size_t FileLength( void *hFile ) override;

	virtual void ThreadStart( void *pThread, CThread *pObject, void (CThread::*pFunction)( void ) ) override;
	virtual void ThreadJoin( void *pThread, CThread *pObject, uint64_t nTimeout = SIRENGINE_UINT64_MAX ) override;

	virtual bool CreateDirectory( const char *pDirectoryPath ) override;

	virtual CVector<FileSystem::CFilePath> ListFiles( const FileSystem::CFilePath& dir, bool bDirectoryOnly = false ) override;

	virtual void OnOutOfMemory( void ) override;

	virtual double GetCPUFrequency( void ) override;
	virtual uint32_t GetNumberOfCores( void ) override;
	CString GetGPUBrand( void );
	
	bool IsRunningOnChromiumOS( void ) const;
	uint32_t GetNumberOfCoresIncludingHyperThreading( void );

	static void CatchSignal( int nSignum, siginfo_t *pSigInfo, void *pContext );

	static void *pOOMBackup;
	static size_t nOOMBackupSize;
private:
	bool32 m_bBacktraceError;
	
	void GetPwd( void );
	static void *ThreadFunction( void *pArgument );
};

};

#endif