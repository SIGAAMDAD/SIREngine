/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/VFS/SIRVFSDirectorySource.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static void sirVFSDirectoryRequire( bool bValid, const char *pszFailure )
{
	if ( !bValid ) {
		fprintf( stderr, "VFS directory source: %s\n", pszFailure );
		abort();
	}
}

static sirVFSPath_t sirVFSDirectoryPath( const char *pszInput )
{
	sirVFSPath_t path = {};
	sirVFSDirectoryRequire( SIRVFSPath::normalize( pszInput, ( uint32_t )strlen( pszInput ), &path ) == SIR_VFS_PATH_OK, "path fixture" );
	return path;
}

static sirVFSMountDescriptor_t sirVFSDirectoryDescriptor( uint64_t nSourceId, uint64_t nSourceRevision )
{
	sirVFSMountDescriptor_t descriptor = {};
	strcpy( descriptor.szNamespace, "game" );
	descriptor.nSourceId = nSourceId;
	descriptor.nSourceRevision = nSourceRevision;
	descriptor.nOwnerScope = 77;
	descriptor.nAllowedOps = SIR_VFS_ACCESS_READ;
	descriptor.eKind = SIR_VFS_BACKEND_DIRECTORY;
	descriptor.bTrusted = true;
	return descriptor;
}

static void sirVFSDirectoryWriteFixture( const char *pszFilename )
{
	FILE *pFile = fopen( pszFilename, "wb" );
	sirVFSDirectoryRequire( pFile != nullptr, "fixture fopen" );
	sirVFSDirectoryRequire( fwrite( "DIRECTORY", 1, 9, pFile ) == 9, "fixture fwrite" );
	sirVFSDirectoryRequire( fclose( pFile ) == 0, "fixture fclose" );
}

int main( void )
{
	char szBase[768] = {};
#if defined( _WIN32 )
	char szTemp[512] = {};
	const DWORD nLength = GetTempPathA( sizeof( szTemp ), szTemp );
	sirVFSDirectoryRequire( nLength != 0 && nLength < sizeof( szTemp ), "windows temp path" );
	snprintf( szBase, sizeof( szBase ), "%ssir_vfs_directory_%lu", szTemp, ( unsigned long )GetCurrentProcessId() );
	sirVFSDirectoryRequire( CreateDirectoryA( szBase, nullptr ), "windows mkdir" );
#else
	strcpy( szBase, "/tmp/sir_vfs_directory_XXXXXX" );
	sirVFSDirectoryRequire( mkdtemp( szBase ) != nullptr, "mkdtemp" );
#endif
	char szNested[896] = {};
	char szFilename[896] = {};
	snprintf( szNested, sizeof( szNested ), "%s/assets", szBase );
	snprintf( szFilename, sizeof( szFilename ), "%s/assets/object.bin", szBase );
#if defined( _WIN32 )
	sirVFSDirectoryRequire( CreateDirectoryA( szNested, nullptr ), "windows assets mkdir" );
#else
	sirVFSDirectoryRequire( mkdir( szNested, 0700 ) == 0, "assets mkdir" );
#endif
	sirVFSDirectoryWriteFixture( szFilename );

	SIRVFSMountTable mounts( 2, 12 );
	const sirVFSMountDescriptor_t descriptor = sirVFSDirectoryDescriptor( 123, 7 );
	sirVFSMountHandle_t mount = {};
	sirVFSDirectoryRequire( mounts.mount( descriptor, &mount ) == SIR_VFS_OK, "mount directory descriptor" );
	const sirVFSPath_t path = sirVFSDirectoryPath( "game:/assets/object.bin" );
	sirVFSCandidateList_t candidates = {};
	sirVFSDirectoryRequire( mounts.acquireCandidates( path, SIR_VFS_ACCESS_READ, &candidates ) == SIR_VFS_OK && candidates.nCount == 1,
	                        "acquire directory lease" );
	SIRVFSFileHandleTable files( mounts, 3, 3 );
	sirVFSFileHandle_t handle = {};
	sirVFSDirectoryRequire( files.openResolved( path, candidates.szCandidates[0].lease, &handle ) == SIR_VFS_OK, "open logical handle" );
	sirVFSDirectoryRequire( mounts.release( candidates.szCandidates[0].lease ) == SIR_VFS_OK, "release lookup lease" );
	sirVFSFileView_t borrowed = {};
	sirVFSDirectoryRequire( files.borrow( handle, &borrowed ) == SIR_VFS_OK, "borrow logical handle" );

	SIRVFSDirectorySource source;
	sirVFSMountDescriptor_t wrongKind = descriptor;
	wrongKind.eKind = SIR_VFS_BACKEND_MEMORY;
	sirVFSDirectoryRequire( source.bind( szBase, wrongKind ) == SIR_VFS_INVALID_ARGUMENT, "reject wrong backend kind" );
	sirVFSDirectoryRequire( source.bind( szBase, descriptor ) == SIR_VFS_OK, "bind root and source generation" );
	sirVFSDirectoryRequire( source.isOpen(), "root opened" );
	sirVFSDirectoryRequire( source.bind( szBase, descriptor ) == SIR_VFS_STOPPED, "reject binding same source twice" );
	SIRPlatformFile native;
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_OK, "open trusted borrowed path" );
	char szContent[10] = {};
	uint64_t nTransferred = 0;
	sirVFSDirectoryRequire( native.readAt( 0, szContent, 9, &nTransferred ) == SIR_FILE_OK && nTransferred == 9, "physical read" );
	sirVFSDirectoryRequire( strcmp( szContent, "DIRECTORY" ) == 0, "read expected bytes" );
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_INVALID_ARGUMENT, "reject already open output" );
	sirVFSDirectoryRequire( native.close() == SIR_FILE_OK, "native close" );

	SIRVFSDirectorySource wrong;
	sirVFSMountDescriptor_t otherGeneration = descriptor;
	otherGeneration.nSourceRevision++;
	sirVFSDirectoryRequire( wrong.bind( szBase, otherGeneration ) == SIR_VFS_OK, "bind distinct physical generation" );
	sirVFSDirectoryRequire( wrong.openRead( files, borrowed.borrow, &native ) == SIR_VFS_FORBIDDEN, "reject wrong physical generation" );
	sirVFSDirectoryRequire( !native.isOpen(), "wrong source did not modify output" );
	sirVFSDirectoryRequire( wrong.close() == SIR_VFS_OK, "close wrong source" );

	sirVFSFileView_t cloned = {};
	sirVFSDirectoryRequire( files.retainBorrow( borrowed.borrow, &cloned ) == SIR_VFS_OK, "borrow ticket clone" );
	sirVFSDirectoryRequire( cloned.borrow.nToken != borrowed.borrow.nToken, "unique cloned ticket" );
	sirVFSDirectoryRequire( files.releaseBorrow( cloned.borrow ) == SIR_VFS_OK, "release clone" );
	sirVFSDirectoryRequire( files.releaseBorrow( cloned.borrow ) == SIR_VFS_STALE_HANDLE, "reject duplicate release" );

	sirVFSDirectoryRequire( files.close( handle ) == SIR_VFS_OK, "close logical handle with borrowed view" );
	sirVFSDirectoryRequire( mounts.unmount( mount ) == SIR_VFS_OK, "unmount while borrow retained" );
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_OK, "open after retirement via pinned view" );
	sirVFSDirectoryRequire( files.releaseBorrow( borrowed.borrow ) == SIR_VFS_OK, "release final original borrow" );
	sirVFSDirectoryRequire( native.readAt( 0, szContent, 9, &nTransferred ) == SIR_FILE_OK, "native handle independent of VFS lease" );
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_INVALID_ARGUMENT, "reject open output file" );
	sirVFSDirectoryRequire( native.close() == SIR_FILE_OK, "native file cleanup" );
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_STALE_HANDLE, "reject released view" );
	sirVFSDirectoryRequire( source.close() == SIR_VFS_OK, "close bound source" );
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_STOPPED, "closed source rejects lookup" );
	sirVFSDirectoryRequire( source.bind( szBase, descriptor ) == SIR_VFS_STOPPED, "closed source cannot rebind generation" );
	sirVFSDirectoryRequire( files.shutdown() == SIR_VFS_OK, "clean logical shutdown" );

	SIRVFSMountTable capMounts( 1, 4 );
	sirVFSMountHandle_t capMount = {};
	sirVFSDirectoryRequire( capMounts.mount( descriptor, &capMount ) == SIR_VFS_OK, "capacity mount" );
	sirVFSCandidateList_t capCandidates = {};
	sirVFSDirectoryRequire( capMounts.acquireCandidates( path, SIR_VFS_ACCESS_READ, &capCandidates ) == SIR_VFS_OK,
	                        "capacity source lease" );
	SIRVFSFileHandleTable capacityFiles( capMounts, 1, 1 );
	sirVFSFileHandle_t capHandle = {};
	sirVFSDirectoryRequire( capacityFiles.openResolved( path, capCandidates.szCandidates[0].lease, &capHandle ) == SIR_VFS_OK,
	                        "capacity open" );
	sirVFSDirectoryRequire( capMounts.release( capCandidates.szCandidates[0].lease ) == SIR_VFS_OK, "capacity lookup release" );
	sirVFSFileView_t capBorrow = {};
	sirVFSDirectoryRequire( capacityFiles.borrow( capHandle, &capBorrow ) == SIR_VFS_OK, "capacity first borrow" );
	sirVFSFileView_t blocked = {};
	blocked.descriptor.nSourceId = 919;
	sirVFSDirectoryRequire( capacityFiles.retainBorrow( capBorrow.borrow, &blocked ) == SIR_VFS_NO_CAPACITY,
	                        "cannot clone beyond ticket capacity" );
	sirVFSDirectoryRequire( blocked.descriptor.nSourceId == 919, "rejection is output-atomic" );
	SIRVFSDirectorySource capSource;
	sirVFSDirectoryRequire( capSource.bind( szBase, descriptor ) == SIR_VFS_OK, "capacity source bind" );
	sirVFSDirectoryRequire( capSource.openRead( capacityFiles, capBorrow.borrow, &native ) == SIR_VFS_NO_CAPACITY,
	                        "native open requires independent ticket" );
	sirVFSDirectoryRequire( !native.isOpen(), "capacity rejection leaves output closed" );
	sirVFSDirectoryRequire( capacityFiles.releaseBorrow( capBorrow.borrow ) == SIR_VFS_OK, "capacity release" );
	sirVFSDirectoryRequire( capacityFiles.close( capHandle ) == SIR_VFS_OK, "capacity close" );
	sirVFSDirectoryRequire( capacityFiles.shutdown() == SIR_VFS_OK, "capacity shutdown" );
	sirVFSDirectoryRequire( capMounts.unmount( capMount ) == SIR_VFS_OK, "capacity unmount" );

#if defined( _WIN32 )
	DeleteFileA( szFilename );
	RemoveDirectoryA( szNested );
	RemoveDirectoryA( szBase );
#else
	unlink( szFilename );
	rmdir( szNested );
	rmdir( szBase );
#endif
	return 0;
}
