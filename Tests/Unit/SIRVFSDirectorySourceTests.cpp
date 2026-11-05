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
	sirVFSFileView_t borrowed = {};
	sirVFSDirectoryRequire( files.borrow( handle, &borrowed ) == SIR_VFS_OK, "borrow logical handle" );

	SIRVFSMountTable memoryMounts( 1, 4 );
	sirVFSMountDescriptor_t memoryDescriptor = descriptor;
	memoryDescriptor.eKind = SIR_VFS_BACKEND_MEMORY;
	sirVFSMountHandle_t memoryMount = {};
	sirVFSDirectoryRequire( memoryMounts.mount( memoryDescriptor, &memoryMount ) == SIR_VFS_OK, "memory mount" );
	sirVFSCandidateList_t memoryCandidates = {};
	sirVFSDirectoryRequire( memoryMounts.acquireCandidates( path, SIR_VFS_ACCESS_READ, &memoryCandidates ) == SIR_VFS_OK,
	                        "memory candidate" );
	SIRVFSDirectorySource rejectedKind;
	sirVFSDirectoryRequire( rejectedKind.bind( memoryMounts, memoryCandidates.szCandidates[0].lease, szBase ) == SIR_VFS_FORBIDDEN,
	                        "reject non-directory leased backend" );
	sirVFSDirectoryRequire( memoryMounts.release( memoryCandidates.szCandidates[0].lease ) == SIR_VFS_OK, "memory lease cleanup" );
	sirVFSDirectoryRequire( memoryMounts.unmount( memoryMount ) == SIR_VFS_OK, "memory mount cleanup" );

	SIRVFSDirectorySource source;
	sirVFSMountLease_t forged = candidates.szCandidates[0].lease;
	forged.nToken += 1000;
	sirVFSDirectoryRequire( source.bind( mounts, forged, szBase ) == SIR_VFS_STALE_HANDLE, "deny forged mount lease" );
	sirVFSDirectoryRequire( source.bind( mounts, candidates.szCandidates[0].lease, szBase ) == SIR_VFS_OK,
	                        "bind root to authenticated mount generation" );
	sirVFSDirectoryRequire( mounts.release( candidates.szCandidates[0].lease ) == SIR_VFS_OK, "release lookup lease" );
	sirVFSDirectoryRequire( borrowed.sourceMount.nSlot == mount.nSlot && borrowed.sourceMount.nGeneration == mount.nGeneration,
	                        "borrow carries authenticated mount identity" );
	sirVFSDirectoryRequire( source.isOpen(), "root opened" );
	sirVFSDirectoryRequire( source.bind( mounts, forged, szBase ) == SIR_VFS_STOPPED, "reject binding same source twice" );
	SIRPlatformFile native;
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_OK, "open trusted borrowed path" );
	char szContent[10] = {};
	uint64_t nTransferred = 0;
	sirVFSDirectoryRequire( native.readAt( 0, szContent, 9, &nTransferred ) == SIR_FILE_OK && nTransferred == 9, "physical read" );
	sirVFSDirectoryRequire( strcmp( szContent, "DIRECTORY" ) == 0, "read expected bytes" );
	sirVFSDirectoryRequire( source.openRead( files, borrowed.borrow, &native ) == SIR_VFS_INVALID_ARGUMENT, "reject already open output" );
	sirVFSDirectoryRequire( native.close() == SIR_FILE_OK, "native close" );

	// A second mount can deliberately use identical public descriptor fields.
	// The directory source must still enforce the actual slot/generation.
	sirVFSMountHandle_t duplicateMount = {};
	sirVFSDirectoryRequire( mounts.mount( descriptor, &duplicateMount ) == SIR_VFS_OK, "mount identical second source" );
	sirVFSCandidateList_t duplicates = {};
	sirVFSDirectoryRequire( mounts.acquireCandidates( path, SIR_VFS_ACCESS_READ, &duplicates ) == SIR_VFS_OK && duplicates.nCount == 2,
	                        "acquire both identical source descriptors" );
	SIRVFSDirectorySource wrong;
	sirVFSDirectoryRequire( wrong.bind( mounts, duplicates.szCandidates[1].lease, szBase ) == SIR_VFS_OK, "bind second mount generation" );
	sirVFSDirectoryRequire( wrong.openRead( files, borrowed.borrow, &native ) == SIR_VFS_FORBIDDEN,
	                        "deny identical descriptor from different mount slot" );
	sirVFSDirectoryRequire( !native.isOpen(), "wrong source did not modify output" );
	for ( uint32_t i = 0; i < duplicates.nCount; ++i ) {
		sirVFSDirectoryRequire( mounts.release( duplicates.szCandidates[i].lease ) == SIR_VFS_OK, "release duplicate candidate" );
	}
	sirVFSDirectoryRequire( wrong.close() == SIR_VFS_OK, "close wrong source" );
	sirVFSDirectoryRequire( mounts.unmount( duplicateMount ) == SIR_VFS_OK, "unmount duplicate source" );

	// Two independently allocated tables can generate the same slot and generation.
	// A provider bound to the other table must reject this caller's file handle.
	SIRVFSMountTable foreignMounts( 1, 4 );
	sirVFSMountHandle_t foreignMount = {};
	sirVFSDirectoryRequire( foreignMounts.mount( descriptor, &foreignMount ) == SIR_VFS_OK, "foreign identical mount" );
	sirVFSCandidateList_t foreignCandidates = {};
	sirVFSDirectoryRequire( foreignMounts.acquireCandidates( path, SIR_VFS_ACCESS_READ, &foreignCandidates ) == SIR_VFS_OK,
	                        "foreign candidate" );
	SIRVFSDirectorySource foreign;
	sirVFSDirectoryRequire( foreign.bind( foreignMounts, foreignCandidates.szCandidates[0].lease, szBase ) == SIR_VFS_OK,
	                        "foreign source bind" );
	sirVFSDirectoryRequire( foreignMounts.release( foreignCandidates.szCandidates[0].lease ) == SIR_VFS_OK, "foreign lookup cleanup" );
	sirVFSDirectoryRequire( foreign.openRead( files, borrowed.borrow, &native ) == SIR_VFS_FORBIDDEN,
	                        "reject identical numeric slot in different mount table" );
	sirVFSDirectoryRequire( foreign.close() == SIR_VFS_OK, "foreign source close" );
	sirVFSDirectoryRequire( foreignMounts.unmount( foreignMount ) == SIR_VFS_OK, "foreign unmount" );

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
	sirVFSDirectoryRequire( source.bind( mounts, forged, szBase ) == SIR_VFS_STOPPED, "closed source cannot rebind generation" );
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
	SIRVFSDirectorySource capSource;
	sirVFSDirectoryRequire( capSource.bind( capMounts, capCandidates.szCandidates[0].lease, szBase ) == SIR_VFS_OK,
	                        "capacity source bind" );
	sirVFSDirectoryRequire( capMounts.release( capCandidates.szCandidates[0].lease ) == SIR_VFS_OK, "capacity lookup release" );
	sirVFSFileView_t capBorrow = {};
	sirVFSDirectoryRequire( capacityFiles.borrow( capHandle, &capBorrow ) == SIR_VFS_OK, "capacity first borrow" );
	sirVFSFileView_t blocked = {};
	blocked.descriptor.nSourceId = 919;
	sirVFSDirectoryRequire( capacityFiles.retainBorrow( capBorrow.borrow, &blocked ) == SIR_VFS_NO_CAPACITY,
	                        "cannot clone beyond ticket capacity" );
	sirVFSDirectoryRequire( blocked.descriptor.nSourceId == 919, "rejection is output-atomic" );
	sirVFSDirectoryRequire( capSource.openRead( capacityFiles, capBorrow.borrow, &native ) == SIR_VFS_NO_CAPACITY,
	                        "native open requires independent ticket" );
	sirVFSDirectoryRequire( !native.isOpen(), "capacity rejection leaves output closed" );
	sirVFSDirectoryRequire( capacityFiles.releaseBorrow( capBorrow.borrow ) == SIR_VFS_OK, "capacity release" );
	sirVFSDirectoryRequire( capacityFiles.close( capHandle ) == SIR_VFS_OK, "capacity close" );
	sirVFSDirectoryRequire( capacityFiles.shutdown() == SIR_VFS_OK, "capacity shutdown" );
	sirVFSDirectoryRequire( capMounts.unmount( capMount ) == SIR_VFS_OK, "capacity unmount" );
	sirVFSDirectoryRequire( capSource.close() == SIR_VFS_OK, "release capacity root source pin" );

	// A bound root retains its source generation even without an open file.
	SIRVFSMountTable pinMounts( 1, 3 );
	sirVFSMountHandle_t pinMount = {};
	sirVFSDirectoryRequire( pinMounts.mount( descriptor, &pinMount ) == SIR_VFS_OK, "independent pinned mount" );
	sirVFSCandidateList_t pinCandidates = {};
	sirVFSDirectoryRequire( pinMounts.acquireCandidates( path, SIR_VFS_ACCESS_READ, &pinCandidates ) == SIR_VFS_OK, "root source lookup" );
	SIRVFSDirectorySource pinned;
	sirVFSDirectoryRequire( pinned.bind( pinMounts, pinCandidates.szCandidates[0].lease, szBase ) == SIR_VFS_OK,
	                        "root source independently retained" );
	sirVFSDirectoryRequire( pinMounts.release( pinCandidates.szCandidates[0].lease ) == SIR_VFS_OK, "release root lookup lease" );
	sirVFSDirectoryRequire( pinMounts.unmount( pinMount ) == SIR_VFS_OK, "retire pinned mount" );
	sirVFSMountHandle_t replacement = {};
	sirVFSDirectoryRequire( pinMounts.mount( descriptor, &replacement ) == SIR_VFS_NO_CAPACITY, "root pin prevents generation slot reuse" );
	sirVFSDirectoryRequire( pinned.close() == SIR_VFS_OK, "root close releases last source pin" );
	sirVFSDirectoryRequire( pinMounts.mount( descriptor, &replacement ) == SIR_VFS_OK, "mount slot reopens after root release" );
	sirVFSDirectoryRequire( replacement.nGeneration != pinMount.nGeneration, "reopened mount has new generation" );
	sirVFSDirectoryRequire( pinMounts.unmount( replacement ) == SIR_VFS_OK, "replacement cleanup" );

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
