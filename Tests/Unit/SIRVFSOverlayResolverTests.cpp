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

#include <Core/VFS/SIRVFSOverlayResolver.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sirVFSTestProbeContext_s
{
	SIRVFSMountTable *pTable;
	sirVFSMountHandle_t retiredHandle;
	uint64_t nPresentSource;
	uint64_t nBlockedSource;
	uint64_t nFaultedSource;
	uint32_t nProbeCalls;
	bool bRetireInProbe;
} sirVFSTestProbeContext_t;

static void sirVFSTestRequire( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		fprintf( stderr, "VFS overlay test failed: %s\n", pszMessage );
		abort();
	}
}

static sirVFSPath_t sirVFSTestPath( const char *pszText )
{
	sirVFSPath_t result = {};
	sirVFSTestRequire( SIRVFSPath::normalize( pszText, ( uint32_t )strlen( pszText ), &result ) == SIR_VFS_PATH_OK, "path" );
	return result;
}

static sirVFSMountDescriptor_t sirVFSTestMount( uint64_t nSourceId, int32_t iPriority )
{
	sirVFSMountDescriptor_t result = {};
	strcpy( result.szNamespace, "game" );
	result.nSourceId = nSourceId;
	result.nSourceRevision = 1;
	result.iPriority = iPriority;
	result.nAllowedOps = SIR_VFS_ACCESS_READ;
	result.eKind = SIR_VFS_BACKEND_MEMORY;
	result.bTrusted = true;
	return result;
}

static sirVFSProbeResult_t sirVFSTestProbe( const sirVFSMountDescriptor_t *pDescriptor, const sirVFSPath_t *pPath, void *pContext ) noexcept
{
	sirVFSTestProbeContext_t *pState = ( sirVFSTestProbeContext_t * )pContext;
	++pState->nProbeCalls;
	if ( pState->bRetireInProbe && pDescriptor->nSourceId == pState->nPresentSource ) {
		if ( pState->pTable->unmount( pState->retiredHandle ) != SIR_VFS_OK ) {
			return SIR_VFS_PROBE_ERROR;
		}
	}
	if ( pPath->nLength == 0 ) {
		return SIR_VFS_PROBE_ERROR;
	}
	if ( pDescriptor->nSourceId == pState->nBlockedSource ) {
		return SIR_VFS_PROBE_FORBIDDEN;
	}
	if ( pDescriptor->nSourceId == pState->nFaultedSource ) {
		return SIR_VFS_PROBE_ERROR;
	}
	if ( pDescriptor->nSourceId == pState->nPresentSource ) {
		return SIR_VFS_PROBE_PRESENT;
	}
	return SIR_VFS_PROBE_MISSING;
}

int main( void )
{
	SIRVFSMountTable table( 3, 4 );
	SIRVFSOverlayResolver resolver( table );
	sirVFSMountHandle_t low = {}, high = {}, middle = {};
	sirVFSMountDescriptor_t descriptor = sirVFSTestMount( 10, 0 );
	sirVFSTestRequire( table.mount( descriptor, &low ) == SIR_VFS_OK, "low mount" );
	descriptor = sirVFSTestMount( 20, 10 );
	sirVFSTestRequire( table.mount( descriptor, &high ) == SIR_VFS_OK, "high mount" );
	descriptor = sirVFSTestMount( 30, 5 );
	sirVFSTestRequire( table.mount( descriptor, &middle ) == SIR_VFS_OK, "middle mount" );

	const sirVFSPath_t path = sirVFSTestPath( "game:/asset" );
	sirVFSTestProbeContext_t probe = {};
	probe.nPresentSource = 30;
	sirVFSMountCandidate_t winner = {};
	winner.descriptor.nSourceId = 999;
	sirVFSTestRequire( resolver.resolveRead( path, nullptr, &probe, &winner ) == SIR_VFS_INVALID_ARGUMENT, "null callback" );
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, nullptr ) == SIR_VFS_INVALID_ARGUMENT, "null result" );
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_OK, "overlay resolution" );
	sirVFSTestRequire( winner.descriptor.nSourceId == 30 && probe.nProbeCalls == 2, "priority, absent top fallback" );
	sirVFSTestRequire( table.release( winner.lease ) == SIR_VFS_OK, "winner lease" );
	sirVFSTestRequire( table.release( winner.lease ) == SIR_VFS_STALE_HANDLE, "winner released once" );

	probe.nProbeCalls = 0;
	probe.nBlockedSource = 20;
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_FORBIDDEN, "denied top is terminal" );
	sirVFSTestRequire( probe.nProbeCalls == 1 && winner.descriptor.nSourceId == 30, "denied does not overwrite output" );
	probe.nBlockedSource = 0;
	probe.nFaultedSource = 20;
	probe.nProbeCalls = 0;
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_IO_ERROR, "failed top is terminal" );
	sirVFSTestRequire( probe.nProbeCalls == 1, "failed top does not fall through" );

	probe.nFaultedSource = 0;
	probe.nPresentSource = 0;
	probe.nProbeCalls = 0;
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_NOT_FOUND, "all absent" );
	sirVFSTestRequire( probe.nProbeCalls == 3, "all entries probed" );
	// A leaked probe candidate would make four concurrent leases impossible.
	sirVFSCandidateList_t all = {};
	sirVFSTestRequire( table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &all ) == SIR_VFS_OK && all.nCount == 3,
	                   "no leaked leases after fault/miss" );
	for ( uint32_t i = 0; i < all.nCount; ++i ) {
		sirVFSTestRequire( table.release( all.szCandidates[i].lease ) == SIR_VFS_OK, "release candidate" );
	}

	probe.nPresentSource = 20;
	probe.bRetireInProbe = true;
	probe.pTable = &table;
	probe.retiredHandle = high;
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_OK, "reentrant retirement from callback" );
	sirVFSTestRequire( winner.descriptor.nSourceId == 20, "retired mount is still the pinned winner" );
	sirVFSTestRequire( table.unmount( high ) == SIR_VFS_STALE_HANDLE, "retired handle invisible" );
	sirVFSTestRequire( table.release( winner.lease ) == SIR_VFS_OK, "pinned retired source released" );
	probe.bRetireInProbe = false;
	probe.nPresentSource = 10;
	sirVFSTestRequire( resolver.resolveRead( path, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_OK && winner.descriptor.nSourceId == 10,
	                   "next lookup skips retired mount" );
	sirVFSTestRequire( table.release( winner.lease ) == SIR_VFS_OK, "last winner release" );

	const sirVFSPath_t invalid = sirVFSTestPath( "mods:/asset" );
	sirVFSTestRequire( resolver.resolveRead( invalid, sirVFSTestProbe, &probe, &winner ) == SIR_VFS_NOT_FOUND, "namespaces are isolated" );
	sirVFSTestRequire( table.unmount( low ) == SIR_VFS_OK, "low unmount" );
	sirVFSTestRequire( table.unmount( middle ) == SIR_VFS_OK, "middle unmount" );
	return 0;
}
