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

#include <Core/VFS/SIRVFSFileHandleTable.h>
#include <Core/VFS/SIRVFSOverlayResolver.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

static void sirVFSRequire( bool bResult, const char *pszReason )
{
	if ( !bResult ) {
		fprintf( stderr, "VFS file handle test failed: %s\n", pszReason );
		abort();
	}
}

static sirVFSPath_t sirVFSTestPath( const char *pszInput )
{
	sirVFSPath_t path = {};
	sirVFSRequire( SIRVFSPath::normalize( pszInput, ( uint32_t )strlen( pszInput ), &path ) == SIR_VFS_PATH_OK, "normalize fixture" );
	return path;
}

static sirVFSMountHandle_t sirVFSTestMount( SIRVFSMountTable& table, const char *pszNamespace, uint64_t nSource )
{
	sirVFSMountDescriptor_t desc = {};
	strcpy( desc.szNamespace, pszNamespace );
	desc.eKind = SIR_VFS_BACKEND_MEMORY;
	desc.nSourceId = nSource;
	desc.nSourceRevision = nSource + 100;
	desc.nAllowedOps = SIR_VFS_ACCESS_READ;
	desc.bTrusted = true;
	sirVFSMountHandle_t handle = {};
	sirVFSRequire( table.mount( desc, &handle ) == SIR_VFS_OK, "mount fixture" );
	return handle;
}

static sirVFSMountLease_t sirVFSTestAcquire( SIRVFSMountTable& table, const sirVFSPath_t& path )
{
	sirVFSCandidateList_t candidates = {};
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &candidates ) == SIR_VFS_OK, "acquire fixture" );
	sirVFSRequire( candidates.nCount == 1, "fixture one candidate" );
	return candidates.szCandidates[0].lease;
}

static sirVFSProbeResult_t sirVFSTestPresentProbe( const sirVFSMountDescriptor_t *pDescriptor, const sirVFSPath_t *pPath,
                                                   void *pContext ) noexcept
{
	const uint64_t nExpectedSource = *( const uint64_t * )pContext;
	if ( pPath->nLength == 0 || pDescriptor->nSourceId != nExpectedSource ) {
		return SIR_VFS_PROBE_ERROR;
	}
	return SIR_VFS_PROBE_PRESENT;
}

static void sirVFSTestSourcePinAndRetirement( void )
{
	SIRVFSMountTable mounts( 1, 6 );
	const sirVFSMountHandle_t original = sirVFSTestMount( mounts, "game", 19 );
	const sirVFSPath_t path = sirVFSTestPath( "game:/world/rock" );
	SIRVFSOverlayResolver resolver( mounts );
	uint64_t nExpectedSource = 19;
	sirVFSMountCandidate_t chosen = {};
	sirVFSRequire( resolver.resolveRead( path, sirVFSTestPresentProbe, &nExpectedSource, &chosen ) == SIR_VFS_OK,
	               "overlay resolver to handle table" );
	const sirVFSMountLease_t lookup = chosen.lease;
	SIRVFSFileHandleTable files( mounts, 1, 2 );
	sirVFSFileHandle_t handle = {};
	sirVFSRequire( files.openResolved( path, lookup, &handle ) == SIR_VFS_OK, "open duplicates mount source pin" );
	sirVFSRequire( mounts.release( lookup ) == SIR_VFS_OK, "lookup lease independent" );
	sirVFSRequire( files.openResolved( path, lookup, &handle ) == SIR_VFS_NO_CAPACITY, "full handle table" );
	sirVFSFileView_t first = {}, second = {};
	sirVFSRequire( files.borrow( handle, &first ) == SIR_VFS_OK, "first borrow" );
	sirVFSRequire( files.borrow( handle, &second ) == SIR_VFS_OK, "second borrow" );
	sirVFSRequire( first.borrow.nToken != second.borrow.nToken, "unique borrow tickets" );
	sirVFSFileView_t failed = {};
	failed.descriptor.nSourceId = 989;
	sirVFSRequire( files.borrow( handle, &failed ) == SIR_VFS_NO_CAPACITY, "borrow capacity" );
	sirVFSRequire( failed.descriptor.nSourceId == 989, "borrow failure atomic" );
	sirVFSRequire( mounts.unmount( original ) == SIR_VFS_OK, "unmount while file pinned" );
	sirVFSMountHandle_t attempted = {};
	sirVFSMountDescriptor_t desc = {};
	strcpy( desc.szNamespace, "game" );
	desc.nSourceId = 20;
	desc.nAllowedOps = SIR_VFS_ACCESS_READ;
	desc.eKind = SIR_VFS_BACKEND_MEMORY;
	desc.bTrusted = true;
	sirVFSRequire( mounts.mount( desc, &attempted ) == SIR_VFS_NO_CAPACITY, "generation retained across unmount" );
	sirVFSRequire( files.close( handle ) == SIR_VFS_OK, "close marks retired" );
	sirVFSRequire( files.close( handle ) == SIR_VFS_STALE_HANDLE, "double close" );
	sirVFSRequire( files.borrow( handle, &failed ) == SIR_VFS_STALE_HANDLE, "no borrow after close" );
	sirVFSRequire( files.shutdown() == SIR_VFS_BUSY, "cannot shutdown with live views" );
	sirVFSRequire( files.releaseBorrow( first.borrow ) == SIR_VFS_OK, "release first" );
	sirVFSRequire( files.releaseBorrow( first.borrow ) == SIR_VFS_STALE_HANDLE, "double release rejected" );
	sirVFSRequire( mounts.mount( desc, &attempted ) == SIR_VFS_NO_CAPACITY, "second view pins mount" );
	sirVFSRequire( first.descriptor.nSourceId == 19 && second.descriptor.nSourceRevision == 119, "immutable source snapshots" );
	sirVFSRequire( strcmp( first.path.szPath, "game:/world/rock" ) == 0, "canonical path preserved" );
	sirVFSRequire( files.releaseBorrow( second.borrow ) == SIR_VFS_OK, "release final view" );
	sirVFSRequire( mounts.mount( desc, &attempted ) == SIR_VFS_OK, "mount reused after last borrow" );
	sirVFSRequire( attempted.nGeneration != original.nGeneration, "source generation bumped" );
	sirVFSRequire( files.releaseBorrow( second.borrow ) == SIR_VFS_STALE_HANDLE, "retired ticket rejected" );
	sirVFSRequire( files.borrow( handle, &failed ) == SIR_VFS_STALE_HANDLE, "old file stale" );
	sirVFSRequire( files.shutdown() == SIR_VFS_OK, "clean shutdown" );
	sirVFSRequire( files.shutdown() == SIR_VFS_STOPPED, "shutdown idempotent result" );
	sirVFSFileHandle_t after = {};
	sirVFSRequire( files.openResolved( path, lookup, &after ) == SIR_VFS_STOPPED, "no admission after shutdown" );
	sirVFSRequire( mounts.unmount( attempted ) == SIR_VFS_OK, "fixture cleanup" );
}

static void sirVFSTestRetainAndValidation( void )
{
	SIRVFSMountTable mounts( 2, 2 );
	const sirVFSMountHandle_t game = sirVFSTestMount( mounts, "game", 1 );
	const sirVFSPath_t path = sirVFSTestPath( "game:/asset" );
	const sirVFSMountLease_t lookup = sirVFSTestAcquire( mounts, path );
	sirVFSMountCandidate_t clone = {};
	sirVFSRequire( mounts.retain( lookup, &clone ) == SIR_VFS_OK, "retain validates ticket" );
	sirVFSRequire( clone.lease.nToken != lookup.nToken, "distinct source pin" );
	sirVFSRequire( mounts.retain( lookup, &clone ) == SIR_VFS_NO_CAPACITY, "mount lease capacity" );
	sirVFSRequire( mounts.release( clone.lease ) == SIR_VFS_OK, "release clone" );
	sirVFSRequire( mounts.release( lookup ) == SIR_VFS_OK, "release source" );
	sirVFSRequire( mounts.retain( lookup, &clone ) == SIR_VFS_STALE_HANDLE, "cannot resurrect released lease" );
	sirVFSRequire( mounts.unmount( game ) == SIR_VFS_OK, "release mount" );

	const sirVFSMountHandle_t user = sirVFSTestMount( mounts, "user", 2 );
	const sirVFSPath_t userPath = sirVFSTestPath( "user:/settings" );
	const sirVFSMountLease_t userLease = sirVFSTestAcquire( mounts, userPath );
	SIRVFSFileHandleTable files( mounts, 2, 3 );
	sirVFSFileHandle_t opened = { 88, 88 };
	sirVFSRequire( files.openResolved( path, userLease, &opened ) == SIR_VFS_FORBIDDEN, "cannot attach wrong namespace" );
	sirVFSRequire( opened.nSlot == 88 && opened.nGeneration == 88, "failed open atomic" );
	sirVFSRequire( files.openResolved( path, { 0, 0, 0 }, &opened ) == SIR_VFS_INVALID_ARGUMENT, "reject stale pin" );
	sirVFSPath_t forged = path;
	forged.szPath[0] = 'G';
	sirVFSRequire( files.openResolved( forged, userLease, &opened ) == SIR_VFS_INVALID_ARGUMENT, "reject noncanonical path" );
	sirVFSRequire( files.openResolved( userPath, userLease, &opened ) == SIR_VFS_OK, "valid open" );
	sirVFSRequire( files.close( opened ) == SIR_VFS_OK, "close without borrows releases pin" );
	sirVFSRequire( files.openResolved( userPath, userLease, &opened ) == SIR_VFS_OK, "recycled handle slot" );
	sirVFSRequire( files.borrow( { opened.nSlot, opened.nGeneration - 1 }, nullptr ) == SIR_VFS_INVALID_ARGUMENT, "null view rejected" );
	sirVFSFileView_t dummy = {};
	sirVFSRequire( files.borrow( { opened.nSlot, opened.nGeneration - 1 }, &dummy ) == SIR_VFS_STALE_HANDLE, "old generation denied" );
	sirVFSRequire( files.shutdown() == SIR_VFS_OK, "shutdown releases unborrowed handle" );
	sirVFSRequire( mounts.release( userLease ) == SIR_VFS_OK, "lookup still owned by caller" );
	sirVFSRequire( mounts.unmount( user ) == SIR_VFS_OK, "unmount user" );
}

static void sirVFSTestZeroCapacityAndParallelRelease( void )
{
	SIRVFSMountTable mounts( 1, 8 );
	const sirVFSMountHandle_t original = sirVFSTestMount( mounts, "game", 9 );
	const sirVFSPath_t path = sirVFSTestPath( "game:/parcel" );
	const sirVFSMountLease_t lookup = sirVFSTestAcquire( mounts, path );
	SIRVFSFileHandleTable empty( mounts, 0, 0 );
	sirVFSFileHandle_t handle = {};
	sirVFSRequire( empty.openResolved( path, lookup, &handle ) == SIR_VFS_NO_CAPACITY, "zero file capacity" );
	sirVFSRequire( empty.shutdown() == SIR_VFS_OK, "empty shutdown" );
	SIRVFSFileHandleTable files( mounts, 1, 4 );
	sirVFSRequire( files.openResolved( path, lookup, &handle ) == SIR_VFS_OK, "open concurrent fixture" );
	sirVFSFileView_t views[4] = {};
	for ( uint32_t i = 0; i < 4; ++i ) {
		sirVFSRequire( files.borrow( handle, &views[i] ) == SIR_VFS_OK, "borrow concurrent fixture" );
	}
	sirVFSRequire( files.close( handle ) == SIR_VFS_OK, "close concurrent fixture" );
	sirVFSRequire( mounts.unmount( original ) == SIR_VFS_OK, "unmount while borrows exist" );
	std::thread threads[4];
	for ( uint32_t i = 0; i < 4; ++i ) {
		threads[i] = std::thread(
		    [&files, &views, i]() { sirVFSRequire( files.releaseBorrow( views[i].borrow ) == SIR_VFS_OK, "parallel release" ); } );
	}
	for ( uint32_t i = 0; i < 4; ++i ) {
		threads[i].join();
	}
	sirVFSRequire( mounts.release( lookup ) == SIR_VFS_OK, "lookup released" );
	sirVFSRequire( files.shutdown() == SIR_VFS_OK, "concurrent cleanup" );
}

static void sirVFSTestWriteOnlyLeaseCannotOpenRead( void )
{
	SIRVFSMountTable mounts( 1, 2 );
	sirVFSMountDescriptor_t desc = {};
	strcpy( desc.szNamespace, "user" );
	desc.nSourceId = 61;
	desc.nAllowedOps = SIR_VFS_ACCESS_WRITE;
	desc.eKind = SIR_VFS_BACKEND_MEMORY;
	desc.bTrusted = true;
	sirVFSMountHandle_t mount = {};
	sirVFSRequire( mounts.mount( desc, &mount ) == SIR_VFS_OK, "write-only mount" );
	const sirVFSPath_t path = sirVFSTestPath( "user:/secret" );
	sirVFSMountCandidate_t writeCandidate = {};
	sirVFSRequire( mounts.acquireWriteTarget( path, mount, &writeCandidate ) == SIR_VFS_OK, "write-only lease" );
	SIRVFSFileHandleTable files( mounts, 1, 1 );
	sirVFSFileHandle_t opened = { 7, 7 };
	sirVFSRequire( files.openResolved( path, writeCandidate.lease, &opened ) == SIR_VFS_FORBIDDEN, "deny read on write-only mount" );
	sirVFSRequire( opened.nSlot == 7 && opened.nGeneration == 7, "read-denied output atomic" );
	sirVFSMountCandidate_t check = {};
	sirVFSRequire( mounts.retain( writeCandidate.lease, &check ) == SIR_VFS_OK, "denied open released retained pin" );
	sirVFSRequire( mounts.release( check.lease ) == SIR_VFS_OK, "release extra lease" );
	sirVFSRequire( mounts.release( writeCandidate.lease ) == SIR_VFS_OK, "release write lease" );
	sirVFSRequire( files.shutdown() == SIR_VFS_OK, "write-only table shutdown" );
	sirVFSRequire( mounts.unmount( mount ) == SIR_VFS_OK, "write-only mount cleanup" );
}

int main( void )
{
	sirVFSTestSourcePinAndRetirement();
	sirVFSTestRetainAndValidation();
	sirVFSTestZeroCapacityAndParallelRelease();
	sirVFSTestWriteOnlyLeaseCannotOpenRead();
	return 0;
}
