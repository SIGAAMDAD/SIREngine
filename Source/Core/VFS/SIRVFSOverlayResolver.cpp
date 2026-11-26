/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
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

SIRVFSResult SIRVFSOverlayResolver::resolveRead( const sirVFSPath_t& path, sirVFSProbeCallback_t pfnProbe, void *pContext,
                                                   sirVFSMountCandidate_t *pWinner ) noexcept
{
	if ( pfnProbe == nullptr || pWinner == nullptr ) {
		return SIRVFSResult::SIR_VFS_INVALID_ARGUMENT;
	}

	// Acquire a generation-pinned mount view under one lock. Backend metadata
	// callbacks run outside that lock, including reentrant mount operations.
	sirVFSCandidateList_t candidates {};
	const SIRVFSResult eAcquire = m_Table.acquireCandidates( path, static_cast<uint32_t>( SIRVFSAccess::SIR_VFS_ACCESS_READ ), &candidates );
	if ( eAcquire != SIRVFSResult::SIR_VFS_OK ) {
		return eAcquire;
	}

	uint32_t nWinner = candidates.nCount;
	SIRVFSResult eResult = SIRVFSResult::SIR_VFS_NOT_FOUND;
	for ( uint32_t i = 0; i < candidates.nCount; ++i ) {
		const sirVFSMountCandidate_t& candidate = candidates.szCandidates[i];
		const SIRVFSProbeResult eProbe = pfnProbe( &candidate.descriptor, &path, pContext );
		if ( eProbe == SIRVFSProbeResult::SIR_VFS_PROBE_MISSING ) {
			continue;
		}
		if ( eProbe == SIRVFSProbeResult::SIR_VFS_PROBE_PRESENT ) {
			nWinner = i;
			eResult = SIRVFSResult::SIR_VFS_OK;
		} else if ( eProbe == SIRVFSProbeResult::SIR_VFS_PROBE_FORBIDDEN ) {
			eResult = SIRVFSResult::SIR_VFS_FORBIDDEN;
		} else {
			eResult = SIRVFSResult::SIR_VFS_IO_ERROR;
		}
		break;
	}

	// Retain exactly one lease, even when the winning mount is retired by a
	// callback before resolution returns. All other leases are released here.
	for ( uint32_t i = 0; i < candidates.nCount; ++i ) {
		if ( i != nWinner ) {
			static_cast<void>(m_Table.release( candidates.szCandidates[i].lease ));
		}
	}
	if ( eResult == SIRVFSResult::SIR_VFS_OK ) {
		*pWinner = candidates.szCandidates[nWinner];
	}
	return eResult;
}
