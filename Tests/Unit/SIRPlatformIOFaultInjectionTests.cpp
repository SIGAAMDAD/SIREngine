/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <Core/Platform/SIRChildProcess.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Core/Platform/SIRPlatformFileMapping.h>
#include <Core/Platform/SIRPlatformPipe.h>
#include <Core/Platform/SIRSemaphore.h>

#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SIR_CHECK( condition )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

// GNU --wrap redirects only the test executable's linked references. No
// production hook, replacement backend, or engine allocation is introduced.
static std::atomic<int> s_nReadFault{ 0 };
static std::atomic<int> s_nWriteFault{ 0 };
static std::atomic<bool> s_bMapFault{ false };
static std::atomic<bool> s_bMapAccessFault{ false };
static std::atomic<bool> s_bSizeFault{ false };
static std::atomic<bool> s_bPageFault{ false };
static std::atomic<bool> s_bUnmapFault{ false };
static std::atomic<bool> s_bNegativeFileSize{ false };
static std::atomic<bool> s_bNegativeFileInfo{ false };
static std::atomic<bool> s_bSyncFileFault{ false };
static std::atomic<bool> s_bTruncateFault{ false };
static std::atomic<bool> s_bCloseFault{ false };
static std::atomic<int> s_nAppendWriteFault{ 0 };
static std::atomic<bool> s_bSyncFault{ false };
static std::atomic<bool> s_bPipeFault{ false };
static std::atomic<bool> s_bWaitFault{ false };
static std::atomic<bool> s_bTimedWaitFault{ false };
static std::atomic<bool> s_bSpawnFault{ false };

extern "C" ssize_t __real_pread( int nHandle, void *pData, size_t nBytes, off_t nOffset );
extern "C" ssize_t __real_pwrite( int nHandle, const void *pData, size_t nBytes, off_t nOffset );
extern "C" void *__real_mmap( void *pAddress, size_t nBytes, int nProtection, int nFlags, int nHandle, off_t nOffset );
extern "C" int __real_msync( void *pAddress, size_t nBytes, int nFlags );
extern "C" int __real_fstat( int nHandle, struct stat *pInfo );
extern "C" int __real_stat( const char *pszPath, struct stat *pInfo );
extern "C" int __real_fsync( int nHandle );
extern "C" int __real_ftruncate( int nHandle, off_t nSize );
extern "C" int __real_close( int nHandle );
extern "C" ssize_t __real_write( int nHandle, const void *pBytes, size_t nCount );
extern "C" long __real_sysconf( int nParameter );
extern "C" int __real_munmap( void *pAddress, size_t nBytes );
extern "C" int __real_pipe2( int *pHandles, int nFlags );
extern "C" int __real_sem_wait( sem_t *pSemaphore );
extern "C" int __real_sem_timedwait( sem_t *pSemaphore, const struct timespec *pDeadline );
extern "C" int __real_posix_spawnp( pid_t *pPid, const char *pszName, const posix_spawn_file_actions_t *pActions,
                                    const posix_spawnattr_t *pAttributes, char *const ppszArgs[], char *const ppszEnvironment[] );

extern "C" ssize_t __wrap_pread( int nHandle, void *pData, size_t nBytes, off_t nOffset )
{
	const int nMode = s_nReadFault.load( std::memory_order_relaxed );
	if ( nMode == 1 && s_nReadFault.exchange( 0, std::memory_order_relaxed ) == 1 ) {
		errno = EINTR;
		return -1;
	}
	if ( nMode == 2 ) {
		const size_t nShort = nBytes > 1 ? nBytes / 2 : 1;
		return __real_pread( nHandle, pData, nShort, nOffset );
	}
	if ( nMode == 3 ) {
		errno = EIO;
		return -1;
	}
	if ( nMode == 4 ) {
		return 0;
	}
	return __real_pread( nHandle, pData, nBytes, nOffset );
}

extern "C" ssize_t __wrap_pwrite( int nHandle, const void *pData, size_t nBytes, off_t nOffset )
{
	const int nMode = s_nWriteFault.load( std::memory_order_relaxed );
	if ( nMode == 1 && s_nWriteFault.exchange( 0, std::memory_order_relaxed ) == 1 ) {
		errno = EINTR;
		return -1;
	}
	if ( nMode == 2 ) {
		const size_t nShort = nBytes > 1 ? nBytes / 2 : 1;
		return __real_pwrite( nHandle, pData, nShort, nOffset );
	}
	if ( nMode == 3 ) {
		errno = ENOSPC;
		return -1;
	}
	if ( nMode == 4 ) {
		errno = EIO;
		return -1;
	}
	return __real_pwrite( nHandle, pData, nBytes, nOffset );
}

extern "C" int __wrap_fstat( int nHandle, struct stat *pInfo )
{
	if ( s_bSizeFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EIO;
		return -1;
	}
	const int nResult = __real_fstat( nHandle, pInfo );
	if ( nResult == 0 && s_bNegativeFileSize.exchange( false, std::memory_order_relaxed ) ) {
		pInfo->st_size = -1;
	}
	return nResult;
}

extern "C" int __wrap_stat( const char *pszPath, struct stat *pInfo )
{
	const int nResult = __real_stat( pszPath, pInfo );
	if ( nResult == 0 && s_bNegativeFileInfo.exchange( false, std::memory_order_relaxed ) ) {
		pInfo->st_size = -1;
		pInfo->st_mtim.tv_sec = -1;
	}
	return nResult;
}

extern "C" int __wrap_fsync( int nHandle )
{
	if ( s_bSyncFileFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EIO;
		return -1;
	}
	return __real_fsync( nHandle );
}

extern "C" int __wrap_ftruncate( int nHandle, off_t nSize )
{
	if ( s_bTruncateFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EINVAL;
		return -1;
	}
	return __real_ftruncate( nHandle, nSize );
}

extern "C" int __wrap_close( int nHandle )
{
	if ( s_bCloseFault.exchange( false, std::memory_order_relaxed ) ) {
		( void )__real_close( nHandle );
		errno = EIO;
		return -1;
	}
	return __real_close( nHandle );
}

extern "C" ssize_t __wrap_write( int nHandle, const void *pBytes, size_t nCount )
{
	const int nMode = s_nAppendWriteFault.load( std::memory_order_relaxed );
	if ( nMode == 1 && s_nAppendWriteFault.exchange( 0, std::memory_order_relaxed ) == 1 ) {
		errno = EINTR;
		return -1;
	}
	if ( nMode == 2 ) {
		errno = EIO;
		return -1;
	}
	if ( nMode == 3 ) {
		return 0;
	}
	if ( nMode == 4 ) {
		return __real_write( nHandle, pBytes, nCount > 1 ? nCount / 2 : nCount );
	}
	return __real_write( nHandle, pBytes, nCount );
}

extern "C" long __wrap_sysconf( int nParameter )
{
	if ( s_bPageFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EINVAL;
		return -1;
	}
	return __real_sysconf( nParameter );
}

extern "C" int __wrap_munmap( void *pAddress, size_t nBytes )
{
	if ( s_bUnmapFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EIO;
		return -1;
	}
	return __real_munmap( pAddress, nBytes );
}

extern "C" void *__wrap_mmap( void *pAddress, size_t nBytes, int nProtection, int nFlags, int nHandle, off_t nOffset )
{
	if ( s_bMapAccessFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EACCES;
		return MAP_FAILED;
	}
	if ( s_bMapFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = ENOMEM;
		return MAP_FAILED;
	}
	return __real_mmap( pAddress, nBytes, nProtection, nFlags, nHandle, nOffset );
}

extern "C" int __wrap_msync( void *pAddress, size_t nBytes, int nFlags )
{
	if ( s_bSyncFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EIO;
		return -1;
	}
	return __real_msync( pAddress, nBytes, nFlags );
}

extern "C" int __wrap_pipe2( int *pHandles, int nFlags )
{
	if ( s_bPipeFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EMFILE;
		return -1;
	}
	return __real_pipe2( pHandles, nFlags );
}

extern "C" int __wrap_sem_wait( sem_t *pSemaphore )
{
	if ( s_bWaitFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EINTR;
		return -1;
	}
	return __real_sem_wait( pSemaphore );
}

extern "C" int __wrap_sem_timedwait( sem_t *pSemaphore, const struct timespec *pDeadline )
{
	if ( s_bTimedWaitFault.exchange( false, std::memory_order_relaxed ) ) {
		errno = EINTR;
		return -1;
	}
	return __real_sem_timedwait( pSemaphore, pDeadline );
}

extern "C" int __wrap_posix_spawnp( pid_t *pPid, const char *pszName, const posix_spawn_file_actions_t *pActions,
                                    const posix_spawnattr_t *pAttributes, char *const ppszArgs[], char *const ppszEnvironment[] )
{
	if ( s_bSpawnFault.exchange( false, std::memory_order_relaxed ) ) {
		return ENOENT;
	}
	return __real_posix_spawnp( pPid, pszName, pActions, pAttributes, ppszArgs, ppszEnvironment );
}

static void sirTestInjectedPositionalTransfers( void )
{
	const char *pszPath = "SIRPlatformIOFaultInjection.bin";
	( void )SIRPlatformFile::remove( pszPath );
	SIRPlatformFile file;
	SIR_CHECK( file.open( pszPath, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE | SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE ) ==
	           SIR_FILE_OK );

	uint8_t szExpected[4096];
	uint8_t szActual[4096] = {};
	for ( uint32_t iByte = 0; iByte < sizeof( szExpected ); ++iByte ) {
		szExpected[iByte] = ( uint8_t )( iByte * 29 + ( iByte >> 4 ) );
	}
	uint64_t nTransferred = UINT64_MAX;
	s_nWriteFault.store( 1, std::memory_order_relaxed );
	SIR_CHECK( file.writeAt( 0, szExpected, sizeof( szExpected ), &nTransferred ) == SIR_FILE_OK );
	SIR_CHECK( nTransferred == sizeof( szExpected ) );
	s_nReadFault.store( 1, std::memory_order_relaxed );
	SIR_CHECK( file.readAt( 0, szActual, sizeof( szActual ), &nTransferred ) == SIR_FILE_OK );
	SIR_CHECK( nTransferred == sizeof( szActual ) && memcmp( szActual, szExpected, sizeof( szActual ) ) == 0 );

	s_nWriteFault.store( 2, std::memory_order_relaxed );
	SIR_CHECK( file.writeAt( 17, szExpected, sizeof( szExpected ) - 17, &nTransferred ) == SIR_FILE_OK );
	SIR_CHECK( nTransferred == sizeof( szExpected ) - 17 );
	s_nWriteFault.store( 0, std::memory_order_relaxed );
	s_nReadFault.store( 2, std::memory_order_relaxed );
	SIR_CHECK( file.readAt( 17, szActual, sizeof( szActual ) - 17, &nTransferred ) == SIR_FILE_OK );
	SIR_CHECK( nTransferred == sizeof( szActual ) - 17 );
	SIR_CHECK( memcmp( szActual, szExpected, sizeof( szActual ) - 17 ) == 0 );
	s_nReadFault.store( 0, std::memory_order_relaxed );

	s_nReadFault.store( 3, std::memory_order_relaxed );
	nTransferred = UINT64_MAX;
	SIR_CHECK( file.readAt( 0, szActual, 16, &nTransferred ) == SIR_FILE_IO_ERROR && nTransferred == 0 );
	s_nReadFault.store( 4, std::memory_order_relaxed );
	SIR_CHECK( file.readAt( 0, szActual, 16, &nTransferred ) == SIR_FILE_END_OF_FILE && nTransferred == 0 );
	s_nReadFault.store( 0, std::memory_order_relaxed );

	s_nWriteFault.store( 3, std::memory_order_relaxed );
	SIR_CHECK( file.writeAt( 0, szExpected, 16, &nTransferred ) == SIR_FILE_NO_SPACE && nTransferred == 0 );
	s_nWriteFault.store( 4, std::memory_order_relaxed );
	SIR_CHECK( file.writeAt( 0, szExpected, 16, &nTransferred ) == SIR_FILE_IO_ERROR && nTransferred == 0 );
	s_nWriteFault.store( 0, std::memory_order_relaxed );

	SIRPlatformFileMapping mapping;
	s_bSizeFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( mapping.map( file, 1, 128, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_IO_ERROR && !mapping.isMapped() );
	s_bPageFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( mapping.map( file, 1, 128, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_IO_ERROR && !mapping.isMapped() );
	s_bMapAccessFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( mapping.map( file, 1, 128, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_ACCESS_DENIED && !mapping.isMapped() );
	s_bMapFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( mapping.map( file, 1, 128, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_IO_ERROR && !mapping.isMapped() );
	SIR_CHECK( mapping.map( file, 1, 128, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_OK );
	mapping.getWritableData()[0] = 0xA7;
	s_bSyncFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( mapping.flush() == SIR_FILE_IO_ERROR && mapping.isMapped() );
	SIR_CHECK( mapping.flush() == SIR_FILE_OK );
	s_bUnmapFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( mapping.unmap() == SIR_FILE_IO_ERROR && mapping.isMapped() );
	SIR_CHECK( mapping.unmap() == SIR_FILE_OK );
	SIR_CHECK( file.readAt( 1, szActual, 1, &nTransferred ) == SIR_FILE_OK && szActual[0] == 0xA7 );
	s_bNegativeFileSize.store( true, std::memory_order_relaxed );
	uint64_t nInvalidSize = 0;
	SIR_CHECK( file.getSize( &nInvalidSize ) == SIR_FILE_OUT_OF_RANGE );
	s_bNegativeFileInfo.store( true, std::memory_order_relaxed );
	sirFileInfo_t invalidInfo = {};
	SIR_CHECK( SIRPlatformFile::getInfo( pszPath, &invalidInfo ) == SIR_FILE_OK );
	SIR_CHECK( invalidInfo.nSizeBytes == 0 && invalidInfo.nModifiedUnixNanoseconds == 0 );
	SIR_CHECK( file.close() == SIR_FILE_OK );
	SIR_CHECK( file.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_APPEND ) == SIR_FILE_OK );
	uint8_t szAppend[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	SIR_CHECK( file.write( szAppend, 8, nullptr ) == SIR_FILE_INVALID_ARGUMENT );
	s_nAppendWriteFault.store( 1, std::memory_order_relaxed );
	SIR_CHECK( file.write( szAppend, 8, &nTransferred ) == SIR_FILE_OK && nTransferred == 8 );
	s_nAppendWriteFault.store( 2, std::memory_order_relaxed );
	SIR_CHECK( file.write( szAppend, 8, &nTransferred ) == SIR_FILE_IO_ERROR && nTransferred == 0 );
	s_nAppendWriteFault.store( 3, std::memory_order_relaxed );
	SIR_CHECK( file.write( szAppend, 8, &nTransferred ) == SIR_FILE_IO_ERROR && nTransferred == 0 );
	s_nAppendWriteFault.store( 4, std::memory_order_relaxed );
	SIR_CHECK( file.write( szAppend, 8, &nTransferred ) == SIR_FILE_OK && nTransferred == 8 );
	s_nAppendWriteFault.store( 0, std::memory_order_relaxed );
	s_bSyncFileFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( file.flush() == SIR_FILE_IO_ERROR );
	s_bTruncateFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( file.truncate( 8 ) == SIR_FILE_INVALID_ARGUMENT );
	s_bCloseFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( file.close() == SIR_FILE_IO_ERROR && !file.isOpen() );
	SIR_CHECK( SIRPlatformFile::renameReplace( "SIRFileMissingRenameSource.bin", "SIRFileMissingRenameDest.bin" ) == SIR_FILE_NOT_FOUND );


	SIR_CHECK( SIRPlatformFile::remove( pszPath ) == SIR_FILE_OK );
}

static void sirTestInjectedOSLifecycles( void )
{
	SIRPlatformPipe pipe;
	s_bPipeFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( pipe.create() == SIR_PIPE_IO_ERROR );
	SIR_CHECK( !pipe.isReadOpen() && !pipe.isWriteOpen() );
	SIR_CHECK( pipe.create() == SIR_PIPE_OK );
	SIR_CHECK( pipe.close() == SIR_PIPE_OK );

	SIRSemaphore semaphore;
	SIR_CHECK( semaphore.create( 0 ) == SIR_SEMAPHORE_OK );
	SIR_CHECK( semaphore.post() == SIR_SEMAPHORE_OK );
	s_bWaitFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( semaphore.wait() == SIR_SEMAPHORE_OK );
	SIR_CHECK( semaphore.post() == SIR_SEMAPHORE_OK );
	s_bTimedWaitFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( semaphore.waitFor( 100 ) == SIR_SEMAPHORE_OK );
	SIR_CHECK( semaphore.tryWait() == SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_CHECK( semaphore.close() == SIR_SEMAPHORE_OK );

	SIRChildProcess child;
	const sirChildProcessLaunch_t launch = { "/bin/true", nullptr, 0, nullptr };
	s_bSpawnFault.store( true, std::memory_order_relaxed );
	SIR_CHECK( child.start( launch ) == SIR_CHILD_PROCESS_SPAWN_FAILED );
	SIR_CHECK( !child.isOpen() && child.getProcessId() == 0 );
	SIR_CHECK( child.start( launch ) == SIR_CHILD_PROCESS_OK );
	uint32_t nExitCode = UINT32_MAX;
	SIR_CHECK( child.waitFor( 10000, &nExitCode ) == SIR_CHILD_PROCESS_OK && nExitCode == 0 );
	SIR_CHECK( child.close() == SIR_CHILD_PROCESS_OK );
}

int main( void )
{
	sirTestInjectedPositionalTransfers();
	sirTestInjectedOSLifecycles();
	return 0;
}
