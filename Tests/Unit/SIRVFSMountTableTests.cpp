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

#include <Core/VFS/SIRVFSMountTable.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sirVFSRequire( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		fprintf( stderr, "VFS mount test failed: %s\n", pszMessage );
		abort();
	}
}

static sirVFSPath_t sirVFSTestPath( const char *pszPath )
{
	sirVFSPath_t path = {};
	sirVFSRequire( SIRVFSPath::normalize( pszPath, ( uint32_t )strlen( pszPath ), &path ) == SIR_VFS_PATH_OK, "normalize setup path" );
	return path;
}

static sirVFSMountDescriptor_t sirVFSTestMount( const char *pszNamespace, uint64_t nSourceId, int32_t iPriority, uint32_t nAllowedOps )
{
	sirVFSMountDescriptor_t descriptor = {};
	strcpy( descriptor.szNamespace, pszNamespace );
	descriptor.nSourceId = nSourceId;
	descriptor.nSourceRevision = 7;
	descriptor.iPriority = iPriority;
	descriptor.nAllowedOps = nAllowedOps;
	descriptor.bTrusted = true;
	descriptor.eKind = SIR_VFS_BACKEND_DIRECTORY;
	return descriptor;
}

static void sirVFSTestOrderingAndRetirement( void )
{
	SIRVFSMountTable table( 3, 8 );
	sirVFSMountHandle_t base = {}, patch = {}, tie = {}, newer = {};
	sirVFSMountDescriptor_t descriptor = sirVFSTestMount( "GAME", 11, 0, SIR_VFS_ACCESS_READ );
	const uint64_t nInitialRevision = table.getRevision();
	sirVFSRequire( table.mount( descriptor, &base ) == SIR_VFS_OK, "base mount" );
	descriptor = sirVFSTestMount( "game", 22, 10, SIR_VFS_ACCESS_READ );
	sirVFSRequire( table.mount( descriptor, &patch ) == SIR_VFS_OK, "patch mount" );
	descriptor.nSourceId = 33;
	sirVFSRequire( table.mount( descriptor, &tie ) == SIR_VFS_OK, "equal-priority mount" );
	sirVFSRequire( table.getRevision() == nInitialRevision + 3, "mount revisions" );
	const sirVFSPath_t path = sirVFSTestPath( "game:/content/mesh" );
	sirVFSCandidateList_t found = {};
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &found ) == SIR_VFS_OK, "read candidates" );
	sirVFSRequire( found.nCount == 3 && found.nRevision == table.getRevision(), "read count and snapshot revision" );
	sirVFSRequire( found.szCandidates[0].descriptor.nSourceId == 22 && found.szCandidates[1].descriptor.nSourceId == 33 &&
	                   found.szCandidates[2].descriptor.nSourceId == 11,
	               "priority descending, oldest tie first" );
	sirVFSRequire( strcmp( found.szCandidates[2].descriptor.szNamespace, "game" ) == 0, "namespace canonicalized" );
	sirVFSRequire( table.unmount( patch ) == SIR_VFS_OK, "retire active leased mount" );
	sirVFSRequire( table.unmount( patch ) == SIR_VFS_STALE_HANDLE, "double unmount rejected" );
	sirVFSRequire( table.getRevision() == nInitialRevision + 4, "retirement publishes revision" );
	descriptor.nSourceId = 44;
	sirVFSRequire( table.mount( descriptor, &newer ) == SIR_VFS_NO_CAPACITY, "leased retired slot not recycled" );
	sirVFSCandidateList_t current = {};
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &current ) == SIR_VFS_OK && current.nCount == 2,
	               "retired mount invisible to new opens" );
	for ( uint32_t i = 0; i < current.nCount; ++i ) {
		sirVFSRequire( table.release( current.szCandidates[i].lease ) == SIR_VFS_OK, "release current candidate" );
	}
	const sirVFSMountLease_t retained = found.szCandidates[0].lease;
	sirVFSRequire( table.release( found.szCandidates[0].lease ) == SIR_VFS_OK, "release retired lease" );
	sirVFSRequire( table.release( retained ) == SIR_VFS_STALE_HANDLE, "double release rejected" );
	sirVFSRequire( table.mount( descriptor, &newer ) == SIR_VFS_OK, "retired slot recyclable after last pin" );
	sirVFSRequire( newer.nSlot == patch.nSlot && newer.nGeneration != patch.nGeneration, "generation increments on reuse" );
	sirVFSRequire( table.unmount( patch ) == SIR_VFS_STALE_HANDLE, "stale generation cannot unmount new mount" );
	sirVFSRequire( table.release( found.szCandidates[1].lease ) == SIR_VFS_OK, "release tied mount" );
	sirVFSRequire( table.release( found.szCandidates[2].lease ) == SIR_VFS_OK, "release base mount" );
	sirVFSRequire( table.unmount( newer ) == SIR_VFS_OK, "new mount cleanup" );
	sirVFSRequire( table.unmount( tie ) == SIR_VFS_OK, "tie cleanup" );
	sirVFSRequire( table.unmount( base ) == SIR_VFS_OK, "base cleanup" );
}

static void sirVFSTestPermissionsAndAtomicity( void )
{
	SIRVFSMountTable table( 4, 3 );
	sirVFSMountHandle_t readOnly = {}, writable = {}, unused = { 999, 999 };
	sirVFSMountDescriptor_t descriptor = sirVFSTestMount( "user", 21, 100, SIR_VFS_ACCESS_READ );
	sirVFSRequire( table.mount( descriptor, &readOnly ) == SIR_VFS_OK, "read overlay" );
	descriptor.nSourceId = 22;
	descriptor.iPriority = 0;
	descriptor.nAllowedOps = SIR_VFS_ACCESS_READ | SIR_VFS_ACCESS_WRITE;
	sirVFSRequire( table.mount( descriptor, &writable ) == SIR_VFS_OK, "explicit write mount" );
	const sirVFSPath_t path = sirVFSTestPath( "user:/save.dat" );
	sirVFSMountCandidate_t result = {};
	result.descriptor.nSourceId = 999;
	sirVFSRequire( table.acquireWriteTarget( path, readOnly, &result ) == SIR_VFS_FORBIDDEN, "no implicit write via read overlay" );
	sirVFSRequire( result.descriptor.nSourceId == 999, "failed write leaves output unchanged" );
	sirVFSRequire( table.acquireWriteTarget( path, writable, &result ) == SIR_VFS_OK, "explicit write route" );
	sirVFSRequire( result.descriptor.nSourceId == 22, "write target identity" );
	sirVFSRequire( table.release( result.lease ) == SIR_VFS_OK, "release write lease" );
	const sirVFSPath_t unrelated = sirVFSTestPath( "game:/save.dat" );
	sirVFSRequire( table.acquireWriteTarget( unrelated, writable, &result ) == SIR_VFS_FORBIDDEN, "namespace isolated" );
	descriptor = sirVFSTestMount( "game", 55, 0, SIR_VFS_ACCESS_WRITE );
	sirVFSRequire( table.mount( descriptor, &unused ) == SIR_VFS_FORBIDDEN, "game writes forbidden" );
	descriptor = sirVFSTestMount( "user", 55, 0, SIR_VFS_ACCESS_WRITE );
	descriptor.eKind = SIR_VFS_BACKEND_SIRPACK;
	sirVFSRequire( table.mount( descriptor, &unused ) == SIR_VFS_FORBIDDEN, "pack writes forbidden" );
	descriptor = sirVFSTestMount( "game", 55, 0, SIR_VFS_ACCESS_READ );
	descriptor.bTrusted = false;
	sirVFSRequire( table.mount( descriptor, &unused ) == SIR_VFS_FORBIDDEN, "untrusted game overlays forbidden" );
	descriptor = sirVFSTestMount( "mods", 55, 0, SIR_VFS_ACCESS_READ );
	descriptor.bTrusted = false;
	sirVFSRequire( table.mount( descriptor, &unused ) == SIR_VFS_OK, "untrusted mods confined" );
	sirVFSRequire( table.unmount( unused ) == SIR_VFS_OK, "untrusted cleanup" );
	descriptor = sirVFSTestMount( "x", 55, 0, SIR_VFS_ACCESS_READ );
	sirVFSRequire( table.mount( descriptor, &unused ) == SIR_VFS_INVALID_ARGUMENT, "invalid namespace" );
	descriptor = sirVFSTestMount( "game", 0, 0, SIR_VFS_ACCESS_READ );
	sirVFSRequire( table.mount( descriptor, &unused ) == SIR_VFS_INVALID_ARGUMENT, "zero source id" );
	sirVFSPath_t forged = path;
	forged.szPath[0] = 'U';
	sirVFSCandidateList_t candidates = {};
	candidates.nCount = 777;
	sirVFSRequire( table.acquireCandidates( forged, SIR_VFS_ACCESS_READ, &candidates ) == SIR_VFS_INVALID_ARGUMENT,
	               "forged noncanonical path" );
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_WRITE, &candidates ) == SIR_VFS_INVALID_ARGUMENT,
	               "write cannot enumerate overlays" );
	sirVFSRequire( candidates.nCount == 777, "failed list does not mutate caller" );
	sirVFSRequire( table.unmount( readOnly ) == SIR_VFS_OK && table.unmount( writable ) == SIR_VFS_OK, "cleanup" );
}

static void sirVFSTestLeaseCapacity( void )
{
	SIRVFSMountTable table( 2, 1 );
	sirVFSMountHandle_t left = {}, right = {};
	sirVFSMountDescriptor_t descriptor = sirVFSTestMount( "game", 1, 0, SIR_VFS_ACCESS_READ );
	sirVFSRequire( table.mount( descriptor, &left ) == SIR_VFS_OK, "first mount" );
	descriptor.nSourceId = 2;
	sirVFSRequire( table.mount( descriptor, &right ) == SIR_VFS_OK, "second mount" );
	sirVFSCandidateList_t result = {};
	result.nCount = 919;
	const sirVFSPath_t path = sirVFSTestPath( "game:/level" );
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &result ) == SIR_VFS_NO_CAPACITY, "all-or-nothing lease capacity" );
	sirVFSRequire( result.nCount == 919, "capacity rejection is atomic" );
	sirVFSRequire( table.unmount( left ) == SIR_VFS_OK, "remove first" );
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &result ) == SIR_VFS_OK && result.nCount == 1,
	               "one mount fits lease pool" );
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &result ) == SIR_VFS_NO_CAPACITY, "second lease rejected" );
	sirVFSRequire( table.release( result.szCandidates[0].lease ) == SIR_VFS_OK, "release one lease" );
	sirVFSRequire( table.unmount( right ) == SIR_VFS_OK, "remove second" );
	sirVFSRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &result ) == SIR_VFS_NOT_FOUND, "empty namespace" );
}

int main( void )
{
	sirVFSTestOrderingAndRetirement();
	sirVFSTestPermissionsAndAtomicity();
	sirVFSTestLeaseCapacity();
	return 0;
}
