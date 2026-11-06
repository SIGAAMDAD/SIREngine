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

SIRVFSOverlayResolver::SIRVFSOverlayResolver( SIRVFSMountTable& table ) noexcept : m_Table( table )
{
}

SIRVFSOverlayResolver::~SIRVFSOverlayResolver( void ) noexcept
{
}

sirVFSResult_t SIRVFSOverlayResolver::resolveRead( const sirVFSPath_t& path, sirVFSProbeCallback_t pfnProbe, void *pContext,
                                                   sirVFSMountCandidate_t *pWinner ) noexcept
{
	if ( pfnProbe == nullptr || pWinner == nullptr ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}

	// Acquire a generation-pinned mount view under one lock. Backend metadata
	// callbacks run outside that lock, including reentrant mount operations.
	sirVFSCandidateList_t candidates = {};
	const sirVFSResult_t eAcquire = m_Table.acquireCandidates( path, SIR_VFS_ACCESS_READ, &candidates );
	if ( eAcquire != SIR_VFS_OK ) {
		return eAcquire;
	}

	uint32_t nWinner = candidates.nCount;
	sirVFSResult_t eResult = SIR_VFS_NOT_FOUND;
	for ( uint32_t i = 0; i < candidates.nCount; ++i ) {
		const sirVFSMountCandidate_t& candidate = candidates.szCandidates[i];
		const sirVFSProbeResult_t eProbe = pfnProbe( &candidate.descriptor, &path, pContext );
		if ( eProbe == SIR_VFS_PROBE_MISSING ) {
			continue;
		}
		if ( eProbe == SIR_VFS_PROBE_PRESENT ) {
			nWinner = i;
			eResult = SIR_VFS_OK;
		} else if ( eProbe == SIR_VFS_PROBE_FORBIDDEN ) {
			eResult = SIR_VFS_FORBIDDEN;
		} else {
			eResult = SIR_VFS_IO_ERROR;
		}
		break;
	}

	// Retain exactly one lease, even when the winning mount is retired by a
	// callback before resolution returns. All other leases are released here.
	for ( uint32_t i = 0; i < candidates.nCount; ++i ) {
		if ( i != nWinner ) {
			( void )m_Table.release( candidates.szCandidates[i].lease );
		}
	}
	if ( eResult == SIR_VFS_OK ) {
		*pWinner = candidates.szCandidates[nWinner];
	}
	return eResult;
}
